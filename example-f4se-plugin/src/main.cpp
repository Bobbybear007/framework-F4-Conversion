// example-f4se-plugin/src/main.cpp
//
// Demonstrates PrismaUI_F4 integration:
//   - Acquires IVPrismaUI3 on kGameDataReady.
//   - Creates a view on kPostLoadGame / kNewGame.
//   - Registers JS listeners inside OnDomReady.
//   - F3 key toggles show/focus.
//   - C++ -> JS via Invoke() and InteropCall().
//   - JS -> C++ via RegisterJSListener().
#include "PCH.h"
#include "PrismaUI_F4_API.h"
#include "keyhandler/keyhandler.h"

static PRISMA_UI_API::IVPrismaUI3* g_api  = nullptr;
static PrismaView                   g_view = 0;
static bool                         g_visible = false;

static void OnDomReady(PrismaView v)
{
    // Safe: DOM is ready — register listeners and push initial data.
    // JSListenerCallback fires on the Ultralight render thread.
    // Any RE:: access must be dispatched via F4SE::GetTaskInterface()->AddTask.

    g_api->RegisterConsoleCallback(v, [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel lvl, const char* msg) {
        const char* tag = lvl == PRISMA_UI_API::ConsoleMessageLevel::Error   ? "[JS ERR] " :
                          lvl == PRISMA_UI_API::ConsoleMessageLevel::Warning ? "[JS WARN]" :
                                                                               "[JS LOG] ";
        logger::info("{} {}", tag, msg);
    });

    g_api->RegisterJSListener(v, "requestClose", [](const char*) {
        g_visible = false;
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    });

    // Example: JS calls sendDataToF4SE("some string") -> dispatched to game thread
    g_api->RegisterJSListener(v, "sendDataToF4SE", [](const char* data) {
        std::string d = data ? data : "";
        F4SE::GetTaskInterface()->AddTask([d]() {
            logger::info("Received from JS: {}", d);
            // RE:: access safe here
        });
    });

    g_api->Invoke(v, "init()");
    logger::info("View DOM ready: {}", v);
}

static void CreateViews()
{
    if (!g_api || (g_view != 0 && g_api->IsValid(g_view))) return;

    g_view = g_api->CreateView("PrismaUI-F4-Example/index.html", OnDomReady);
    // Views start hidden — no Hide() needed, but explicit is fine:
    // g_api->Hide(g_view);
}

static void Toggle()
{
    if (!g_api || !g_api->IsValid(g_view)) return;
    g_visible = !g_visible;
    if (g_visible) {
        g_api->Show(g_view);
        g_api->Focus(g_view, /*pauseGame=*/false);
        g_api->Invoke(g_view, "updateFocusLabel('Focused. Press F3 to close.')");
    } else {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    }
}

static void F4SEMessageHandler(F4SE::MessagingInterface::Message* message)
{
    switch (message->type) {

    case F4SE::MessagingInterface::kGameDataReady:
    {
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI3>();
        if (!g_api) {
            logger::error("PrismaUI_F4 API not found — is PrismaUI_F4.dll installed?");
            return;
        }

        KeyHandler::RegisterSink();
        // F4 uses Windows VK codes — RE::BS_BUTTON_CODE::kF3 == 0x72 (VK_F3)
        KeyHandler::GetSingleton()->Register(
            static_cast<uint32_t>(RE::BS_BUTTON_CODE::kF3), KeyEventType::KEY_DOWN, Toggle);
        break;
    }

    case F4SE::MessagingInterface::kPostLoadGame:
    case F4SE::MessagingInterface::kNewGame:
        if (g_api) CreateViews();
        break;

    default:
        break;
    }
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
    auto path = logger::log_directory();
    if (!path) return false;
    *path /= "PrismaUI-F4-Example-Plugin.log";
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
    auto log  = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
    log->set_level(spdlog::level::info);
    spdlog::set_default_logger(std::move(log));

    a_info->infoVersion = F4SE::PluginInfo::kVersion;
    a_info->name        = "PrismaUI-F4-Example-Plugin";
    a_info->version     = 1;

    if (a_f4se->IsEditor()) return false;
    if (a_f4se->RuntimeVersion() < F4SE::RUNTIME_1_10_162) return false;

    return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
    F4SE::Init(a_f4se);
    F4SE::GetMessagingInterface()->RegisterListener(F4SEMessageHandler);
    return true;
}
