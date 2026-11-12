#ifndef WORLDSTARTUPTESTS_HPP_
#define WORLDSTARTUPTESTS_HPP_

#include <cxxtest/TestSuite.h>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unistd.h>
#include "common/core/EventHandler.hpp"
#include "common/core/LoggerHandler.hpp"
#include "common/core/Registry.hpp"
#include "common/event/Events.hpp"
#include "server/core/ServerApplication.hpp"

class WorldStartupTests : public CxxTest::TestSuite {
public:
	void testUnknownMobReturnsFailureInsteadOfTerminatingTheSimulationThread() {
		namespace fs = std::filesystem;
		struct Restore { fs::path original = fs::current_path(); ~Restore() { fs::current_path(original); } } restore;
		auto directory = fs::temp_directory_path() / ("opencraft-startup-test-" + std::to_string(getpid()));
		fs::create_directories(directory / "mods/broken");
		fs::create_directories(directory / "config");
		fs::create_directories(directory / "saves");
		std::ofstream(directory / "mods/broken/config.lua") << "mod_config = {id = 'broken', dependencies = {}}";
		std::ofstream(directory / "mods/broken/init.lua") <<
			"local mod = opencraft.mod_loader:register_mod('broken')\n"
			"mod:entity {id='test', properties={visual={type='retro_mob', model='missing_model'}}}\n";
		std::ofstream(directory / "saves/preserved.dat") << "unchanged";
		fs::current_path(directory);
		EventHandler events;
		unsigned failures = 0, successes = 0;
		events.addListener<ServerOnlineEvent>([&](const auto &event) { if (event.isOnline) ++successes; else ++failures; });
		int result = 0;
		std::thread simulation([&] {
			ServerApplication app(events);
			app.setWorldName("preserved");
			result = app.run();
		});
		simulation.join();
		events.processEvents();
		TS_ASSERT_EQUALS(result, 1);
		TS_ASSERT_EQUALS(failures, 1u);
		TS_ASSERT_EQUALS(successes, 0u);
		std::ifstream save(directory / "saves/preserved.dat");
		std::string content; save >> content;
		TS_ASSERT_EQUALS(content, "unchanged");
		std::ifstream log(directory / "logs/opencraft_server.log");
		std::string logged((std::istreambuf_iterator<char>(log)), std::istreambuf_iterator<char>());
		TS_ASSERT(logged.find("missing_model") != std::string::npos);
		TS_ASSERT(logged.find("broken:test") != std::string::npos);
	}
};

#endif
