---
title: 'Limitations'
---
# Limitations

PrismaUI F4 is powered by **Ultralight**, an embeddable WebKit-based renderer. It is not a full browser. The following limitations apply to all views regardless of what you build.

---

## Media

**Video is not supported.** The `<video>` element and all video playback APIs are unavailable. Use animated GIF images as a replacement for looping animations.

**Audio is not supported.** The Web Audio API and `<audio>` element do not function. Play sounds through F4SE instead - trigger audio from your C++ plugin in response to JavaScript callbacks.

**WebGL is not supported.** Canvas 2D works, but `getContext('webgl')` and `getContext('webgl2')` return `null`.

---

## Rendering

PrismaUI F4 uses **CPU rendering only**. GPU-accelerated compositing is not available in the current release.

- UI refresh rate is capped at **60 FPS**
- Heavy CSS operations cause real FPS impact. Use sparingly:
  - `box-shadow` and `text-shadow` on large elements
  - `filter: blur()`, `backdrop-filter`
  - Large `border-radius` on frequently-repainted elements
  - Heavy `background: linear-gradient()` on animated elements

Keep your UI lightweight. Flat colours and simple transitions perform significantly better than visually complex designs.

---

## JavaScript

- ES2022 and below only. Features introduced in ES2023+ may not work.
- WebKit version: `615.1.18.100.1`
- For a full compatibility matrix, see the [Ultralight Supported Web Features](https://github.com/ultralight-ux/Ultralight/wiki/Supported-Web-Features) wiki page.

---

## CSS Frameworks

If you use **TailwindCSS**, you must use **v3**. Tailwind v4 relies on browser features not available in Ultralight and will not work.

---

## Operation Queue

Each view has an internal operation queue with a hard limit of **100 pending operations**. Show, Hide, Focus, Unfocus, and similar calls each occupy one slot. If the queue fills up, further operations are silently dropped with an error in the F4SE log.

In normal usage this limit is never reached. It becomes relevant only if you are calling Show/Hide/Focus in a tight loop faster than the Ultralight thread can process them - for example, toggling a view every frame. Throttle such calls or gate them behind state checks (`IsHidden`, `HasFocus`) to avoid queuing redundant operations.

---

## Multiple Views

The API supports multiple views per plugin. Each view has its own Ultralight context, D3D11 textures, and JS environment - they do not share state.

Where possible, prefer a **single view** that manages its own routing and page state internally (React Router, Vue Router, etc.). Multiple views are fine for genuinely separate surfaces like a persistent HUD alongside a toggle menu, but avoid creating many views for screens that could be JS routes inside one page. Each view adds texture memory and CPU rendering overhead.

---

## Event Handling Quirks

### Right-click / contextmenu

The `contextmenu` DOM event does not fire correctly. Implement right-click detection manually:

```javascript
window.addEventListener('mousedown', (event) => {
  if (event.button === 2) {
    const contextMenuEvent = new MouseEvent('contextmenu', {
      ...event,
      view: window,
      bubbles: true,
      cancelable: true,
      screenX: event.pageX,
      screenY: event.pageY,
      clientX: event.pageX,
      clientY: event.pageY,
    });
    event.target?.dispatchEvent(contextMenuEvent);
  }
});
```

### Blocking specific keys from inputs

Numpad keys and other game-bound keys may type characters into focused `<input>` elements. Block them with a combined `keydown` + `beforeinput` listener:

```javascript
const BLOCKED_KEY_CODES = [
  96, 97, 98, 99, 100, // Numpad 0-4
  101, 102, 103, 104, 105, // Numpad 5-9
];

let lastKeyCode = null;

window.addEventListener('keydown', (e) => {
  lastKeyCode = e.keyCode;
}, { capture: true });

window.addEventListener('beforeinput', (e) => {
  if (lastKeyCode !== null && BLOCKED_KEY_CODES.includes(lastKeyCode)) {
    e.preventDefault();
  }
}, { capture: true });
```

---

## Custom Cursor

Replace the default system cursor with a custom PNG image when any PrismaUI view has focus. Place your file at:

```
Data/PrismaUI_F4/misc/cursor.png
```

In MO2, inside your mod folder:

```
mods/MyPlugin_F4/
└── PrismaUI_F4/
    └── misc/
        └── cursor.png
```

No code changes required. PrismaUI F4 automatically uses the file when a view is focused. Use a PNG that is visible against varying in-game backgrounds.

---

## Ultralight DLL version mismatch

**Symptom:** `AppCore.dll could not be loaded (Error 127)` in the F4SE log even though the DLL is present next to the other Ultralight files.

**Error 127** (`ERROR_PROC_NOT_FOUND`) means the DLL loaded but an exported function it expects doesn't exist in that version. This happens when you have Ultralight DLLs from an older PrismaUI_F4 install in `Data/PrismaUI_F4/libs/` and install a newer version of the framework without replacing them.

**Fix:** Delete all files inside `Data/PrismaUI_F4/libs/` and redeploy the full PrismaUI_F4 package. Never mix DLL files from different PrismaUI_F4 versions - all four (`AppCore.dll`, `Ultralight.dll`, `UltralightCore.dll`, `WebCore.dll`) must come from the same release.
