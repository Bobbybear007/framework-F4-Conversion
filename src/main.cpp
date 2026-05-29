#include "API/API.h"
#include "Hooks/Hooks.h"
#include "Utils/DllLoader.h"

#include <spdlog/sinks/basic_file_sink.h>

static void F4SEMessageHandler(F4SE::MessagingInterface::Message* message)
{
    if (message->type == F4SE::MessagingInterface::kGameDataReady) {
        Hooks::D3DHooks::Install();
    }
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
    auto path = logger::log_directory();
    if (!path) { return false; }
    *path /= "PrismaUI_F4.log";
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
    auto log  = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);
    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%Y-%m-%d %T.%e] [%l] [%t] [%s:%#] %v");

    logger::info("PrismaUI_F4 v{}.{}.{}", 1, 0, 0);

    a_info->infoVersion = F4SE::PluginInfo::kVersion;
    a_info->name        = "PrismaUI_F4";
    a_info->version     = 1;

    if (a_f4se->IsEditor()) {
        logger::critical("Loaded in editor — aborting.");
        return false;
    }

    const auto ver = a_f4se->RuntimeVersion();
    if (ver < F4SE::RUNTIME_1_10_162) {
        logger::critical("Unsupported runtime v{}", ver.string());
        return false;
    }

    return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
    F4SE::Init(a_f4se);

    if (!PrismaUI::Utils::DllLoader::GetSingleton().LoadUltralightLibraries()) {
        logger::critical("Failed to load Ultralight libraries from Data/PrismaUI_F4/libs/!");
        return false;
    }

    F4SE::AllocTrampoline(1 << 10);

    const auto* messaging = F4SE::GetMessagingInterface();
    if (!messaging) {
        logger::critical("Failed to get F4SE messaging interface!");
        return false;
    }
    messaging->RegisterListener(F4SEMessageHandler);

    return true;
}

extern "C" DLLEXPORT void* F4SEAPI RequestPluginAPI(const PRISMA_UI_API::InterfaceVersion a_interfaceVersion)
{
    auto api = PluginAPI::PrismaUIInterface::GetSingleton();

    switch (a_interfaceVersion) {
    case PRISMA_UI_API::InterfaceVersion::V1:
        logger::info("RequestPluginAPI returned V1 interface");
        return static_cast<PRISMA_UI_API::IVPrismaUI1*>(api);
    case PRISMA_UI_API::InterfaceVersion::V2:
        logger::info("RequestPluginAPI returned V2 interface");
        return static_cast<PRISMA_UI_API::IVPrismaUI2*>(api);
    default:
        logger::info("RequestPluginAPI: unsupported interface version");
        return nullptr;
    }
}
