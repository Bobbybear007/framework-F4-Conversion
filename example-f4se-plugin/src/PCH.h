// example-f4se-plugin/src/PCH.h
#pragma once

#pragma warning(push)
#include <RE/Fallout.h>
#include <F4SE/F4SE.h>
#pragma warning(pop)

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>

using namespace std::literals;
namespace logger = F4SE::log;

#define DLLEXPORT __declspec(dllexport)
