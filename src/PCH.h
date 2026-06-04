#pragma once

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
