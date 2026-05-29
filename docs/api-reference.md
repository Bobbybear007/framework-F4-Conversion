# API Reference — PrismaUI_F4

## Overview

The public API is declared entirely in `PrismaUI_F4_API.h`. Copy that single header into your plugin's `src/` folder. You do not link against PrismaUI_F4 at compile time; the connection is made at runtime via `GetProcAddress`.

```cpp
#include "PrismaUI_F4_API.h"

// On kGameDataReady:
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();
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

All callbacks are invoked on the main game thread.

---

## Interface Versions

| Type | Version | Notes |
|------|---------|-------|
| `IVPrismaUI1` | V1 | All core view operations |
| `IVPrismaUI2` | V2 | Extends V1, adds `RegisterConsoleCallback` |

Always request the highest version you need. If the user's installed PrismaUI_F4 is older than your requested version, `RequestPluginAPI` returns `nullptr` — handle this gracefully.

```cpp
// Request v2 (recommended)
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();

// Request v1 only (if you don't need console callbacks)
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
| `htmlPath` | Filename only, e.g. `"mymenu.html"`. The framework prepends `file:///views/` and maps to `Data/PrismaUI_F4/views/`. Do not include path separators or subdirectory components. **Exception:** if the string starts with `http://` or `https://`, it is used as-is (the game process has no network access in practice; this path is for local dev/testing only). |
| `onDomReadyCallback` | Optional. Called once on the main thread when the DOM is fully parsed. Safe to call `RegisterJSListener` and `Invoke` from here. |

**Returns** a non-zero `PrismaView` handle on success. The view starts **visible** — always call `Hide(view)` immediately after creation unless you want it to appear on screen right away.

**Thread safety:** Call from the main thread (e.g., inside an F4SE message handler).

**Create views on `kPostLoadGame` / `kNewGame`**, not on `kGameDataReady`. The view system is ready after a game is loaded, not immediately at plugin init.

---

### `Invoke`

```cpp
virtual void Invoke(
    PrismaView view,
    const char* script,
    JSCallback callback = nullptr
) noexcept = 0;
```

Evaluates an arbitrary JavaScript expression in the view's context.

| Parameter | Description |
|-----------|-------------|
| `script` | Any valid JS expression or statement. The return value of the expression (if a callback is provided) is serialized to a string and passed to `callback`. |
| `callback` | Optional. Receives the JSON-serialized result of `script`. Called on the main thread. |

**Example:**
```cpp
// Push data into the page
api->Invoke(view, "updateInventory('[{\"name\":\"Stimpack\",\"count\":5}]')");

// Read a value back
api->Invoke(view, "document.getElementById('hp').textContent", [](const char* val) {
    logger::info("HP display: {}", val);
});
```

**Encoding:** `Invoke` validates the script string as UTF-8 and automatically converts from ANSI if needed. Item names from the game's string tables are often ANSI — this conversion is handled for you.

**Performance:** `Invoke` serializes the script string and posts it to the Ultralight thread. For high-frequency calls, prefer `InteropCall`.

---

### `InteropCall`

```cpp
virtual void InteropCall(
    PrismaView view,
    const char* functionName,
    const char* argument
) noexcept = 0;
```

Calls a named JavaScript function via the JS Interop API (lower overhead than `Invoke`). The function must be defined on the `window` object. The argument is passed as a single string parameter.

| Parameter | Description |
|-----------|-------------|
| `functionName` | Name of a `window`-level JS function |
| `argument` | String passed as the sole argument. Use JSON to pass structured data. |

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

**Encoding:** Same ANSI → UTF-8 auto-conversion as `Invoke` — safe to pass game strings directly.

Use `InteropCall` instead of `Invoke` when you're calling the same function repeatedly (e.g., every frame or on every inventory change).

---

### `RegisterJSListener`

```cpp
virtual void RegisterJSListener(
    PrismaView view,
    const char* functionName,
    JSListenerCallback callback
) noexcept = 0;
```

Exposes a C++ callback to JavaScript. After registration, calling `functionName()` from JS invokes the C++ callback with the first argument serialized to a string.

| Parameter | Description |
|-----------|-------------|
| `functionName` | The JS function name that will be created on `window`. |
| `callback` | C++ function called with the string argument. Called on the main thread. |

**Best practice:** Register listeners inside your `OnDomReadyCallback`, not before.

**Example:**
```cpp
// C++ — register in OnDomReady
api->RegisterJSListener(view, "onCloseRequest", [](const char* /*arg*/) {
    api->Unfocus(view);
    api->Hide(view);
});

// JS — call from the page
onCloseRequest();           // close with no data
onCloseRequest('{"ok":1}'); // close with a payload
```

---

### `HasFocus`

```cpp
virtual bool HasFocus(PrismaView view) noexcept = 0;
```

Returns `true` if this view currently has input focus. When focused, keyboard and mouse input goes to the HTML view rather than the game.

---

### `Focus`

```cpp
virtual bool Focus(
    PrismaView view,
    bool pauseGame = false,
    bool disableFocusMenu = false
) noexcept = 0;
```

Gives input focus to the view. This:
- Routes keyboard and mouse events to the HTML page
- Makes the game cursor visible and releases `ClipCursor`
- Optionally pauses game time

| Parameter | Description |
|-----------|-------------|
| `pauseGame` | If `true`, sets game time scale to 0 (freezes NPCs, timers). Restored on `Unfocus`. |
| `disableFocusMenu` | Advanced. If `true`, suppresses the FocusMenu Scaleform overlay. Use `false` unless you have a specific reason. |

**Returns** `true` on success.

**Call `Show` before `Focus`.** A hidden view can receive focus but the user won't see it.

---

### `Unfocus`

```cpp
virtual void Unfocus(PrismaView view) noexcept = 0;
```

Removes focus from the view. Restores game input, hides the cursor, and re-engages `ClipCursor`. If `pauseGame` was `true` on `Focus`, game time is restored.

Call `Hide` after `Unfocus` if you want the view invisible while not in use.

---

### `Show`

```cpp
virtual void Show(PrismaView view) noexcept = 0;
```

Makes a hidden view visible. The view is composited on top of the game image at the next Present call. Does not grant input focus.

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

Returns `true` if the view handle is live and backed by a real Ultralight view. Check this before any operation if the view might have been destroyed or not yet created.

---

### `Destroy`

```cpp
virtual void Destroy(PrismaView view) noexcept = 0;
```

Tears down the view completely, freeing all resources (Ultralight view, D3D11 textures, JS context). The handle becomes invalid after this call. You rarely need this; views are typically kept alive for the session.

---

### `SetOrder`

```cpp
virtual void SetOrder(PrismaView view, int order) noexcept = 0;
```

Sets the rendering z-order. Higher values render on top. Default is 0 for all views. When two views overlap, the one with the higher order is drawn over the other.

---

### `GetOrder`

```cpp
virtual int GetOrder(PrismaView view) noexcept = 0;
```

Returns the current z-order of the view.

---

### `CreateInspectorView`

```cpp
virtual void CreateInspectorView(PrismaView view) noexcept = 0;
```

Attaches an Ultralight inspector (developer tools) to the view. Must be called once before using any other inspector methods. The inspector view is a separate HTML view containing the WebKit DevTools UI.

---

### `SetInspectorVisibility`

```cpp
virtual void SetInspectorVisibility(PrismaView view, bool visible) noexcept = 0;
```

Shows or hides the inspector overlay. The inspector must have been created with `CreateInspectorView` first.

---

### `IsInspectorVisible`

```cpp
virtual bool IsInspectorVisible(PrismaView view) noexcept = 0;
```

Returns `true` if the inspector overlay is currently visible.

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

| Parameter | Description |
|-----------|-------------|
| `topLeftX` / `topLeftY` | Screen position of the inspector's top-left corner |
| `width` / `height` | Inspector dimensions in pixels |

---

### `HasAnyActiveFocus`

```cpp
virtual bool HasAnyActiveFocus() noexcept = 0;
```

Returns `true` if any PrismaUI view currently has input focus. Useful for suppressing game hotkeys while a menu is open.

---

## IVPrismaUI2

Extends `IVPrismaUI1` with one additional method.

### `RegisterConsoleCallback`

```cpp
virtual void RegisterConsoleCallback(
    PrismaView view,
    ConsoleMessageCallback callback
) noexcept = 0;
```

Registers a callback that receives all `console.log`, `console.warn`, `console.error`, `console.debug`, and `console.info` calls from the view's JavaScript context.

| Parameter | Description |
|-----------|-------------|
| `callback` | Function called with view handle, severity level, and message text. Pass `nullptr` to unregister. |

**Always register this during development.** JavaScript errors are otherwise silent. The callback fires on the main thread.

```cpp
api->RegisterConsoleCallback(view,
    [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel lvl, const char* msg) {
        const char* tag = lvl == PRISMA_UI_API::ConsoleMessageLevel::Error  ? "[JS ERR] " :
                          lvl == PRISMA_UI_API::ConsoleMessageLevel::Warning ? "[JS WARN]" :
                                                                               "[JS LOG] ";
        logger::info("{} {}", tag, msg);
    });
```

---

## Typical Call Sequence

```
kGameDataReady:
  RequestPluginAPI<IVPrismaUI2>()       → get api
  KeyHandler::RegisterSink()
  KeyHandler::Register(key, Toggle)

kPostLoadGame / kNewGame:
  api->CreateView("page.html", OnDomReady)   → g_view
  api->RegisterConsoleCallback(g_view, ...)
  api->Hide(g_view)

OnDomReady:
  api->RegisterJSListener(g_view, "jsFunc", cppCallback)
  api->Invoke(g_view, "initPage()")

Toggle (key press):
  if visible:
    api->Show(g_view)
    api->Focus(g_view, pauseGame, false)
  else:
    api->Unfocus(g_view)
    api->Hide(g_view)
```
