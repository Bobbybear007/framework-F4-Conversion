#pragma once

#include <cstdint>
#include <functional>

#pragma warning(push)
#pragma warning(disable : 4100)
#include <AppCore/Platform.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <Ultralight/String.h>
#include <Ultralight/Ultralight.h>
#include <Ultralight/View.h>
#pragma warning(pop)

namespace PRISMA_UI_API {
    enum class ConsoleMessageLevel : uint8_t;
}

namespace PrismaUI::Core {
    typedef uint64_t PrismaViewId;
}

namespace PrismaUI::ViewManager {
    using namespace ultralight;

    Core::PrismaViewId Create(const std::string& htmlPath,
                              std::function<void(Core::PrismaViewId)> onDomReadyCallback = nullptr);
    void Show(const Core::PrismaViewId& viewId);
    void Hide(const Core::PrismaViewId& viewId);
    bool IsHidden(const Core::PrismaViewId& viewId);
    bool Focus(const Core::PrismaViewId& viewId, bool pauseGame = false, bool disableFocusMenu = false);
    void Unfocus(const Core::PrismaViewId& viewId);
    bool HasFocus(const Core::PrismaViewId& viewId);
    bool ViewHasInputFocus(const Core::PrismaViewId& viewId);
    void Destroy(const Core::PrismaViewId& viewId);
    bool IsValid(const Core::PrismaViewId& viewId);
    void SetScrollingPixelSize(const Core::PrismaViewId& viewId, int pixelSize);
    int GetScrollingPixelSize(const Core::PrismaViewId& viewId);
    void SetOrder(const Core::PrismaViewId& viewId, int order);
    int GetOrder(const Core::PrismaViewId& viewId);

    // Inspector View functions
    void CreateInspectorView(const Core::PrismaViewId& viewId);
    void SetInspectorVisibility(const Core::PrismaViewId& viewId, bool visible);
    bool IsInspectorVisible(const Core::PrismaViewId& viewId);
    void SetInspectorBounds(const Core::PrismaViewId& viewId, float topLeftX, float topLeftY, uint32_t width,
                            uint32_t height);
    bool HasAnyActiveFocus();

    // Console message callback registration
    void RegisterConsoleCallback(const Core::PrismaViewId& viewId,
                                 std::function<void(Core::PrismaViewId, PRISMA_UI_API::ConsoleMessageLevel, const std::string&)> callback);

    // Translations — inject window.L10N / window.t before page scripts run.
    // pluginName is the bare plugin name matching the translation filename, e.g. "MyPlugin_F4".
    void RegisterTranslations(const Core::PrismaViewId& viewId, const std::string& pluginName);

    // Enumerate all currently-registered views. Callback receives (id, htmlPath) for each view,
    // where htmlPath is the original relative path passed to Create (e.g., "debug_panel.html").
    // Snapshot is taken under the shared lock; callback is invoked outside the lock.
    void EnumerateViews(std::function<void(Core::PrismaViewId, const std::string&)> callback);
}
