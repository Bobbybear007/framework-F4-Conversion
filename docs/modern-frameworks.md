---
title: 'Modern Frameworks'
---
# Modern Frameworks

You can build PrismaUI F4 views with React, Vue, Svelte, or any other frontend framework. This guide explains the one timing problem you will hit and the pattern that solves it.

---

## The Timing Problem

When you use a modern framework, there is a specific ordering issue between F4SE sending data and your framework mounting components:

1. F4SE calls `CreateView()` — the HTML file starts loading
2. DOM becomes ready — `OnDomReadyCallback` fires on the C++ side
3. **F4SE sends initial data** via `Invoke()` or `InteropCall()`
4. **Your framework initializes** and mounts components
5. Components register their event handlers inside lifecycle hooks

Steps 3 and 4 can arrive in the wrong order. If F4SE sends data before your React components have mounted and registered their `window` functions, **that data is silently lost**.

### The common mistake

```jsx
// Wrong — window.receiveData does not exist yet when F4SE calls it
function MyComponent() {
  useEffect(() => {
    window.receiveData = (data) => {
      setState(JSON.parse(data));
    };
  }, []);
}
```

---

## The Solution: Register at Module Load Time

Register all `window.*` functions **outside any component or lifecycle hook**, at the top level of a module that is imported before your framework renders. Use an external state manager (Zustand works well) so those functions can write to state that components read reactively.

```
src/
├── store/
│   └── gameStore.ts   <- window.* registration + Zustand store
├── components/
│   └── PlayerHUD.tsx
├── main.tsx           <- imports store BEFORE ReactDOM.render
└── index.html
```

---

## Step 1 — Create the Store and Register Window Functions

```typescript
// src/store/gameStore.ts
import { create } from 'zustand';

interface PlayerData {
  health: number;
  maxHealth: number;
  name: string;
}

interface GameStore {
  player: PlayerData | null;
  isReady: boolean;
  setPlayer: (data: PlayerData) => void;
  updateHealth: (health: number) => void;
}

export const useGameStore = create<GameStore>((set) => ({
  player: null,
  isReady: false,
  setPlayer: (data) => set({ player: data, isReady: true }),
  updateHealth: (health) =>
    set((state) => ({
      player: state.player ? { ...state.player, health } : null,
    })),
}));

// These run IMMEDIATELY when the module is imported.
// F4SE can safely call them as soon as OnDomReadyCallback fires.
window.initializePlayer = (jsonData: string) => {
  try {
    useGameStore.getState().setPlayer(JSON.parse(jsonData));
  } catch (e) {
    console.error('[PrismaUI] initializePlayer parse error:', e);
  }
};

window.updatePlayerHealth = (value: string) => {
  useGameStore.getState().updateHealth(parseFloat(value));
};

console.log('[PrismaUI] F4SE bridge registered');
```

---

## Step 2 — Import the Store Before React Renders

```tsx
// src/main.tsx
import React from 'react';
import ReactDOM from 'react-dom/client';

// Import FIRST so window.* functions exist before React starts
import './store/gameStore';

import App from './App';

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);
```

---

## Step 3 — Read from the Store in Components

```tsx
// src/components/PlayerHUD.tsx
import { useGameStore } from '../store/gameStore';

export function PlayerHUD() {
  const player = useGameStore((state) => state.player);
  const isReady = useGameStore((state) => state.isReady);

  if (!isReady || !player) {
    return <div className="hud-loading">Connecting...</div>;
  }

  return (
    <div className="hud">
      <span className="name">{player.name}</span>
      <div
        className="health-bar"
        style={{ width: `${(player.health / player.maxHealth) * 100}%` }}
      />
    </div>
  );
}
```

---

## Step 4 — F4SE Side (C++)

Send data as soon as `OnDomReadyCallback` fires. By that point your JS bundle has already loaded and all `window.*` functions are registered.

```cpp
static void OnDomReady(PrismaView view)
{
    // Build initial player data and push it
    std::string json =
        "{\"name\":\"Sole Survivor\",\"health\":100,\"maxHealth\":100}";

    std::string script = "window.initializePlayer('" + json + "')";
    g_api->Invoke(view, script.c_str());

    // For high-frequency updates later, use InteropCall (faster than Invoke)
    // g_api->InteropCall(view, "updatePlayerHealth", "95");
}
```

---

## Sending Data Back to F4SE

Register JS listeners immediately after `CreateView` (before `OnDomReady` — this is safe, registration does not need the JS context). Call them from JavaScript by function name.

```cpp
// Register right after CreateView, no need to wait for OnDomReady
g_view = g_api->CreateView("mymenu.html", OnDomReady);
g_api->RegisterJSListener(g_view, "onSettingChanged", [](const char* data) {
    logger::info("Setting changed: {}", data);
});
g_api->RegisterJSListener(g_view, "onClose", [](const char*) {
    g_api->Unfocus(g_view);
    g_api->Hide(g_view);
});
g_api->Hide(g_view);
```

```typescript
// src/lib/f4se-api.ts
export const F4SE_API = {
  sendToF4SE: (fnName: string, data?: string) => {
    try {
      (window as Record<string, unknown>)[fnName]?.(data);
    } catch {
      /* silent in production */
    }
  },
};
```

```tsx
function SettingsPanel() {
  const handleClose = () => F4SE_API.sendToF4SE('onClose');
  const handleSave = (settings: object) =>
    F4SE_API.sendToF4SE('onSettingChanged', JSON.stringify(settings));

  return (
    <div>
      <button onClick={handleClose}>Close</button>
      <button onClick={() => handleSave({ volume: 0.8 })}>Save</button>
    </div>
  );
}
```

---

## TypeScript Window Declarations

Declare F4SE-injected functions and translation globals on `Window` to get proper types:

```typescript
// src/global.d.ts
export {};

declare global {
  interface Window {
    // F4SE JS listeners
    initializePlayer: (json: string) => void;
    updatePlayerHealth: (value: string) => void;
    onClose: (arg: string) => void;
    onSettingChanged: (data: string) => void;

    // PrismaUI translations (injected by RegisterTranslations before any script runs)
    t: (key: string) => string;
    L10N: Record<string, string>;
  }
}
```

`window.t` and `window.L10N` are available before your framework initializes — they are injected at the `OnWindowObjectReady` stage, earlier than `DOMContentLoaded`. You can call `window.t('$KEY')` safely from module-level code, store initializers, and component render functions without any guards.

```typescript
// Safe at any point — translations are ready before JS runs
const LABELS = {
  close:  window.t('$CLOSE'),
  title:  window.t('$MY_MENU_TITLE'),
};
```

If translations are optional in your plugin, guard the call:

```typescript
const t = (key: string) => window.t?.(key) ?? key;
```

---

## Development Mode

In a real browser (dev server) there is no F4SE to inject data. Guard dev-only behaviour with `import.meta.env.DEV`:

```tsx
// src/app.tsx
export const App = () => {
  useEffect(() => {
    if (import.meta.env.DEV) {
      // Inject mock data so UI is visible in the browser
      window.initializePlayer(JSON.stringify({
        name: 'Sole Survivor',
        health: 85,
        maxHealth: 100,
      }));
    }
  }, []);

  return <PlayerHUD />;
};
```

---

## Key Rules

| Rule | Why |
|------|-----|
| Register `window.*` at module load, not in `useEffect` | F4SE may call before components mount |
| Import your store module before `ReactDOM.render` | Ensures functions exist at DOM ready |
| `RegisterJSListener` can be called before `OnDomReady` | It is C++-side only, no JS context needed |
| `Invoke` must wait for `OnDomReady` | Requires a live JS context |
| Use `InteropCall` for high-frequency updates | Better performance than `Invoke` for rapid stat updates |
| Use `Invoke` for initial data and complex JSON | More flexible, returns a result |
| One view per plugin | Manage multiple screens via JS routing inside the view |
