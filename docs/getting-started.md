# Getting Started with PrismaUI_F4

This guide walks you from an empty folder to a working F4SE plugin that opens an HTML overlay when you press a key.

## Prerequisites

- **Visual Studio 2022** with the C++ Desktop workload
- **xmake 3.0+**
- **[CommonLibF4 (Dear-Modding-FO4 Fork)](https://github.com/Dear-Modding-FO4/CommonLibF4)** - The modern xmake-based fork of CommonLibF4. You only need to download/submodule this single library, which fully supports both **Old-Gen** and **Next-Gen** builds.
- **PrismaUI_F4** installed (`PrismaUI_F4.dll` + Ultralight libs in your MO2 mod)
- **Fallout 4 + F4SE** (Old-Gen 1.10.163 or Next-Gen 1.10.984)

**For a complete working reference, see the example plugin:** `example-f4se-plugin\`. It demonstrates all core PrismaUI features (Papyrus Bridge, C++ Bridge, Event Log, Tutorial) and includes an automated `build-and-deploy.bat` to handle dual-targeting.

---

## 1. Project Structure

A typical PrismaUI plugin structure looks like this:

```text
MyPlugin_F4/
├── src/
│   ├── PCH.h
│   ├── main.cpp
│   └── PrismaUI_F4_API.h       ← copy from PrismaUI_F4 framework
├── view/                       ← Your HTML/CSS/JS files
│   └── index.html
├── xmake.lua                   ← Your build configuration
└── build-and-deploy.bat        ← The build script
```

---

## 2. xmake.lua (Build Configuration)

Instead of CMake, we use `xmake` for a clean, modern build experience.

```lua
-- MyPlugin_F4/xmake.lua
includes("lib/commonlibf4") -- Path to your CommonLibF4 submodule

target("MyPlugin_F4")
    set_kind("shared")
    set_languages("c++23")
    set_filename("MyPlugin_F4.dll")

    add_rules("commonlibf4.plugin", {
        name    = "MyPlugin_F4",
        author  = "YourName",
        version = "1.0.0"
    })

    add_includedirs("src")
    add_files("src/**.cpp")
    set_pcxxheader("src/PCH.h")
    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")

    if is_plat("windows") then
        add_cxflags("/permissive-", "/wd4200", "/wd4201", "/wd4324")
        add_syslinks("Version", "Ole32", "OleAut32", "User32", "bcrypt", "crypt32")
    end
```

---

## 3. Dual-Targeting: build-and-deploy.bat

Because Fallout 4's memory offsets changed completely between Old-Gen (1.10.163) and Next-Gen (1.10.984), **you must compile two different versions of your DLL**.

We use an automated `.bat` script that configures an environment variable (`PRISMA_TARGET`) and tells `xmake` which `CommonLibF4` version to link against.

```bat
@echo off
setlocal enabledelayedexpansion

echo ========================================
echo   Building MyPlugin_F4...
echo ========================================
echo.
set /p TARGET_VER="Build for Old-Gen (OG) or Next-Gen (NG)? [OG/NG]: "
if /i "!TARGET_VER!"=="OG" (
    set PRISMA_TARGET=og
) else (
    set PRISMA_TARGET=ng
)
echo Building for !TARGET_VER! ...

xmake f -c
xmake
if errorlevel 1 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo.
set /p DEPLOY_PATH="Enter deployment path (e.g., MO2 Mods folder): "
copy /Y "build\windows\x64\release\MyPlugin_F4.dll" "!DEPLOY_PATH!\F4SE\plugins\MyPlugin_F4.dll"
xcopy /Y /E "view\*" "!DEPLOY_PATH!\PrismaUI_F4\views\MyPlugin_F4\"
echo Deployment Complete!
pause
```

---

## 4. PCH.h

```cpp
#pragma once

#include <RE/Fallout.h>
#include <F4SE/F4SE.h>
#include <F4SE/Impl/WinAPI.h>

#include <spdlog/spdlog.h>

namespace logger = F4SE::log;

using namespace std::literals;
```

---

## 5. main.cpp

Your C++ file handles initializing F4SE and loading your UI view when the game is ready.

```cpp
#include "PrismaUI_F4_API.h"
#include <spdlog/sinks/basic_file_sink.h>

static PRISMA_UI_API::IVPrismaUI3* g_api  = nullptr;
static PrismaView                   g_view = 0;
static bool                         g_visible = false;

static void OnDomReady(PrismaView /*view*/)
{
    logger::info("MyPlugin: DOM ready");
}

static void F4SEMessageHandler(F4SE::MessagingInterface::Message* msg)
{
    if (!msg) return;
    switch (msg->type) {
    case F4SE::MessagingInterface::kGameDataReady:
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI3>();
        if (!g_api) { logger::error("MyPlugin: PrismaUI V3 not found"); return; }
        break;
    case F4SE::MessagingInterface::kPostLoadGame:
    case F4SE::MessagingInterface::kNewGame:
        if (g_api && g_view == 0) {
            g_view = g_api->CreateView("MyPlugin_F4/index.html", OnDomReady);
            g_api->RegisterConsoleCallback(g_view,
                [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel, const char* msg) {
                    logger::info("[JS] {}", msg);
                });
        }
        break;
    }
}

F4SE_PLUGIN_LOAD(const F4SE::LoadInterface* a_intfc)
{
    F4SE::Init(a_intfc);
    const auto* messaging = F4SE::GetMessagingInterface();
    if (!messaging) return false;
    messaging->RegisterListener(F4SEMessageHandler);
    return true;
}
```

---

## 6. index.html

```html
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<style>
  * { margin:0; padding:0; box-sizing:border-box; }
  body {
    width:100vw; height:100vh; background:rgba(0,0,0,0.75);
    display:flex; align-items:center; justify-content:center;
    font-family:'Courier New',monospace; color:#00ff41;
  }
  .panel {
    background:#000; border:1px solid #00661a;
    padding:32px 40px; text-align:center;
  }
  h1 { font-size:20px; letter-spacing:3px; margin-bottom:16px; }
  p  { font-size:12px; color:#009921; }
</style>
</head>
<body>
<div class="panel">
  <h1 id="title">MY MENU</h1>
  <p id="msg">Press the key to close</p>
</div>
<script>
  console.log('mymenu loaded');
</script>
</body>
</html>
```

---

## 7. Build and Deploy

1. Run `build-and-deploy.bat`.
2. Type `OG` (Old-Gen) or `NG` (Next-Gen) when prompted.
3. Provide your MO2 Mods path when prompted to automatically deploy the DLL and HTML files.

**CRITICAL:** Never `Remove-Item` a mod folder. MO2 mod folders contain pre-existing runtime files that are not tracked. Always copy individual files.

---

## 8. Load Order

PrismaUI_F4 must be loaded before your plugin. To be safe, always request the API during `kGameDataReady` (not `kPostLoad`), which fires after all plugins are initialized.

---

## 9. Debugging

Logs: `%USERPROFILE%\Documents\My Games\Fallout4\F4SE\MyPlugin_F4.log`

Register `RegisterConsoleCallback` to see JS `console.*` output in your log. This is the primary JS debugging tool.

For DOM inspection during development:

```cpp
#ifdef PRISMA_DEBUG
api->CreateInspectorView(g_view);
api->SetInspectorBounds(g_view, 10.0f, 10.0f, 900, 560);
api->SetInspectorVisibility(g_view, true);
#endif
```

Remove all `CreateInspectorView` calls before releasing.
