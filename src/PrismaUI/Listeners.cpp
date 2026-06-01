#include "Listeners.h"

#include "Communication.h"
#include "Core.h"
#include "NetworkSandbox.h"
#include "PrismaUI_F4_API.h"
#include "Translations.h"

namespace PrismaUI::Listeners {
    using namespace Core;
    using namespace Communication;

    // MyLoadListener
    MyLoadListener::MyLoadListener(Core::PrismaViewId id) : viewId_(std::move(id)) {}

    MyLoadListener::~MyLoadListener() = default;

    void MyLoadListener::OnBeginLoading(View* /*caller*/, uint64_t /*frame_id*/, bool /*is_main_frame*/,
                                        const String& url) {
        logger::info("View [{}]: LoadListener: Begin loading URL: {}", viewId_, url.utf8().data());
    }

    void MyLoadListener::OnFinishLoading(View* /*caller*/, uint64_t /*frame_id*/, bool /*is_main_frame*/,
                                         const String& url) {
        logger::info("View [{}]: LoadListener: Finished loading URL: {}", viewId_, url.utf8().data());
        ultralightThread.submit([id = viewId_, urlStr = std::string(url.utf8().data())] {
            std::shared_lock lock(viewsMutex);
            auto it = views.find(id);
            if (it != views.end()) {
                it->second->isLoadingFinished = true;
                it->second->lastLoadedUrl = urlStr;
                it->second->recoveryAttempts = 0;
                Communication::BindJSCallbacks(id);
            }
        });
    }

    void MyLoadListener::OnFailLoading(View* /*caller*/, uint64_t /*frame_id*/, bool /*is_main_frame*/,
                                       const String& url, const String& description, const String& /*error_domain*/,
                                       int /*error_code*/) {
        logger::error("View [{}]: LoadListener: Failed loading URL: {}. Error: {}", viewId_, url.utf8().data(),
                      description.utf8().data());
        ultralightThread.submit([id = viewId_] {
            std::shared_lock lock(viewsMutex);
            auto it = views.find(id);
            if (it != views.end()) {
                it->second->isLoadingFinished = false;
            }
        });
    }

    void MyLoadListener::OnWindowObjectReady(View* caller, uint64_t /*frame_id*/, bool is_main_frame,
                                             const String& /*url*/) {
        if (!is_main_frame) return;
        logger::info("View [{}]: LoadListener: Window object ready.", viewId_);

        // ── Network sandbox (Layer 1: JS kill + Layer 2: CSP meta) ─────────────────
        // Runs BEFORE page scripts. configurable:false descriptors cannot be reversed.
        {
            ultralight::String exc;
            caller->EvaluateScript(ultralight::String(NetworkSandbox::kNetworkBlockScript), &exc);
            if (!exc.empty())
                logger::warn("View [{}]: Network sandbox injection failed: {}", viewId_, exc.utf8().data());
            else
                logger::debug("View [{}]: Network sandbox injected.", viewId_);
        }

        std::shared_lock lock(viewsMutex);
        auto it = views.find(viewId_);
        if (it == views.end() || !it->second || it->second->translationsPluginName.empty()) return;

        std::string pluginName = it->second->translationsPluginName;
        lock.unlock();

        auto lang = Translations::DetectGameLanguage();
        auto map  = Translations::ParseTranslationFile(pluginName, lang);
        auto script = Translations::BuildL10NScript(map);
        if (script.empty()) return;

        ultralight::String ulScript(script.c_str());
        caller->EvaluateScript(ulScript);
        logger::info("View [{}]: Injected L10N for '{}' ({} keys, lang={})", viewId_, pluginName, map.size(), lang);
    }

    void MyLoadListener::OnDOMReady(View* /*caller*/, uint64_t /*frame_id*/, bool is_main_frame,
                                    const String& /*url*/) {
        if (is_main_frame) {
            logger::info("View [{}]: LoadListener: DOM ready.", viewId_);

            ultralightThread.submit([id = viewId_] {
                std::shared_lock lock(viewsMutex);
                auto it = views.find(id);
                if (it != views.end() && it->second->domReadyCallback) {
                    it->second->domReadyCallback(id);
                }
            });
        }
    }

    // MyViewListener
    MyViewListener::MyViewListener(Core::PrismaViewId id) : viewId_(std::move(id)) {}

    MyViewListener::~MyViewListener() = default;

    // ── Layer 3: Block child view creation (window.open, target=_blank) ──────────
    RefPtr<View> MyViewListener::OnCreateChildView(View* /*caller*/, const String& /*opener_url*/,
                                                    const String& target_url, bool /*is_popup*/,
                                                    const IntRect& /*popup_rect*/) {
        logger::warn("[PrismaUI Security] Blocked child view navigation to '{}'",
                     target_url.utf8().data());
        return nullptr;
    }

    void MyViewListener::OnAddConsoleMessage(ultralight::View* /*caller*/,
                                              const ultralight::ConsoleMessage& message) {
        // ── Layer 4: Log network-source messages ───────────────────────────────────
        // kMessageSource_Network errors indicate blocked resource loads or CSP violations.
        if (message.source() == kMessageSource_Network) {
            logger::warn("[PrismaUI Security] View [{}]: [Network] {}", viewId_, message.message().utf8().data());
        }
        std::shared_lock lock(viewsMutex);
        auto it = views.find(viewId_);
        if (it != views.end() && it->second && it->second->consoleMessageCallback) {
            PRISMA_UI_API::ConsoleMessageLevel prismaLevel = PRISMA_UI_API::ConsoleMessageLevel::Log;
            switch (message.level()) {
                case kMessageLevel_Warning: prismaLevel = PRISMA_UI_API::ConsoleMessageLevel::Warning; break;
                case kMessageLevel_Error:   prismaLevel = PRISMA_UI_API::ConsoleMessageLevel::Error; break;
                case kMessageLevel_Debug:   prismaLevel = PRISMA_UI_API::ConsoleMessageLevel::Debug; break;
                case kMessageLevel_Info:    prismaLevel = PRISMA_UI_API::ConsoleMessageLevel::Info; break;
                default: break;
            }
            auto msg = std::string(message.message().utf8().data());
            auto cb  = it->second->consoleMessageCallback;
            auto id  = viewId_;
            lock.unlock();
            cb(id, prismaLevel, msg);
        }
    }

    RefPtr<View> MyViewListener::OnCreateInspectorView(View* /*caller*/, bool is_local, const String& inspectedURL) {
        logger::info(
            "View [{}]: ViewListener: OnCreateInspectorView called (is_local={}, "
            "URL={})",
            viewId_, is_local, inspectedURL.utf8().data());

        RefPtr<View> inspectorView = nullptr;

        std::unique_lock lock(viewsMutex);
        auto it = views.find(viewId_);
        if (it != views.end() && it->second) {
            auto viewData = it->second;

            if (!viewData->inspectorView && viewData->ultralightView && renderer) {
                uint32_t width = viewData->inspectorDisplayWidth > 0 ? viewData->inspectorDisplayWidth : 800;
                uint32_t height = viewData->inspectorDisplayHeight > 0 ? viewData->inspectorDisplayHeight : 600;

                ViewConfig config;
                config.is_accelerated = false;
                config.is_transparent = true;

                viewData->inspectorView = renderer->CreateView(width, height, config, nullptr);
                inspectorView = viewData->inspectorView;

                logger::info("View [{}]: Inspector view created with size {}x{}", viewId_, width, height);
            } else if (viewData->inspectorView) {
                inspectorView = viewData->inspectorView;
                logger::info("View [{}]: Returning existing inspector view", viewId_);
            }
        }

        return inspectorView;
    }

    // MyUltralightLogger
    MyUltralightLogger::~MyUltralightLogger() = default;

    void MyUltralightLogger::LogMessage(LogLevel /*log_level*/, const String& /*message*/) {
        // Implementation was empty, so keep it empty.
    }
}  // namespace PrismaUI::Listeners
