#include "PCH.h"

namespace {
	std::shared_ptr<spdlog::logger> log;

	void InitializeLog() {
		auto path = std::string("Data/F4SE/Plugins/PrismaUI_F4.log");
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path, true);
		auto logger_ptr = std::make_shared<spdlog::logger>("global log", std::move(sink));
		log = logger_ptr;
		log->set_level(spdlog::level::trace);
		log->flush_on(spdlog::level::trace);

		spdlog::register_logger(log);
		spdlog::set_default_logger(log);
	}
}

extern "C" {
	bool F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info) {
		InitializeLog();

		if (a_f4se->IsEditor()) {
			logger::critical("Loaded in editor, marking incompatible!");
			return false;
		}

		const auto ver = a_f4se->RuntimeVersion();
		if (ver < F4SE::RUNTIME_1_10_162) {
			logger::critical("Unsupported runtime version {}", ver);
			return false;
		}

		return true;
	}

	bool F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se) {
		F4SE::Init(a_f4se);
		logger::info("PrismaUI_F4 Framework loaded successfully");
		return true;
	}
};
