# API Reference — PrismaUI_F4

## Overview

The public API is declared entirely in `PrismaUI_F4_API.h`. Copy that single header into your plugin's `src/` folder. You do not link against PrismaUI_F4 at compile time; the connection is made at runtime via `GetProcAddress`.

```cpp
#include "PrismaUI_F4_API.h"

// On kGameDataReady:
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI3>();
```

---

## Types

### `PrismaView`

```cpp
typedef uint64_t PrismaView;
```

An opaque handle that identifies one HTML view. The value `0` means "no view" / invalid. Always check `IsValid(view)` before using a handle you haven't used recently, particularly after a game reload.

---

### `ConsoleMessageLevel`

```cpp
enum class ConsoleMessageLevel : uint8_t {
    Log = 0,
    Warning,
    Error,
    Debug,
    Info
};
```

Passed to `ConsoleMessageCallback`. Maps directly to the JavaScript `console.*` level.

---

### Callback Types

```cpp
// Called once when the HTML document's DOM is fully parsed and ready.
typedef void (*OnDomReadyCallback)(PrismaView view);

// Called with the string result of a JS expression evaluated via Invoke().
typedef void (*JSCallback)(const char* result);

// Called when JS code calls the registered listener function on window.
typedef void (*JSListenerCallback)(const char* argument);

// Called for every console.log/warn/error line from JS.
typedef void (*ConsoleMessageCallback)(
    PrismaView view,
    ConsoleMessageLevel level,
    const char* message
);
```

**Threading:** `OnDomReadyCallback` is invoked on the main game thread (dispatched via `F4SE::GetTaskInterface()->AddTask`). `JSListenerCallback` fires on the **Ultralight render thread** — see [Threading Warning](#threading-warning-js-listener-callbacks) below.

---

## Interface Versions

| Type | Version | Adds |
|------|---------|------|
| `IVPrismaUI1` | V1 | All core view operations |
| `IVPrismaUI2` | V2 | `RegisterConsoleCallback` |
| `IVPrismaUI3` | V3 | `RegisterTranslations` |

Always request the highest version you need. If the installed PrismaUI_F4 is older than your requested version, `RequestPluginAPI` returns `nullptr` — handle this gracefully. V3 is the current recommended version.

```cpp
// Recommended — request V3
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI3>();
if (!api) {
    logger::error("PrismaUI V3 not available — update PrismaUI_F4");
    return;
}

// V1/V2 still supported for backward compatibility
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI1>();
```

---

## `RequestPluginAPI`

```cpp
template <typename T>
[[nodiscard]] inline T* RequestPluginAPI();
```

Locates `PrismaUI_F4.dll` in the current process via `GetModuleHandleW`, calls its exported `RequestPluginAPI` function, and casts the result. Returns `nullptr` if:
- PrismaUI_F4 is not loaded
- The loaded version does not support the requested interface

**Call timing:** During or after `F4SE::MessagingInterface::kGameDataReady`. Do not call during `F4SEPlugin_Load` or `F4SEPlugin_Query`; F4SE may not have loaded PrismaUI_F4 yet.

---

## Threading Warning: JS Listener Callbacks

`JSListenerCallback` functions registered via `RegisterJSListener` fire on the **Ultralight render thread**, not the game thread.

**You must not access CommonLibF4 game objects (`RE::*`) directly from a JS listener callback.** Doing so causes undefined behavior — `GetSingleton()` calls may return null or corrupt data, and crashes will appear non-deterministic.

**Always dispatch game thread work via `AddTask`:**

```cpp
api->RegisterJSListener(view, "requestGameData", [](const char* s) {
    // WRONG — RE:: access on Ultralight thread:
    // auto* player = RE::PlayerCharacter::GetSingleton(); // crash

    // CORRECT — capture and dispatch to game thread:
    std::string arg = s ? s : "";
    F4SE::GetTaskInterface()->AddTask([arg]() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        // ... safe game thread access ...
    });
});
```

`InteropCall` and `Invoke` calls made from within `AddTask` (game thread) are safe — they marshal internally.

---

## IVPrismaUI1

### `CreateView`

```cpp
virtual PrismaView CreateView(
    const char* htmlPath,
    OnDomReadyCallback onDomReadyCallback = nullptr
) noexcept = 0;
```

Creates an HTML view and begins loading the specified file.

| Parameter | Description |
|-----------|-------------|
| `htmlPath` | Path relative to `Data/PrismaUI_F4/views/`, e.g. `"mymenu.html"` or `"Interface/MyPlugin/page.html"`. |
| `onDomReadyCallback` | Optional. Called once on the **main game thread** when the DOM is fully parsed. Safe to call `RegisterJSListener` and `Invoke` here. |

**Returns** a non-zero `PrismaView` handle on success. The view starts **hidden** — call `Show(view)` when you want it visible.

**Thread safety:** Call from the main thread (inside an F4SE message handler or `AddTask`).

**Create views on `kPostLoadGame` / `kNewGame`**, not on `kGameDataReady`. The rendering system is ready after a game is loaded.

---

### `Invoke`

```cpp
virtual void Invoke(
    PrismaView view,
    const char* script,
    JSCallback callback = nullptr
) noexcept = 0;
```

Evaluates an arbitrary JavaScript expression in the view's context. The `JSCallback` result is delivered on the **main game thread**.

**Example:**
```cpp
// Push data into the page
api->Invoke(view, "updateInventory('[{\"name\":\"Stimpack\",\"count\":5}]')");

// Read a value back
api->Invoke(view, "document.getElementById('hp').textContent", [](const char* val) {
    logger::info("HP display: {}", val);
});
```

**Encoding:** Auto-converts ANSI game strings to UTF-8.

**Performance:** Higher overhead than `InteropCall` — posts to the Ultralight thread and waits for a round-trip. Use for one-shots and reads, not high-frequency calls.

---

### `InteropCall`

```cpp
virtual void InteropCall(
    PrismaView view,
    const char* functionName,
    const char* argument
) noexcept = 0;
```

Calls a named JavaScript function via the JS Interop API (lower overhead than `Invoke`). The function must exist on the `window` object.

**Example:**
```cpp
// C++
api->InteropCall(view, "onInventoryData", jsonString.c_str());

// JS (mymenu.html)
function onInventoryData(json) {
  var items = JSON.parse(json);
  // render items...
}
```

**Encoding:** Same ANSI → UTF-8 auto-conversion as `Invoke`.

Use `InteropCall` for anything called more than once per second or in tight loops.

---

### `RegisterJSListener`

```cpp
virtual void RegisterJSListener(
    PrismaView view,
    const char* functionName,
    JSListenerCallback callback
) noexcept = 0;
```

Exposes a C++ callback to JavaScript. After registration, calling `window.functionName(arg)` from JS invokes the C++ callback.

> **Warning:** Callback fires on the Ultralight render thread. See [Threading Warning](#threading-warning-js-listener-callbacks).

**Best practice:** Register listeners inside your `OnDomReadyCallback`.

**Example:**
```cpp
// C++ — register in OnDomReady
api->RegisterJSListener(view, "onCloseRequest", [](const char* /*arg*/) {
    // No RE:: access here — safe, this is just UI state
    F4SE::GetTaskInterface()->AddTask([]() {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    });
});

// JS — call from the page
onCloseRequest();
```

---

### `HasFocus`

```cpp
virtual bool HasFocus(PrismaView view) noexcept = 0;
```

Returns `true` if this view currently has input focus.

---

### `Focus`

```cpp
virtual bool Focus(
    PrismaView view,
    bool pauseGame = false,
    bool disableFocusMenu = false
) noexcept = 0;
```

Gives input focus to the view, routing keyboard and mouse events to the HTML page.

| Parameter | Description |
|-----------|-------------|
| `pauseGame` | If `true`, sets game time scale to 0. Restored on `Unfocus`. Use for menus that need exclusive player attention. |
| `disableFocusMenu` | If `false` (default), PrismaUI shows a Scaleform FocusMenu overlay that manages the cursor and intercepts ESC to unfocus. If `true`, the overlay is suppressed — keyboard events reach the HTML `keydown` handler directly and the game's existing cursor (e.g. PauseMenu cursor) remains active. |

**When to use `disableFocusMenu=true`:** When your view opens on top of an existing game menu that already shows a cursor (e.g. the PauseMenu). With `false`, the FocusMenu overlay intercepts ESC before your JS sees it and may conflict with the existing cursor. With `true`, your JS `keydown` handler is responsible for closing the view on ESC.

**When to use `disableFocusMenu=false` (default):** Standard toggle menus opened directly from gameplay with no other game menu present.

**Returns** `true` on success.

---

### `Unfocus`

```cpp
virtual void Unfocus(PrismaView view) noexcept = 0;
```

Removes focus from the view. Restores game input. If `pauseGame` was `true` on `Focus`, game time is restored.

Call `Hide` after `Unfocus` if you want the view invisible while not in use.

---

### `Show`

```cpp
virtual void Show(PrismaView view) noexcept = 0;
```

Makes a hidden view visible at the next Present call. Does not grant input focus.

---

### `Hide`

```cpp
virtual void Hide(PrismaView view) noexcept = 0;
```

Removes a visible view from the composite. Does not destroy the view or stop JavaScript execution.

---

### `IsHidden`

```cpp
virtual bool IsHidden(PrismaView view) noexcept = 0;
```

Returns `true` if the view is currently hidden.

---

### `GetScrollingPixelSize`

```cpp
virtual int GetScrollingPixelSize(PrismaView view) noexcept = 0;
```

Returns the number of pixels scrolled per mouse wheel tick. Default: 28 px.

---

### `SetScrollingPixelSize`

```cpp
virtual void SetScrollingPixelSize(PrismaView view, int pixelSize) noexcept = 0;
```

Sets the mouse wheel scroll amount in pixels.

---

### `IsValid`

```cpp
virtual bool IsValid(PrismaView view) noexcept = 0;
```

Returns `true` if the view handle is live and backed by a real Ultralight view. Check this before any operation on a stored handle, especially after a game reload.

---

### `Destroy`

```cpp
virtual void Destroy(PrismaView view) noexcept = 0;
```

Tears down the view completely. The handle becomes invalid. Rarely needed — views are typically kept alive for the session.

---

### `SetOrder`

```cpp
virtual void SetOrder(PrismaView view, int order) noexcept = 0;
```

Sets the rendering z-order. Higher values render on top. Default is 0.

---

### `GetOrder`

```cpp
virtual int GetOrder(PrismaView view) noexcept = 0;
```

Returns the current z-order.

---

### `CreateInspectorView`

```cpp
virtual void CreateInspectorView(PrismaView view) noexcept = 0;
```

Attaches a WebKit DevTools inspector to the view. Call once before using other inspector methods. Do not ship this call in released mods.

---

### `SetInspectorVisibility`

```cpp
virtual void SetInspectorVisibility(PrismaView view, bool visible) noexcept = 0;
```

Shows or hides the inspector overlay.

---

### `IsInspectorVisible`

```cpp
virtual bool IsInspectorVisible(PrismaView view) noexcept = 0;
```

Returns `true` if the inspector is currently visible.

---

### `SetInspectorBounds`

```cpp
virtual void SetInspectorBounds(
    PrismaView view,
    float topLeftX,
    float topLeftY,
    unsigned int width,
    unsigned int height
) noexcept = 0;
```

Positions and sizes the inspector overlay in screen pixels.

---

### `HasAnyActiveFocus`

```cpp
virtual bool HasAnyActiveFocus() noexcept = 0;
```

Returns `true` if any PrismaUI view currently has input focus. Use in hotkey handlers to suppress game actions while a menu is open.

---

## IVPrismaUI2

Extends `IVPrismaUI1`.

### `RegisterConsoleCallback`

```cpp
virtual void RegisterConsoleCallback(
    PrismaView view,
    ConsoleMessageCallback callback
) noexcept = 0;
```

Receives all `console.log/warn/error/debug/info` calls from the view's JS context. Pass `nullptr` to unregister. Callback fires on the **main game thread**.

Always register this during development — JS errors are otherwise silent.

```cpp
api->RegisterConsoleCallback(view,
    [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel lvl, const char* msg) {
        const char* tag = lvl == PRISMA_UI_API::ConsoleMessageLevel::Error   ? "[JS ERR] " :
                          lvl == PRISMA_UI_API::ConsoleMessageLevel::Warning  ? "[JS WARN]" :
                                                                                "[JS LOG] ";
        logger::info("{} {}", tag, msg);
    });
```

---

## IVPrismaUI3

Extends `IVPrismaUI2`.

### `RegisterTranslations`

```cpp
virtual void RegisterTranslations(
    PrismaView view,
    const char* pluginName
) noexcept = 0;
```

Loads a translation file and injects `window.L10N` and `window.t()` into the view's JavaScript context on every page load.

| Parameter | Description |
|-----------|-------------|
| `pluginName` | Your plugin's base name without extension, e.g. `"MyPlugin_F4"`. |

**Translation file location:** `Data\Interface\Translations\<pluginName>_<lang>.txt`
where `<lang>` matches the game's current language setting (e.g. `en`, `de`, `fr`).

**Call after `CreateView`.** Translations are re-injected automatically on each subsequent page load, so you only need to call this once per view.

**In JS:**
```javascript
// window.t() returns the translated string for a key, or the key itself if not found
document.getElementById('title').textContent = t('ui.title');

// window.L10N is the raw object of all key → value pairs
console.log(L10N['ui.title']);
```

**Translation file format** (one entry per line):
```
ui.title	My Plugin
ui.close	Close
ui.settings	Settings
```

---

## Typical Call Sequence

```
F4SEPlugin_Load:
  F4SE::Init(a_f4se)
  messaging->RegisterListener(F4SEMessageHandler)

kGameDataReady:
  RequestPluginAPI<IVPrismaUI3>()     → g_api
  [register key handler / event sink]

kPostLoadGame / kNewGame:
  g_api->CreateView("page.html", OnDomReady)   → g_view
  g_api->RegisterConsoleCallback(g_view, ...)
  g_api->RegisterTranslations(g_view, "MyPlugin_F4")   // V3 only

OnDomReady (game thread):
  g_api->RegisterJSListener(g_view, "fnName", callback)
  g_api->Invoke(g_view, "init()")

Toggle (key / event):
  if opening:
    g_api->Show(g_view)
    g_api->Focus(g_view, pauseGame, disableFocusMenu)
  if closing:
    g_api->Unfocus(g_view)
    g_api->Hide(g_view)

JSListenerCallback (Ultralight thread — dispatch RE:: work):
  F4SE::GetTaskInterface()->AddTask([capture]() {
      // RE:: access here is safe
      g_api->InteropCall(g_view, "result", data.c_str());
  });
```

---

## Papyrus Bridge API (window.prisma)

### Overview

`window.prisma` is automatically injected by PrismaUI_F4 into every HTML view. It provides read-only access to Papyrus globals and script properties without requiring C++ code in your plugin.

**Available methods:**
- `await prisma.getGlobal(esp, formId)` — Read a `TESGlobal` form value
- `await prisma.getProperty(esp, formId, scriptName, propertyName)` — Read an `Auto` property from a Papyrus script

### Property Writes and Globals

Both reads and writes are fully supported for both globals and properties:
- `await prisma.setGlobal(esp, formId, value)` — Write a `TESGlobal` float value
- `await prisma.setProperty(esp, formId, scriptName, propertyName, value)` — Write a float, int, or bool property to the script instance attached to the form.

**Under the Hood:** Writing to properties looks up the active script instance(s) attached to the form's handle in the Papyrus VM, finds the backing variable by name, and updates it directly on the game thread.

**Callbacks/Events Limitation:** Because updating a property variable directly from F4SE/C++ does not run any Papyrus VM instructions (like a custom property `Set()` block in Papyrus), the Papyrus script will not be automatically notified of the change unless it polls the property value or unless another event is triggered. For workflows that require immediate script execution when a setting changes, write to a global or property, and have the Papyrus script poll it periodically (using `RegisterForSingleUpdate`), or use MCM actions.

### Return Values

Both read methods return **Promises** and handle errors gracefully:
- Returns a `number` on success (including 0.0, which is a valid result)
- Returns `null` if the form/plugin is not loaded, form doesn't exist, or script/property name mismatch
- Never throws — always guard with `if (val === null)`

### Example

```javascript
// Read a global
const val = await prisma.getGlobal('MyMod.esp', '800');
if (val !== null) {
    console.log('Global value:', val);
} else {
    console.log('Form not found or plugin not loaded');
}

// Read a quest property (most reliable host for properties)
const propVal = await prisma.getProperty('MyMod.esp', '801', 'MyQuestScript', 'CurrentPhase');
if (propVal !== null && propVal !== undefined) {
    console.log('Quest phase:', propVal);
}
```

### Timing

`window.prisma` is available immediately — no wait needed. However, `getProperty` calls may return `null` if the Papyrus VM is not yet ready (e.g., if called before `kPostLoadGame`). For best results, call property reads after the game has finished loading.
