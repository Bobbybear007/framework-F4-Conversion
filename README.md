# PrismaUI_F4

**PrismaUI_F4** is an F4SE plugin that embeds the [Ultralight](https://ultralig.ht/) WebKit renderer into Fallout 4. It lets mod authors create fully interactive HTML/CSS/JavaScript overlays - menus, HUDs, settings panels, terminals - without touching Scaleform or the game's native UI system.

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
3. Create a view (starts visible - hide it immediately):
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
| [docs/getting-started.md](docs/getting-started.md) | Full tutorial: project setup, xmake, first view, deploy |
| [docs/api-reference.md](docs/api-reference.md) | Complete C++ API - every method with parameters and return values |
| [docs/html-views.md](docs/html-views.md) | Writing HTML/CSS/JS for Ultralight - supported APIs, known gaps, JS↔C++ bridge |
| [docs/view-lifecycle.md](docs/view-lifecycle.md) | View states, focus model, ordering, inspector |
| [docs/examples.md](docs/examples.md) | Annotated working code - toggle menu, data push, JS events, multi-view |

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

## Building the Framework

PrismaUI_F4 uses **xmake** for compilation and supports **dual-targeting** for both Old-Gen (1.10.163) and Next-Gen (1.10.984) versions of Fallout 4.

1. Double-click `build-and-deploy.bat` in the root folder.
2. The script will prompt: `Build for Old-Gen (OG) or Next-Gen (NG)? [OG/NG]:`
3. Type `OG` or `NG` based on your target and hit Enter. The script will automatically configure the correct `CommonLibF4` offsets and build the framework.
4. Enter your Mod Organizer 2 mods path (or game data folder) when prompted to automatically deploy the DLL and UI assets.

## Building the Example Plugin

We provide a complete, working reference plugin in the [example-f4se-plugin/](example-f4se-plugin) directory that demonstrates standard integrations (bridges, console logging, view creation).

To build and deploy the example plugin:
1. Navigate to the `example-f4se-plugin/` directory.
2. Double-click the `build-and-deploy.bat` script inside that folder.
3. Choose `OG` (Old-Gen) or `NG` (Next-Gen) to compile the appropriate plugin DLL.
4. Enter your Mod Organizer 2 mods path (or game data folder) when prompted to automatically install the compiled plugin DLL and the universal HTML/CSS/JS UI assets.
5. Mod authors can easily use this folder as a boilerplate/template to start building their own custom PrismaUI mods.

## Requirements

- Fallout 4 runtime (Old-Gen 1.10.163 or Next-Gen 1.10.984)
- F4SE
- C++23 compiler (MSVC 2022 recommended)
- xmake 3.0+

## Credits and License

This project is a Fallout 4 (F4SE) conversion of the original **Prisma UI** framework
by the Prisma UI team, used with their permission. The original SKSE/Skyrim framework
lives at https://github.com/PrismaUI-SKSE/framework.

This software is distributed under the **Prisma UI License** (see [LICENSE.md](LICENSE.md)).
It is NOT MIT and NOT open source. In short:

- You may use it for non-commercial or limited commercial use, subject to the revenue and
  funding limits in the license.
- You may study it and modify it for private use.
- You may redistribute the original version only if this license is included without modification.
- You may NOT publicly distribute a modified version without written permission from the author.

### Ultralight dependency

Prisma UI embeds the **Ultralight** SDK, a commercial, source-available renderer owned by
Ultralight, Inc. Ultralight is NOT included in this repository and must be obtained separately
from https://ultralig.ht. Ultralight has its own terms, including no reverse engineering,
no static linking, and the same revenue and funding limits. Any binary release that ships the
Ultralight DLLs must include Ultralight's LICENSE.txt, EULA.txt, and NOTICES file alongside them.
