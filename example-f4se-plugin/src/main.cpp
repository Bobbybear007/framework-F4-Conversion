// example-f4se-plugin/src/main.cpp
//
// Demonstrates PrismaUI_F4 integration:
//   - Creates an HTML view when game data is ready.
//   - F3 key toggles keyboard focus on/off.
//   - C++ -> JS via Invoke().
//   - JS -> C++ via RegisterJSListener().
#include "PCH.h"
#include "PrismaUI_F4_API.h"
#include "keyhandler/keyhandler.h"

PRISMA_UI_API::IVPrismaUI1* PrismaUI = nullptr;
static PrismaView view = 0;

static void F4SEMessageHandler(F4SE::MessagingInterface::Message* message)
{
    switch (message->type) {

    case F4SE::MessagingInterface::kGameDataReady:
    {
        // 1. Acquire PrismaUI_F4 API at runtime (no link dependency on PrismaUI_F4.dll).
        PrismaUI = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI1>();
        if (!PrismaUI) {
            logger::error("PrismaUI_F4 API not found — is PrismaUI_F4.dll installed?");
            return;
        }

        // 2. Create an HTML view; the lambda fires once the DOM is ready.
        view = PrismaUI->CreateView("PrismaUI-F4-Example/index.html", [](PrismaView v) {
            logger::info("View DOM is ready: {}", v);
        });
        PrismaUI->Hide(view); // views start visible — hide until the player toggles it open

        // 3. Register a JS -> C++ listener.
        //    In JS: window.sendDataToF4SE("some data")
        PrismaUI->RegisterJSListener(view, "sendDataToF4SE", [](const char* data) {
            logger::info("Received from JS: {}", data);
        });

        // 4. Register F3 key (BS_BUTTON_CODE::kF3 == 0x72, Windows VK_F3) to show/hide the view.
        //    F4 uses Windows VK codes for keyboard, not DirectInput scan codes.
        KeyHandler::RegisterSink();
        {
            auto* keyHandler = KeyHandler::GetSingleton();
            // kF3 = 0x72 (defined in RE::BS_BUTTON_CODE, matches Windows VK_F3)
            const uint32_t F3_VK = static_cast<uint32_t>(RE::BS_BUTTON_CODE::kF3);

            keyHandler->Register(F3_VK, KeyEventType::KEY_DOWN, []() {
                if (!PrismaUI || !view) return;

                if (PrismaUI->IsHidden(view)) {
                    PrismaUI->Show(view);
                    PrismaUI->Focus(view, /*pauseGame=*/false);
                    PrismaUI->Invoke(view, "updateFocusLabel('Focused. Press F3 to close.')");
                } else {
                    PrismaUI->Unfocus(view);
                    PrismaUI->Hide(view);
                }
            });
        }
        break;
    }

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
