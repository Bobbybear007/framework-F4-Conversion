#include "ConflictChecker.h"

#include <Psapi.h>
#include <dxgi.h>
#include <windows.h>
#include <algorithm>
#include <mutex>
#include <string>
#include <vector>

#pragma comment(lib, "Psapi.lib")
#pragma intrinsic(_ReturnAddress)

namespace PrismaUI::ConflictChecker {

    // ──────────────────────────────────────────────────────────────
    // Internal helpers
    // ──────────────────────────────────────────────────────────────

    static std::wstring GetModuleBasenameW(HMODULE hMod) {
        wchar_t path[MAX_PATH] = {};
        GetModuleFileNameW(hMod, path, MAX_PATH);
        std::wstring full(path);
        auto pos = full.rfind(L'\\');
        return (pos != std::wstring::npos) ? full.substr(pos + 1) : full;
    }

    static std::string WideToUtf8(const std::wstring& ws) {
        if (ws.empty()) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0) return {};
        std::string out(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), len, nullptr, nullptr);
        return out;
    }

    static std::string WideToUtf8(const wchar_t* ws) {
        return ws ? WideToUtf8(std::wstring(ws)) : std::string{};
    }

    // Identifies which module owns a given address. Returns basename or "<unknown>".
    static std::string OwnerOf(void* address) {
        HMODULE hOwner = nullptr;
        if (GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(address),
                &hOwner) && hOwner) {
            return WideToUtf8(GetModuleBasenameW(hOwner));
        }
        return "<unknown>";
    }

    // ──────────────────────────────────────────────────────────────
    // Check C — Duplicate module + export squatting
    // ──────────────────────────────────────────────────────────────

    void CheckEarly() {
        logger::info("[ConflictChecker] CheckEarly: scanning loaded modules");

        // C1: Count instances of PrismaUI_F4.dll
        HANDLE hProcess = GetCurrentProcess();
        HMODULE modules[1024] = {};
        DWORD cbNeeded = 0;

        if (!EnumProcessModules(hProcess, modules, sizeof(modules), &cbNeeded)) {
            logger::warn("[ConflictChecker] CheckEarly: EnumProcessModules failed (error {})", GetLastError());
        } else {
            const DWORD count = cbNeeded / sizeof(HMODULE);
            std::vector<std::wstring> matches;

            for (DWORD i = 0; i < count; ++i) {
                std::wstring basename = GetModuleBasenameW(modules[i]);
                std::wstring lower = basename;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
                if (lower == L"prismaui_f4.dll") {
                    wchar_t path[MAX_PATH] = {};
                    GetModuleFileNameW(modules[i], path, MAX_PATH);
                    matches.push_back(path);
                }
            }

            if (matches.size() == 1) {
                logger::info("[ConflictChecker] Module check OK: single instance at '{}'",
                             WideToUtf8(matches[0]));
            } else if (matches.size() > 1) {
                logger::critical("[ConflictChecker] DUPLICATE: PrismaUI_F4.dll loaded {} times:",
                                 matches.size());
                for (const auto& p : matches) {
                    logger::critical("[ConflictChecker]   -> '{}'", WideToUtf8(p));
                }
            } else {
                logger::warn("[ConflictChecker] CheckEarly: PrismaUI_F4.dll not found in module list (unexpected)");
            }
        }

        // C2: Export squatting — verify RequestPluginAPI is owned by us
        HMODULE hOurs = GetModuleHandleW(L"PrismaUI_F4.dll");
        if (hOurs) {
            void* exportFn = reinterpret_cast<void*>(GetProcAddress(hOurs, "RequestPluginAPI"));
            if (exportFn) {
                HMODULE hOwner = nullptr;
                GetModuleHandleExW(
                    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                    reinterpret_cast<LPCWSTR>(exportFn), &hOwner);

                if (hOwner && hOwner != hOurs) {
                    logger::critical(
                        "[ConflictChecker] CONFLICT: RequestPluginAPI export resolves to '{}' instead of our module"
                        " — another DLL is squatting on our name",
                        WideToUtf8(GetModuleBasenameW(hOwner)));
                } else {
                    logger::info("[ConflictChecker] Export check OK: RequestPluginAPI owned by PrismaUI_F4.dll");
                }
            } else {
                logger::warn("[ConflictChecker] CheckEarly: GetProcAddress(RequestPluginAPI) returned null");
            }
        } else {
            logger::warn("[ConflictChecker] CheckEarly: GetModuleHandleW(PrismaUI_F4.dll) returned null");
        }
    }

    // ──────────────────────────────────────────────────────────────
    // Check B — D3D hook conflicts
    // ──────────────────────────────────────────────────────────────

    static const wchar_t* kKnownConflictDlls[] = {
        L"ReShade.dll",
        L"ReShade64.dll",
        L"FallSouls.dll",
        L"FallSouls_NG.dll",
        L"enbseries.dll",
        L"dxvk.dll",
        L"d3d11_log.dll",
    };

    // Shared state for Check A (populated by OnAPIRequest, read by CheckPreHooks)
    struct APIConsumer { std::string name; PRISMA_UI_API::InterfaceVersion version; };
    static std::vector<APIConsumer> s_apiConsumers;
    static std::mutex s_apiConsumersMutex;

    static void PrintAPIConsumerSummary() {
        std::lock_guard lock(s_apiConsumersMutex);
        if (s_apiConsumers.empty()) {
            logger::warn("[ConflictChecker] No Prisma plugins connected via RequestPluginAPI by kGameDataReady");
            return;
        }
        std::string summary;
        for (size_t i = 0; i < s_apiConsumers.size(); ++i) {
            if (i > 0) summary += ", ";
            summary += s_apiConsumers[i].name;
            summary += " (V";
            summary += std::to_string(static_cast<int>(s_apiConsumers[i].version));
            summary += ")";
        }
        logger::info("[ConflictChecker] Prisma API consumers ({}): {}", s_apiConsumers.size(), summary);
    }

    void CheckPreHooks() {
        logger::info("[ConflictChecker] CheckPreHooks: scanning for D3D hook conflicts");

        // B1: Known conflicting DLL scan
        bool anyKnownConflict = false;
        for (const auto* dllName : kKnownConflictDlls) {
            if (GetModuleHandleW(dllName)) {
                logger::warn("[ConflictChecker] WARNING: '{}' is loaded — may conflict with D3D Present hook",
                             WideToUtf8(dllName));
                anyKnownConflict = true;
            }
        }
        if (!anyKnownConflict) {
            logger::info("[ConflictChecker] Known-DLL scan OK: no known conflicting modules detected");
        }

        // B2: Vtable integrity check
        auto* rendererData = RE::BSGraphics::RendererData::GetSingleton();
        if (!rendererData || !rendererData->renderWindow[0].swapChain) {
            logger::warn("[ConflictChecker] CheckPreHooks: cannot get IDXGISwapChain for vtable check"
                         " — skipping vtable integrity check");
        } else {
            IDXGISwapChain* swapChain = rendererData->renderWindow[0].swapChain;
            void** vtable = *reinterpret_cast<void***>(swapChain);

            // Determine dxgi.dll address range
            uintptr_t dxgiBase = 0, dxgiEnd = 0;
            HMODULE hDxgi = GetModuleHandleW(L"dxgi.dll");
            if (hDxgi) {
                MODULEINFO mi = {};
                if (GetModuleInformation(GetCurrentProcess(), hDxgi, &mi, sizeof(mi))) {
                    dxgiBase = reinterpret_cast<uintptr_t>(mi.lpBaseOfDll);
                    dxgiEnd  = dxgiBase + mi.SizeOfImage;
                }
            }

            auto checkVtableEntry = [&](int idx, const char* name) {
                uintptr_t addr = reinterpret_cast<uintptr_t>(vtable[idx]);
                const bool inDxgi = (dxgiBase != 0 && addr >= dxgiBase && addr < dxgiEnd);
                if (inDxgi) {
                    logger::info("[ConflictChecker] vtable[{}] ({}) OK — points inside dxgi.dll", idx, name);
                } else {
                    const std::string owner = OwnerOf(vtable[idx]);
                    logger::error(
                        "[ConflictChecker] CONFLICT: vtable[{}] ({}) already hooked by '{}' at 0x{:016X}"
                        " — rendering may be unstable",
                        idx, name, owner, addr);
                }
            };

            checkVtableEntry(8,  "Present");
            checkVtableEntry(13, "ResizeBuffers");
        }

        // Print API consumer summary (populated by OnAPIRequest calls before kGameDataReady)
        PrintAPIConsumerSummary();
    }

    // ──────────────────────────────────────────────────────────────
    // Check A — API caller & version tracking
    // ──────────────────────────────────────────────────────────────

    void OnAPIRequest(void* returnAddress, PRISMA_UI_API::InterfaceVersion version) {
        // Identify calling module from its return address
        std::string callerName = "<unknown>";
        HMODULE hCaller = nullptr;
        if (GetModuleHandleExW(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCWSTR>(returnAddress),
                &hCaller) && hCaller) {
            callerName = WideToUtf8(GetModuleBasenameW(hCaller));
        }

        const int versionNum = static_cast<int>(version);
        logger::info("[ConflictChecker] API request: caller='{}' requested V{}", callerName, versionNum);

        // Determine if this version is actually supported
        const bool supported =
            version == PRISMA_UI_API::InterfaceVersion::V1 ||
            version == PRISMA_UI_API::InterfaceVersion::V2 ||
            version == PRISMA_UI_API::InterfaceVersion::V3;

        if (!supported) {
            logger::error(
                "[ConflictChecker] CONFLICT: '{}' requested unsupported interface version V{}"
                " — plugin likely built against a different PrismaUI_F4",
                callerName, versionNum);
        }

        // Record for the summary printed in CheckPreHooks (deduplicate by name)
        {
            std::lock_guard lock(s_apiConsumersMutex);
            for (const auto& c : s_apiConsumers) {
                if (c.name == callerName) return;
            }
            s_apiConsumers.push_back({callerName, version});
        }
    }

}  // namespace PrismaUI::ConflictChecker
