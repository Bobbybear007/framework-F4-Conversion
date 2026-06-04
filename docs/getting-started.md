# Getting Started with PrismaUI_F4

This guide walks you from an empty folder to a working F4SE plugin that opens an HTML overlay when you press a key.

## Prerequisites

- **Visual Studio 2022** with the C++ Desktop workload and C++ CMake tools component
- **CMake ≥ 3.21**
- **vcpkg** bootstrapped and `VCPKG_ROOT` or `VCPKG_INSTALLATION_ROOT` set
- **CommonLibF4** — source at `E:\F4SE OG\CommonLibF4`
- **PrismaUI_F4** installed (`PrismaUI_F4.dll` + Ultralight libs in your MO2 mod)
- **Fallout 4 + F4SE 0.7.1+** (Next-Gen)
- **Address Library for F4SE Plugins** — the `.bin` file matching your game version in `Data\F4SE\Plugins\`

The easiest starting point is to clone `PrismaInventory_F4` or `PrismaDebugger_F4` and adapt them. Their `cmake/` folders and `CMakeLists.txt` are already wired for CommonLibF4 + vcpkg.

---

## 1. Project Structure

```
MyPlugin_F4/
├── cmake/
│   ├── commonlibf4.cmake
│   └── CompilerFlags.cmake
├── src/
│   ├── PCH.h
│   ├── main.cpp
│   └── PrismaUI_F4_API.h       ← copy from PrismaUI_F4 New Gen/src/
├── assets/
│   └── views/
│       └── mymenu.html
├── CMakeLists.txt
├── vcpkg.json
└── build.bat
```

---

## 2. CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.21)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
project(MyPlugin_F4 VERSION 1.0.0 LANGUAGES CXX)

if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds not allowed.")
endif()

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CompilerFlags)
include(commonlibf4)
find_package(spdlog CONFIG REQUIRED)

file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")
add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibF4::CommonLibF4 spdlog::spdlog bcrypt version)
target_precompile_headers(${PROJECT_NAME} PRIVATE "src/PCH.h")

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

set(DIST_DIR "${CMAKE_SOURCE_DIR}/dist/MyPlugin_F4_${PROJECT_VERSION}")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/F4SE/plugins"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/PrismaUI_F4/views"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${DIST_DIR}/F4SE/plugins/"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/assets/views/mymenu.html" "${DIST_DIR}/PrismaUI_F4/views/"
)
```

---

## 3. vcpkg.json

```json
{
  "name": "myplugin-f4",
  "version-string": "1.0.0",
  "dependencies": [
    "spdlog",
    "vcpkg-cmake-config"
  ]
}
```

---

## 4. build.bat

```bat
@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.44 >nul 2>&1
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DVCPKG_TARGET_TRIPLET=x64-windows-static -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded
if errorlevel 1 exit /b 1
cmake --build build/release --config Release --target MyPlugin_F4
```

---

## 5. PCH.h

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

## 6. main.cpp

There are two F4SE plugin registration styles. **Use both** — they coexist in the same DLL and cover old and new F4SE versions.

```cpp
#include "PrismaUI_F4_API.h"
#include <spdlog/sinks/basic_file_sink.h>

static PRISMA_UI_API::IVPrismaUI3* g_api  = nullptr;
static PrismaView                   g_view = 0;
static bool                         g_visible = false;

// ── Modern F4SE 0.7.1+ registration (Next-Gen) ───────────────────────────────
// g_pluginVersionData is read by F4SE before calling F4SEPlugin_Load.
// Empty CompatibleVersions = accept any runtime version.
// UsesAddressLibrary = require a matching .bin file in Data\F4SE\Plugins\.
extern "C" DLLEXPORT constinit F4SE::PluginVersionData g_pluginVersionData = []() {
    F4SE::PluginVersionData data{};
    data.PluginVersion(REL::Version{ 1, 0, 0, 0 });
    data.PluginName("MyPlugin_F4");
    data.AuthorName("YourName");
    data.UsesAddressLibrary(true);
    data.IsLayoutDependent(true);
    return data;
}();

static void OnDomReady(PrismaView /*view*/)
{
    logger::info("MyPlugin: DOM ready");
    // Register JS→C++ listeners here
}

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

static void F4SEMessageHandler(F4SE::MessagingInterface::Message* msg)
{
    if (!msg) return;
    switch (msg->type) {
    case F4SE::MessagingInterface::kGameDataReady:
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI3>();
        if (!g_api) { logger::error("MyPlugin: PrismaUI V3 not found"); return; }
        // Register key handler here
        break;
    case F4SE::MessagingInterface::kPostLoadGame:
    case F4SE::MessagingInterface::kNewGame:
        if (g_api && g_view == 0) {
            g_view = g_api->CreateView("mymenu.html", OnDomReady);
            g_api->RegisterConsoleCallback(g_view,
                [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel, const char* msg) {
                    logger::info("[JS] {}", msg);
                });
            g_api->RegisterTranslations(g_view, "MyPlugin_F4");
        }
        break;
    }
}

// ── Legacy F4SE registration (pre-0.7.1, pre-Next-Gen) ───────────────────────
// Old F4SE ignores g_pluginVersionData and uses these exports instead.
extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
    auto path = logger::log_directory();
    if (!path) return false;
    *path /= "MyPlugin_F4.log";
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
    auto log  = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);
    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%Y-%m-%d %T.%e] [%l] [%t] [%s:%#] %v");

    logger::info("MyPlugin_F4 v1.0.0");
    a_info->infoVersion = F4SE::PluginInfo::kVersion;
    a_info->name        = "MyPlugin_F4";
    a_info->version     = 1;

    if (a_f4se->IsEditor()) return false;
    if (a_f4se->RuntimeVersion() < F4SE::RUNTIME_1_10_162) return false;
    return true;
}

extern "C" DLLEXPORT bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
    F4SE::Init(a_f4se);
    const auto* messaging = F4SE::GetMessagingInterface();
    if (!messaging) return false;
    messaging->RegisterListener(F4SEMessageHandler);
    return true;
}
```

---

## 7. mymenu.html

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

  function updateMessage(text) {
    document.getElementById('msg').textContent = text;
  }
</script>
</body>
</html>
```

---

## 8. Build and Deploy

```powershell
# Build
.\build.bat

# Deploy — always targeted Copy-Item, never Remove-Item on mod folders
$dist = ".\dist\MyPlugin_F4_1.0.0"
$mo2  = "E:\Modlists\<your-list>\mods\MyPlugin_F4"

New-Item -ItemType Directory -Force "$mo2\F4SE\plugins" | Out-Null
New-Item -ItemType Directory -Force "$mo2\PrismaUI_F4\views" | Out-Null

Copy-Item "$dist\F4SE\plugins\MyPlugin_F4.dll"      "$mo2\F4SE\plugins\" -Force
Copy-Item "$dist\PrismaUI_F4\views\mymenu.html"     "$mo2\PrismaUI_F4\views\" -Force
```

**CRITICAL:** Never `Remove-Item` a mod folder or any subfolder inside `mods/`. MO2 mod folders contain pre-existing runtime files that are not tracked in git. Always copy individual files with `-Force`.

---

## 9. MO2 Mod Folder Structure

MO2 treats the mod folder as the `Data/` root.

```
mods/MyPlugin_F4/
├── F4SE/plugins/
│   └── MyPlugin_F4.dll
└── PrismaUI_F4/
    └── views/
        └── mymenu.html
```

---

## 10. Load Order

PrismaUI_F4 must be loaded before your plugin. F4SE loads plugins alphabetically; to be safe, always request the API during `kGameDataReady` (not `kPostLoad`), which fires after all plugins are initialized.

---

## 11. Debugging

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
