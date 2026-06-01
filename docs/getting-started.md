# Getting Started with PrismaUI_F4

This guide walks you from an empty folder to a working F4SE plugin that opens an HTML overlay when you press a key. It takes about 30 minutes if you have the prerequisites installed.

## Prerequisites

- **Visual Studio 2022** with the C++ Desktop workload and C++ CMake tools component
- **CMake ≥ 3.21**
- **vcpkg** bootstrapped and `VCPKG_ROOT` or `VCPKG_INSTALLATION_ROOT` set in your environment
- **CommonLibF4** — source or pre-built; your plugin links against it
- **PrismaUI_F4** installed (the framework DLL plus `PrismaUI_F4_API.h`)
- **Fallout 4 + F4SE**

The easiest way to get CommonLibF4 and a working vcpkg environment is to clone an existing plugin like PrismaInventory_F4 and adapt it. Your cmake module folder (`cmake/`) contains `commonlibf4.cmake` which locates CommonLibF4; keep that file.

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
│   ├── KeyHandler.h
│   ├── KeyHandler.cpp
│   ├── PrismaUI_F4_API.h       ← copy from PrismaUI_F4/src/
│   └── PrismaUI_F4_Helper.h    ← optional; copy from PrismaUI_F4/src/
├── assets/
│   └── views/
│       └── mymenu.html
├── CMakeLists.txt
├── vcpkg.json
└── _configure_and_build.bat
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

# Copy DLL to dist folder after build
set(DIST_DIR "${CMAKE_SOURCE_DIR}/dist/MyPlugin_F4_${PROJECT_VERSION}")
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/F4SE/Plugins"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DIST_DIR}/PrismaUI_F4/views"
    COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:${PROJECT_NAME}>" "${DIST_DIR}/F4SE/Plugins/"
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

## 4. _configure_and_build.bat

```bat
@echo off
cmake -B build/release -S . ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^
  -DVCPKG_INSTALLED_DIR="E:\F4SE OG\PrismaUI_F4\build\release\vcpkg_installed"
cmake --build build/release --config Release
```

The `-DVCPKG_INSTALLED_DIR` points at PrismaUI_F4's already-built vcpkg tree so you don't need to compile spdlog and friends again. Adjust the path to wherever PrismaUI_F4 lives on your machine.

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

## 6. KeyHandler.h / KeyHandler.cpp

Copy these files verbatim from `PrismaInventory_F4/src/` or `PrismaShowcase_F4/src/`. They implement a thin wrapper around `RE::BSInputEventUser` for binding keydown/keyup callbacks to BS button codes.

**Common key codes:**
| Key | BS Code |
|-----|---------|
| F5  | 0x3F    |
| F6  | 0x40    |
| F7  | 0x41    |
| F8  | 0x77    |
| F9  | 0x43    |
| F10 | 0x44    |
| F11 | 0x57    |
| F12 | 0x58    |

---

## 7. main.cpp

```cpp
#include "PrismaUI_F4_API.h"
#include "PrismaUI_F4_Helper.h"  // optional — JSON helpers
#include "KeyHandler.h"
#include <spdlog/sinks/basic_file_sink.h>

static PRISMA_UI_API::IVPrismaUI4* g_api  = nullptr;
static PrismaView                   g_view = 0;
static bool                         g_visible = false;

static void OnDomReady(PrismaView view)
{
    g_api->RegisterConsoleCallback(view,
        [](PrismaView, PRISMA_UI_API::ConsoleMessageLevel lvl, const char* msg) {
            const char* tag = (lvl == PRISMA_UI_API::ConsoleMessageLevel::Error)  ? "[JS ERR] " :
                              (lvl == PRISMA_UI_API::ConsoleMessageLevel::Warning) ? "[JS WARN]" :
                                                                                     "[JS LOG] ";
            logger::info("{} {}", tag, msg);
        });

    // BindUIEvent — callback fires on game thread, RE:: access is safe directly
    g_api->BindUIEvent(view, "requestClose", [](const char*) {
        g_visible = false;
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    });

    g_api->Invoke(view, "init()");
    logger::info("MyPlugin: DOM ready (view={})", view);
}

static void Toggle()
{
    if (!g_api || !g_api->IsValid(g_view)) return;
    g_visible = !g_visible;
    if (g_visible) {
        g_api->Show(g_view);
        g_api->Focus(g_view, /*pauseGame=*/true);
    } else {
        g_api->Unfocus(g_view);
        g_api->Hide(g_view);
    }
}

static void CreateViews()
{
    if (g_view && g_api->IsValid(g_view)) return;
    g_view = g_api->CreateView("Interface/MyPlugin/mymenu.html", OnDomReady);
    // RegisterTranslations must be called here, before DOM ready (V3+)
    // g_api->RegisterTranslations(g_view, "MyPlugin_F4");
    g_api->Hide(g_view);
    logger::info("MyPlugin: view created (id={})", g_view);
}

static void F4SEMessageHandler(F4SE::MessagingInterface::Message* msg)
{
    switch (msg->type) {
    case F4SE::MessagingInterface::kGameDataReady:
        g_api = PRISMA_UI_API::RequestPluginAPI<PRISMA_UI_API::IVPrismaUI4>();
        if (!g_api) { logger::error("MyPlugin: PrismaUI_F4 not found — is it installed?"); return; }
        KeyHandler::RegisterSink();
        KeyHandler::GetSingleton()->Register(0x43, KeyEventType::KEY_DOWN, Toggle); // F9
        break;
    case F4SE::MessagingInterface::kPostLoadGame:
    case F4SE::MessagingInterface::kNewGame:
        if (g_api) CreateViews();
        break;
    }
}

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

## 8. mymenu.html

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
  <h1>MY MENU</h1>
  <p id="msg">Press F9 to close</p>
</div>
<script>
  console.log('mymenu loaded');
  // Called from C++:  api->Invoke(view, "updateMessage('Hello!')");
  function updateMessage(text) {
    document.getElementById('msg').textContent = text;
  }
</script>
</body>
</html>
```

---

## 9. Build and Deploy

```powershell
# Build
.\_configure_and_build.bat

# Deploy framework (if not already installed)
Copy-Item "E:\F4SE OG\PrismaUI_F4\build\release\bin\PrismaUI_F4.dll" `
    "E:\Modlists\<your-list>\mods\PrismaUI_F4\F4SE\Plugins\" -Force

# Deploy your plugin
$dist = "E:\F4SE OG\MyPlugin_F4\dist\MyPlugin_F4_1.0.0"
Copy-Item "$dist\F4SE\Plugins\MyPlugin_F4.dll" `
    "E:\Modlists\<your-list>\mods\MyPlugin_F4\F4SE\Plugins\" -Force
Copy-Item "$dist\PrismaUI_F4\views\mymenu.html" `
    "E:\Modlists\<your-list>\mods\MyPlugin_F4\PrismaUI_F4\views\" -Force
```

**CRITICAL — always edit HTML in source, then deploy.** Never edit files under `mods/` directly; the next build will overwrite them.

---

## 10. MO2 Mod Folder Structure

MO2 treats the mod folder as the `Data/` root. Do not create a `Data/` subfolder inside your mod folder.

```
mods/MyPlugin_F4/
├── F4SE/Plugins/
│   └── MyPlugin_F4.dll
└── PrismaUI_F4/
    └── views/
        └── mymenu.html
```

---

## Load Order

PrismaUI_F4 must appear before your plugin in the F4SE load order. F4SE loads plugins alphabetically by DLL name; plugins starting with `P` load before those starting with other letters but this is not guaranteed. To be safe, call `RequestPluginAPI` during `kGameDataReady` (not `kPostLoad`), which fires after all plugins are initialized.

---

## Debugging

Logs are written to:
```
%USERPROFILE%\Documents\My Games\Fallout4\F4SE\MyPlugin_F4.log
```

JavaScript `console.log()`, `console.warn()`, and `console.error()` calls appear in the log if you registered a `ConsoleMessageCallback` (see the `CreateViews` example above). Always register this callback during development.

Open the Ultralight inspector for live DOM debugging: see [docs/view-lifecycle.md](view-lifecycle.md#inspector).
