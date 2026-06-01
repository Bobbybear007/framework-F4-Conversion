#pragma once

// Ultralight DLLs are loaded from Data/PrismaUI_F4/libs/ at runtime by DllLoader.
// DELAYLOAD prevents Windows resolving them from PATH at plugin load time.
#pragma comment(linker, "/DELAYLOAD:AppCore.dll")
#pragma comment(linker, "/DELAYLOAD:Ultralight.dll")
#pragma comment(linker, "/DELAYLOAD:UltralightCore.dll")
#pragma comment(linker, "/DELAYLOAD:WebCore.dll")
#pragma comment(lib, "delayimp.lib")

#pragma warning(push)
#include <RE/Fallout.h>
#include <F4SE/F4SE.h>
#include <spdlog/spdlog.h>
#pragma warning(pop)

using namespace std::literals;

// NewCommonLib (xmake) removed F4SE::log. spdlog is a public dependency of
// commonlib-shared and exposes the same free-function API (info/warn/error/critical).
namespace logger = spdlog;

#define DLLEXPORT __declspec(dllexport)
