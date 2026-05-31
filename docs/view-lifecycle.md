# View Lifecycle

## States

A PrismaUI view moves through these states:

```
[not created]
     │
     │  CreateView("page.html", onDomReady)
     ▼
[loading]          ← Ultralight is fetching and parsing the HTML
     │
     │  DOM parsed and JS executed
     ▼
[ready + visible]  ← default state after creation (call Hide immediately!)
     │
     │  Hide()
     ▼
[ready + hidden]   ← typical idle state for toggle menus
     │
     │  Show()
     ▼
[visible + no focus]  ← rendered on screen, game input unchanged
     │
     │  Focus(view, pauseGame, disableFocusMenu)
     ▼
[visible + focused]   ← mouse/keyboard routed to HTML, cursor shown
     │
     │  Unfocus()
     ▼
[visible + no focus]
     │
     │  Hide()
     ▼
[ready + hidden]
     │
     │  Destroy()  (optional — rarely needed)
     ▼
[destroyed]
```

---

## Creation

```cpp
PrismaView view = api->CreateView("page.html", OnDomReady);
api->Hide(view);   // views start hidden; this is redundant but explicit
```

`CreateView` is asynchronous — the HTML file is loaded on the Ultralight thread. Your `OnDomReady` callback fires on the **main thread** (via `F4SE::GetTaskInterface()->AddTask`) after the DOM is parsed and all inline `<script>` blocks have executed.

**Do not call `Invoke` or `RegisterJSListener` before `OnDomReady` fires.** The JS context is not yet ready.

**Create views on `kPostLoadGame` / `kNewGame`**, not on `kGameDataReady`. Example:

```cpp
case F4SE::MessagingInterface::kPostLoadGame:
case F4SE::MessagingInterface::kNewGame:
    if (g_view == 0 && g_api) CreateMyViews();
    break;
```

Guard with `g_view == 0` to avoid creating duplicates on multiple load events.

---

## DOM Ready Callback

```cpp
static void OnDomReady(PrismaView view)
{
    // RegisterConsoleCallback — capture JS errors during development
    g_api->RegisterConsoleCallback(view,
        [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel lvl, const char* msg) {
            logger::info("[JS] {}", msg);
        });

    // BindUIEvent (V4) — fires on game thread, RE:: access safe directly
    g_api->BindUIEvent(view, "onAction", [](const char* data) {
        // RE:: access safe here
    });

    // RegisterJSListener — use for pure UI callbacks that don't need game state
    g_api->RegisterJSListener(view, "onClose", [](const char*) {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    });

    g_api->Invoke(view, "init()");
    logger::info("DOM ready for view {}", view);
}
```

The callback fires on the **main game thread** (dispatched via `AddTask`). The view handle is passed so you can use one `OnDomReady` function for multiple views.

**Do not call `Invoke`, `RegisterJSListener`, or `BindUIEvent` before this callback fires.** The JS context is not ready until then.

---

## Show / Hide

`Show` and `Hide` control compositing — whether the view's pixels are included in the D3D11 Present call. They are **not** the same as `Focus`/`Unfocus`.

| Operation | What it does |
|-----------|-------------|
| `Show` | View is rendered on screen |
| `Hide` | View is invisible but JS keeps running |
| `Focus` | Input (mouse + keyboard) goes to the view, cursor shown |
| `Unfocus` | Input returns to game, cursor hidden |

Typical toggle pattern:

```cpp
static void Toggle()
{
    if (!g_api || !g_api->IsValid(g_view)) return;
    g_visible = !g_visible;
    if (g_visible) {
        g_api->Show(g_view);
        g_api->Focus(g_view, /*pauseGame=*/true, /*disableFocusMenu=*/false);
    } else {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    }
}
```

---

## Focus

### `pauseGame`

When `true`, the game's time scale is set to zero — NPCs stop moving, timers pause, projectiles freeze. Restored automatically on `Unfocus`. Use for menus where the player needs to interact without danger (inventory, settings, terminal).

When `false`, the game continues running while the UI is open. Use for HUDs or overlays that don't require exclusive attention.

### `disableFocusMenu`

Normally `false`. The FocusMenu is a Scaleform overlay the framework uses to route the game cursor to the HTML view. Setting this to `true` suppresses it, which can cause cursor visibility issues. Leave it `false` unless you have a specific technical reason.

### `HasAnyActiveFocus`

```cpp
if (api->HasAnyActiveFocus()) {
    // Suppress game hotkeys while any PrismaUI menu is open
    return;
}
```

Use this in your hotkey handler to prevent accidental game actions (drawing weapons, etc.) while a menu is open.

---

## Multiple Views

Each call to `CreateView` produces an independent view with its own Ultralight context, D3D11 textures, and JS environment. Views do not share state.

**Ordering:** Views are composited in ascending `order` value. Default order is 0. If two views overlap, set the one that should appear on top to a higher order:

```cpp
api->SetOrder(backgroundView, 0);
api->SetOrder(popupView, 10);
```

**Focus:** Only one view can have focus at a time. Calling `Focus` on a second view while the first is focused will focus the second; the first loses focus.

**Performance:** Each active view costs GPU texture memory and Ultralight rendering time. Keep views hidden when not in use. Rendering is skipped for hidden views.

---

## View Recovery

PrismaUI_F4 has an internal recovery system. If the Ultralight thread throws a structured exception (SEH) while processing a view, the framework marks that view for recovery and reloads it from its original URL. Recovery attempts are limited to prevent infinite loops.

You do not need to implement recovery logic in your plugin. If a view is behaving strangely after a long game session, check your F4SE log for recovery messages.

---

## Inspector

The Ultralight inspector is a WebKit DevTools interface. Use it during development to debug layout, run console queries, and inspect element styles.

```cpp
// Setup (do once, do not ship to end users)
api->CreateInspectorView(view);
api->SetInspectorBounds(view, 0.0f, 0.0f, 900, 550);

// Toggle visibility
bool showing = api->IsInspectorVisible(view);
api->SetInspectorVisibility(view, !showing);
```

The inspector renders as an overlay at the position and size you specify. You can interact with it using the mouse while it's visible. The main view is still rendered beneath it.

**Do not ship `CreateInspectorView` calls in released mods.** Wrap them in a debug flag or `#ifdef`:

```cpp
#ifdef PRISMA_DEBUG
api->CreateInspectorView(g_view);
api->SetInspectorBounds(g_view, 10.0f, 10.0f, 900, 560);
api->SetInspectorVisibility(g_view, true);
#endif
```

---

## Destruction

`Destroy` fully tears down a view. After calling it, the handle is invalid — do not use it again.

```cpp
api->Unfocus(view);
api->Hide(view);
api->Destroy(view);
view = 0;
```

Destruction happens asynchronously on the Ultralight thread. Do not create a new view with the same filename immediately after destroying one; wait for the next `kPostLoadGame` event.

In normal usage you never need to destroy views — create them once on `kPostLoadGame` and keep them for the session.

---

## Scroll

Mouse wheel events are forwarded to the focused view. The scroll amount in pixels per tick can be tuned:

```cpp
api->SetScrollingPixelSize(view, 40);  // faster scrolling
```

The default is 28 px per tick. Adjusting this only affects this view; other views are unaffected.
