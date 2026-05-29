# PrismaUI_F4

**PrismaUI_F4** is an F4SE plugin that embeds the [Ultralight](https://ultralig.ht/) WebKit renderer into Fallout 4. It lets mod authors create fully interactive HTML/CSS/JavaScript overlays — menus, HUDs, settings panels, terminals — without touching Scaleform or the game's native UI system.

## How It Works

PrismaUI_F4 hooks D3D11's `Present` call and composites one or more HTML views on top of every rendered frame. Each view is a full browser page rendered by Ultralight (Chromium-era WebKit). You write standard HTML/CSS/JS; the framework handles rendering, input routing, focus management, and cursor visibility.

## Architecture

```
Fallout 4 process
├── F4SE
│   └── PrismaUI_F4.dll          ← framework (you never modify this)
│       ├── Ultralight renderer  ← WebKit, single dedicated thread
│       ├── D3D11 Present hook   ← composites views each frame
│       ├── FocusMenu            ← Scaleform menu that owns the cursor
│       └── Public API           ← IVPrismaUI1 / IVPrismaUI2
│
└── YourPlugin.dll               ← your F4SE plugin
    ├── PrismaUI_F4_API.h        ← single header, copy into your project
    └── main.cpp                 ← request API, create views, wire hotkeys
```

## Quick Start

1. Copy `PrismaUI_F4_API.h` into your plugin's `src/` folder.
2. Request the API on `kGameDataReady`:
   ```cpp
   auto* api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI2>();
   ```
3. Create a view (starts visible — hide it immediately):
   ```cpp
   PrismaView view = api->CreateView("mymenu.html", OnDomReady);
   api->Hide(view);  // views are visible by default
   ```
4. Place your HTML file at `Data/PrismaUI_F4/views/mymenu.html`.
5. Show, focus, hide from a key handler.

See [docs/getting-started.md](docs/getting-started.md) for the full walkthrough.

## Documentation

| Document | Description |
|---|---|
| [docs/getting-started.md](docs/getting-started.md) | Full tutorial: project setup, CMake, first view, deploy |
| [docs/api-reference.md](docs/api-reference.md) | Complete C++ API — every method with parameters and return values |
| [docs/html-views.md](docs/html-views.md) | Writing HTML/CSS/JS for Ultralight — supported APIs, known gaps, JS↔C++ bridge |
| [docs/view-lifecycle.md](docs/view-lifecycle.md) | View states, focus model, ordering, inspector |
| [docs/examples.md](docs/examples.md) | Annotated working code — toggle menu, data push, JS events, multi-view |

## File Layout (installed)

```
Data/
├── F4SE/Plugins/
│   └── PrismaUI_F4.dll
└── PrismaUI_F4/
    ├── libs/                ← Ultralight DLLs (shipped with framework)
    │   ├── UltralightCore.dll
    │   ├── WebCore.dll
    │   ├── Ultralight.dll
    │   └── AppCore.dll
    ├── resources/           ← Ultralight built-in resources
    └── views/               ← all HTML files from all consumer plugins land here
        ├── inventory.html   (from PrismaInventory_F4)
        └── mymenu.html      (from your plugin)
```

MO2 merges every mod's `PrismaUI_F4/views/` folder into one virtual directory. View filenames must be unique across all mods.

## Requirements

- Fallout 4 runtime ≥ 1.10.162
- F4SE
- C++23 compiler (MSVC 2022 recommended) for consumer plugins
- CommonLibF4
- vcpkg (spdlog, bcrypt, version)
