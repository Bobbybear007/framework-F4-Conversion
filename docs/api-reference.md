# API Reference — PrismaUI_F4

## Overview

The public API is declared entirely in `PrismaUI_F4_API.h`. Copy that single header into your plugin's `src/` folder. You do not link against PrismaUI_F4 at compile time; the connection is made at runtime via `GetProcAddress`.

```cpp
#include "PrismaUI_F4_API.h"

// On kGameDataReady — request the latest interface:
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI4>();
```

An optional `PrismaUI_F4_Helper.h` ships alongside the API header. Drop it into your project for lightweight JSON parsing helpers and Papyrus event dispatch. It is not part of the core API guarantee.

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

All callbacks fire on the **main game thread**. RE:: access is safe inside all callbacks without wrapping in `AddTask`.

---

## Interface Versions

| Type | Added | New method |
|------|-------|-----------|
| `IVPrismaUI1` | V1 | All core view operations |
| `IVPrismaUI2` | V2 | `RegisterConsoleCallback` |
| `IVPrismaUI3` | V3 | `RegisterTranslations` |
| `IVPrismaUI4` | V4 | `BindUIEvent`, `EnumerateViews` |

Interfaces are additive — `IVPrismaUI4` exposes every method from V1 through V4. Always request the highest version you need. If the user's installed PrismaUI_F4 does not support your requested version, `RequestPluginAPI` returns `nullptr` — handle this gracefully.

```cpp
// Recommended — request V4 (includes all previous methods)
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI4>();

// Fallback pattern — use V4 if available, V2 otherwise
auto* api4 = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI4>();
auto* api  = api4 ? static_cast<PRISMA_UI_API::IVPrismaUI2*>(api4)
                  : PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();
```

---

## Updating Your Plugin to a New Interface Version

If your plugin was built on V2 and you want V4 features, change one line:

```cpp
// Before
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();

// After
auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI4>();
```

Change the pointer type to match:

```cpp
// Before
static PRISMA_UI_API::IVPrismaUI2* g_api = nullptr;

// After
static PRISMA_UI_API::IVPrismaUI4* g_api = nullptr;
```

That is the entire change. All V1/V2 methods remain identical. Your existing `RegisterJSListener`, `Invoke`, `InteropCall`, etc. calls compile and behave identically against V4.

If you need to support users who have an older PrismaUI_F4 installed, check for `nullptr`:

```cpp
g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI4>();
if (!g_api) {
    logger::warn("PrismaUI V4 not available — BindUIEvent and RegisterTranslations unavailable.");
    // Optionally fall back:
    // g_api2 = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();
}
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
| `htmlPath` | Path relative to `Data/PrismaUI_F4/views/`, e.g. `"Interface/MyPlugin/menu.html"`. The framework prepends `file:///views/`. Paths beginning with `http://` or `https://` are **rejected** — the call returns `0`. |
| `onDomReadyCallback` | Optional. Called once on the main game thread when the DOM is fully parsed. Safe to call `RegisterJSListener`, `BindUIEvent`, and `Invoke` from here. |

**Returns** a non-zero `PrismaView` handle on success. The view starts **hidden** — call `Show(view)` when you want it visible.

**Thread safety:** Call from the main thread (e.g., inside an F4SE message handler).

**Create views on `kPostLoadGame` / `kNewGame`**, not on `kGameDataReady`.

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
| `script` | Any valid JS expression or statement. |
| `callback` | Optional. Receives the string-serialized result. Called on the main thread. |

**Encoding:** Auto-converts ANSI game strings to UTF-8.

**Performance:** Higher overhead than `InteropCall` — use for one-shots and reads, not per-frame calls.

---

### `InteropCall`

```cpp
virtual void InteropCall(
    PrismaView view,
    const char* functionName,
    const char* argument
) noexcept = 0;
```

Calls a named `window`-level JavaScript function with a single string argument (lower overhead than `Invoke`).

```cpp
// C++
api->InteropCall(view, "onInventoryData", jsonString.c_str());

// JS
function onInventoryData(json) {
    var items = JSON.parse(json);
}
```

Use `InteropCall` for anything called more than once per second. `Invoke` is the right choice for one-shots.

---

### `RegisterJSListener`

```cpp
virtual void RegisterJSListener(
    PrismaView view,
    const char* functionName,
    JSListenerCallback callback
) noexcept = 0;
```

Exposes a C++ callback to JavaScript. After registration, calling `window.functionName(arg)` from JS invokes the C++ callback on the main game thread.

**Best practice:** Register listeners inside your `OnDomReadyCallback`. RE:: access is safe inside the callback.

```cpp
api->RegisterJSListener(view, "onCloseRequest", [](const char* /*arg*/) {
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
});
```

> **Tip:** For JS → C++ calls that need game-state access, prefer `BindUIEvent` (V4) — it is identical but makes the game-thread guarantee explicit in code.

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

Gives input focus to the view. Routes keyboard and mouse events to the HTML page, makes the cursor visible, and releases `ClipCursor`.

| Parameter | Description |
|-----------|-------------|
| `pauseGame` | If `true`, freezes game time. Restored on `Unfocus`. |
| `disableFocusMenu` | Advanced. Leave `false` unless you have a specific reason. |

**Call `Show` before `Focus`.**

---

### `Unfocus`

```cpp
virtual void Unfocus(PrismaView view) noexcept = 0;
```

Removes focus, restores game input, hides cursor, and restores game time if it was paused.

---

### `Show`

```cpp
virtual void Show(PrismaView view) noexcept = 0;
```

Makes a hidden view visible. Does not grant input focus.

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

### `GetScrollingPixelSize` / `SetScrollingPixelSize`

```cpp
virtual int  GetScrollingPixelSize(PrismaView view) noexcept = 0;
virtual void SetScrollingPixelSize(PrismaView view, int pixelSize) noexcept = 0;
```

Gets/sets the mouse wheel scroll amount in pixels per tick. Default: 28 px.

---

### `IsValid`

```cpp
virtual bool IsValid(PrismaView view) noexcept = 0;
```

Returns `true` if the view handle is live. Check this before any operation if the view might have been destroyed or not yet created.

---

### `Destroy`

```cpp
virtual void Destroy(PrismaView view) noexcept = 0;
```

Fully tears down the view. The handle is invalid after this call. Rarely needed — views are typically kept alive for the session.

---

### `SetOrder` / `GetOrder`

```cpp
virtual void SetOrder(PrismaView view, int order) noexcept = 0;
virtual int  GetOrder(PrismaView view) noexcept = 0;
```

Sets/gets the rendering z-order. Higher values render on top. Default is 0.

---

### `CreateInspectorView`

```cpp
virtual void CreateInspectorView(PrismaView view) noexcept = 0;
```

Attaches an Ultralight inspector (developer tools) to the view. Call once before using other inspector methods. **Do not ship inspector calls in released mods.**

---

### `SetInspectorVisibility` / `IsInspectorVisible`

```cpp
virtual void SetInspectorVisibility(PrismaView view, bool visible) noexcept = 0;
virtual bool IsInspectorVisible(PrismaView view) noexcept = 0;
```

Shows/hides the inspector overlay.

---

### `SetInspectorBounds`

```cpp
virtual void SetInspectorBounds(
    PrismaView view,
    float topLeftX, float topLeftY,
    unsigned int width, unsigned int height
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

Registers a callback for all `console.log/warn/error/debug/info` output from the view. Pass `nullptr` to unregister. Callback fires on the main thread.

**Always register this during development** — JS errors are otherwise silent.

```cpp
g_api->RegisterConsoleCallback(view,
    [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel lvl, const char* msg) {
        const char* tag = lvl == PRISMA_UI_API::ConsoleMessageLevel::Error   ? "[JS ERR] " :
                          lvl == PRISMA_UI_API::ConsoleMessageLevel::Warning ? "[JS WARN]" :
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

Loads a Fallout 4 translation file for this view and injects `window.L10N` / `window.t` into the page before any page scripts run.

| Parameter | Description |
|-----------|-------------|
| `pluginName` | Your plugin's bare name, e.g. `"MyPlugin_F4"`. The framework looks for `Data/Interface/Translations/MyPlugin_F4_<lang>.txt`. Language is auto-detected from the game's INI files. Falls back to English if the language file is not found. |

**Call immediately after `CreateView`**, before the DOM is ready.

```cpp
g_view = g_api->CreateView("Interface/MyPlugin/menu.html", OnDomReady);
g_api->RegisterTranslations(g_view, "MyPlugin_F4");
g_api->Hide(g_view);
```

**Translation file format** (`Data/Interface/Translations/MyPlugin_F4_en.txt`, UTF-16 LE with BOM):
```
$MY_TITLE	My Menu Title
$CLOSE_BUTTON	Close
$ITEM_COUNT	Items: {0}
```

**JS usage:**
```javascript
// window.t() looks up a key, returns the key itself if not found
document.getElementById('title').textContent = window.t('$MY_TITLE');

// window.L10N is the raw object for direct access
console.log(window.L10N['$CLOSE_BUTTON']);
```

---

## IVPrismaUI4

Extends `IVPrismaUI3`.

### `BindUIEvent`

```cpp
virtual void BindUIEvent(
    PrismaView view,
    const char* functionName,
    JSListenerCallback callback
) noexcept = 0;
```

Exposes a C++ callback to JavaScript, guaranteed to fire on the **game thread**. This is the preferred way to handle any JS → C++ call that needs to access game state (RE:: types, actors, forms, etc.).

`BindUIEvent` is functionally identical to `RegisterJSListener` with an internal `AddTask` wrap. The difference is intent and clarity: `BindUIEvent` makes the threading guarantee explicit, so readers of your code know RE:: access is safe without any additional wrapping.

| Parameter | Description |
|-----------|-------------|
| `functionName` | The JS function name that will be created on `window`. |
| `callback` | C++ function called with the string argument. **Fires on the game thread.** RE:: access is safe directly inside. |

**Example — receive JSON from JS and act on game state:**

```cpp
// C++ — register in OnDomReady
g_api->BindUIEvent(view, "onItemEquip", [](const char* data) {
    // RE:: access is safe here — already on game thread
    uint32_t formId = PRISMA_UI_HELPER::GetJsonInt(data, "formId");
    auto* form = RE::TESForm::GetFormByID(formId);
    if (form) logger::info("Equip requested: {}", form->GetFullName());
});

// JS — send from the page
document.getElementById('equipBtn').onclick = function() {
    window.onItemEquip(JSON.stringify({ formId: 0x1234 }));
};
```

**`BindUIEvent` vs `RegisterJSListener`:**

| | `RegisterJSListener` | `BindUIEvent` |
|---|---|---|
| Thread | Game thread | Game thread |
| RE:: access | Safe | Safe |
| Intent | Generic listener | Listener that touches game state |

Both fire on the game thread. Use `BindUIEvent` when your callback accesses RE:: types — it communicates the intent clearly. Use `RegisterJSListener` for pure UI logic (closing a panel, updating a global) where game state is irrelevant.

---

### `EnumerateViews`

```cpp
// Callback type — called once per registered view
typedef void (*ViewEnumCallback)(PrismaView id, const char* htmlPath, void* userdata);

virtual void EnumerateViews(ViewEnumCallback callback, void* userdata) noexcept = 0;
```

Iterates over every currently-registered view across all plugins. Useful for debug tooling or overlay managers that need to know what views are alive.

| Parameter | Description |
|-----------|-------------|
| `callback` | Called synchronously for each view. `id` is the view handle; `htmlPath` is the relative path that was passed to `CreateView` (e.g. `"Interface/PrismaMCM/mcm.html"`). |
| `userdata` | Arbitrary pointer forwarded to the callback — use to pass context without a global. |

**Thread safety:** Safe to call from any thread.

```cpp
// List all active views to the log
g_api->EnumerateViews([](PrismaView id, const char* path, void*) {
    logger::info("  view={} path={}", id, path);
}, nullptr);
```

---

## Typical Call Sequence

```
kGameDataReady:
  RequestPluginAPI<IVPrismaUI4>()        → g_api
  KeyHandler::RegisterSink()
  KeyHandler::Register(key, Toggle)

kPostLoadGame / kNewGame:
  g_view = g_api->CreateView("Interface/MyPlugin/menu.html", OnDomReady)
  g_api->RegisterTranslations(g_view, "MyPlugin_F4")   // V3, if using translations
  g_api->Hide(g_view)

OnDomReady:
  g_api->RegisterConsoleCallback(g_view, ...)           // V2
  g_api->RegisterJSListener(g_view, "onClose", ...)     // pure UI — no game state
  g_api->BindUIEvent(g_view, "onAction", ...)           // V4 — accesses RE::
  g_api->Invoke(g_view, "init()")

Toggle (key press):
  if opening:
    g_api->Show(g_view)
    g_api->Focus(g_view, /*pauseGame=*/false)
    g_api->Invoke(g_view, "updateFocusLabel('Focused')")
  if closing:
    g_api->Unfocus(g_view)
    g_api->Hide(g_view)
```

---

## window.prisma — Papyrus Bridge

`window.prisma` is automatically injected into every view during `OnWindowObjectReady`, before any page scripts execute. No C++ plugin code is required — read and write game data directly from JavaScript.

### Methods

| Method | Returns | Description |
|---|---|---|
| `prisma.getGlobal(esp, formId)` | `Promise<number \| null>` | Read a `TESGlobal` value |
| `prisma.setGlobal(esp, formId, value)` | `void` | Write a `TESGlobal` value |
| `prisma.getProperty(esp, formId, scriptName, propName)` | `Promise<number \| boolean \| null>` | Read a Papyrus `Auto` property |
| `prisma.setProperty(esp, formId, scriptName, propName, value)` | `void` | Write a Papyrus `Auto` property |

**Parameters:**

- `esp` — plugin filename including extension, e.g. `"MyMod.esp"` or `"MyMod.esl"`
- `formId` — local hex form ID, no file-index byte — `"800"` means `0x00000800` inside the plugin
- `scriptName` — Papyrus script name attached to the form (case-insensitive)
- `propName` — `Auto` property name on that script (case-insensitive)

### Example

```js
// Read a TESGlobal — always await, always guard null
const diff = await prisma.getGlobal("MyMod.esp", "801");
if (diff === null) return; // plugin not loaded or form not found

// Write a TESGlobal
prisma.setGlobal("MyMod.esp", "801", 3.0);

// Read a Papyrus Auto property
const dmg = await prisma.getProperty("MyMod.esp", "800", "MyMod_QuestScript", "DamageScale");
if (dmg === null) return;

// Write a Papyrus Auto property
prisma.setProperty("MyMod.esp", "800", "MyMod_QuestScript", "DamageScale", 2.5);
```

### Null return

Read methods return `null` and never throw when:
- The plugin is not in the active load order
- The form ID does not exist in that plugin
- The form is not the expected type (`TESGlobal`, or script not attached to form)
- The property name is not found on the script
- The Papyrus VM is not ready (called before `kPostLoadGame`)

Always guard: `if (val === null) { return; }`

### Supported property types

`float`, `int`, `bool` only. Strings and arrays are not supported. Writes coerce the incoming JS value to the property's declared Papyrus type automatically.

### Requirements

- `window.prisma` exists from DOM ready, but the game data is not available until `kPostLoadGame`. Don't call reads during page init — call them in response to data pushed from C++ or user interaction.
- Quest forms are the most reliable script host — they persist across cell changes and fast travel.
- No C++ changes required. `window.prisma` is injected automatically into every view regardless of interface version (V1–V4).

---

## Security & Privacy

PrismaUI_F4 runs a full web-rendering engine (Ultralight/WebKit) inside the Fallout 4 process. Every view has access to F4SE's task interface and the full game address space through C++ callbacks registered by the hosting plugin. The following protections are applied automatically to every view created by any plugin, regardless of interface version (V1–V4):

### Network sandbox

The framework injects a security script into every view **before any page scripts execute** (`OnWindowObjectReady`). This script:

- Replaces `fetch`, `XMLHttpRequest`, `WebSocket`, `EventSource`, `Worker`, `SharedWorker`, `navigator.sendBeacon`, and `navigator.serviceWorker` with `undefined` using `Object.defineProperty({ configurable: false })`. Page scripts **cannot redefine or delete** these descriptors.
- Injects a `Content-Security-Policy` meta tag with `connect-src: 'none'` as the first element of `<head>`, blocking browser-level network loads (`<script src="https://...">`, `<img src="https://...">`, CSS `@import url(https://...)`, etc.).

### URL restrictions

`CreateView` rejects any `htmlPath` that begins with `http://` or `https://`. External URLs return `0` (invalid handle) and log an error. All views must load from local `file://` paths under `Data/PrismaUI_F4/views/`.

### Child view blocking

`window.open()` and `<a target="_blank">` navigation are blocked. Attempts are logged.

### What is not blocked

- `eval()` and `new Function()` — intentionally permitted for compatibility with mod UI patterns.
- Local `file://` reads — views can load local assets normally.
- C++ → JS calls via `InteropCall`/`Invoke` — these are the intended data channel and are not restricted.
- Non-JS DLL code making direct WinSock calls — out of scope; this sandbox operates at the JS/browser layer only.

### Audit logging

All network-source console messages (CSP violations, blocked resource loads) are written to the PrismaUI_F4 spdlog output at `warn` level, prefixed `[PrismaUI Security]`.
