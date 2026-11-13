#ifndef EXPANSIONENGINETESTS_HPP_
#define EXPANSIONENGINETESTS_HPP_

#include <cxxtest/TestSuite.h>
#include <filesystem>
#include <fstream>
#include <unistd.h>

#include "common/core/GameTime.hpp"
#include "common/core/Registry.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerWorld.hpp"
#include "server/world/WorldController.hpp"
#include "server/world/save/WorldSaveBasicBackend.hpp"

class ExpansionEngineTests : public CxxTest::TestSuite {
	static void registerAir(Registry &definitions) {
		auto &air = definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0);
		air.isOpaque(false); air.isCollidable(false);
		definitions.registerItem<Item>({}, BLOCK_AIR, "air");
	}
	static void settle(ServerWorld &world) {
		for (unsigned pass = 0; pass < 100; ++pass) {
			bool pending = false;
			for (const auto &entry : world.chunks()) {
				if (!entry.second->isInitialized() || !entry.second->lightmap().hasPendingUpdates()) continue;
				entry.second->lightmap().updateLights(65536); pending = true;
			}
			if (!pending) return;
		}
	}
	public:
		void testEquivalentBlockReplacementsKeepSettledLighting() {
			Registry definitions; Registry::setInstance(definitions); registerAir(definitions);
			auto &stone = definitions.registerBlock<ServerBlock>("test:stone");
			auto &brick = definitions.registerBlock<ServerBlock>("test:brick");
			auto &glass = definitions.registerBlock<ServerBlock>("test:glass");
			glass.getState(0).isOpaque(false); glass.getState(0).drawType(BlockDrawType::Glass);
			auto &water = definitions.registerBlock<ServerBlock>("test:water");
			water.getState(0).isOpaque(false); water.getState(0).drawType(BlockDrawType::Liquid);
			PlayerList players; Dimension dimension{0, "test:world", "world"};
			ServerWorld world(players, dimension, 42);
			auto &chunk = world.getOrCreateChunk(0, 0, 0); chunk.setInitialized(true);
			chunk.setBlockRaw(8, 8, 8, stone.id()); chunk.lightmap().clearPendingUpdates();
			for (int i = 0; i < 100; ++i) chunk.setBlock(8, 8, 8, i % 2 ? stone.id() : brick.id());
			TS_ASSERT(!chunk.lightmap().hasPendingUpdates());
			chunk.setBlock(8, 8, 8, 0);
			TS_ASSERT(chunk.lightmap().hasPendingUpdates());
			chunk.lightmap().clearPendingUpdates();
			chunk.lightmap().setLightDataRaw(8, 8, 8, 0xf7);
			chunk.setBlock(8, 8, 8, glass.id());
			TS_ASSERT(!chunk.lightmap().hasPendingUpdates());
			TS_ASSERT_EQUALS(chunk.lightmap().getLightDataRaw(8, 8, 8), 0xf7);
			chunk.setBlock(8, 8, 8, water.id());
			TS_ASSERT(chunk.lightmap().hasPendingUpdates());
			TS_ASSERT_EQUALS(chunk.lightmap().getTorchlight(8, 8, 8), 7);
		}

		void testCaveCeilingsAndLoadedRoofsDoNotSeedSkylight() {
			Registry definitions; Registry::setInstance(definitions); registerAir(definitions);
			auto &stone = definitions.registerBlock<ServerBlock>("test:stone");
			PlayerList players; Dimension dimension{0, "test:world", "world"};
			ServerWorld world(players, dimension, 42);
			auto &terrain = world.heightmap().getOrCreateChunk(0, 0);
			terrain.blocks().fill(0);
			for (int x = 0; x < 16; ++x) for (int y = 0; y < 16; ++y) terrain.blocks()[(x * 16 + y) * 128 + 90] = 1;
			auto &cave = world.getOrCreateChunk(0, 0, 1); cave.setInitialized(true);
			world.initializeChunkLighting(cave); settle(world);
			TS_ASSERT_EQUALS(cave.lightmap().getSunlight(8, 8, 31), 0);
			TS_ASSERT_EQUALS(cave.lightmap().getSunlight(8, 8, 15), 0);
			auto &roof = world.getOrCreateChunk(0, 0, 5);
			for (int x = 0; x < 16; ++x) for (int y = 0; y < 16; ++y) roof.setBlockRaw(x, y, 0, stone.id());
			roof.setInitialized(true);
			auto &below = world.getOrCreateChunk(0, 0, 4); below.setInitialized(true);
			world.initializeChunkLighting(below); settle(world);
			TS_ASSERT_EQUALS(below.lightmap().getSunlight(8, 8, 31), 0);
		}

		void testLightQueryUsesLoadedChunksAndDaylight() {
			Registry definitions; Registry::setInstance(definitions); registerAir(definitions);
			PlayerList players; Dimension dimension{0, "test:world", "world"};
			ServerWorld world(players, dimension, 42);
			TS_ASSERT_EQUALS(world.getLight(8, 8, 128), -1);
			auto &sky = world.getOrCreateChunk(0, 0, 4);
			TS_ASSERT_EQUALS(world.getLight(8, 8, 128), -1);
			sky.setInitialized(true); world.initializeChunkLighting(sky); settle(world);
			u64 oldTicks = GameTime::getTicks();
			GameTime::setTicks(6000); TS_ASSERT_EQUALS(world.getLight(8, 8, 140), 15);
			GameTime::setTicks(18000); TS_ASSERT_EQUALS(world.getLight(8, 8, 140), 4);
			sky.lightmap().addTorchlight(8, 8, 12, 11);
			TS_ASSERT_EQUALS(world.getLight(8, 8, 140), 11);
			World::initUsertype(lua); lua["test_world"] = static_cast<World *>(&world);
			auto result = lua.safe_script("return test_world:get_light(8, 8, 140)", sol::script_pass_on_error);
			TS_ASSERT(result.valid()); if (result.valid()) TS_ASSERT_EQUALS(result.get<int>(), 11);
			GameTime::setTicks(oldTicks);
		}

		void testNetherAndWeakSourcesKeepTheirOwnLightLevels() {
			Registry definitions; Registry::setInstance(definitions); registerAir(definitions);
			auto &weak = definitions.registerBlock<ServerBlock>("test:weak");
			weak.getState(0).isLightSource(true); weak.getState(0).isOpaque(false); weak.addGroup("group:om_light_level", 4);
			auto &strong = definitions.registerBlock<ServerBlock>("test:strong"); strong.getState(0).isLightSource(true);
			PlayerList players; Dimension dimension{1, "test:nether", "nether"};
			ServerWorld world(players, dimension, 42); auto &chunk = world.getOrCreateChunk(0, 0, 3);
			chunk.setBlockRaw(8, 8, 15, weak.id()); chunk.setBlockRaw(10, 8, 15, strong.id()); chunk.setInitialized(true);
			chunk.lightmap().setSunlight(8, 8, 31, 15);
			world.initializeChunkLighting(chunk); settle(world);
			TS_ASSERT_EQUALS(chunk.lightmap().getSunlight(8, 8, 31), 0);
			TS_ASSERT_EQUALS(chunk.lightmap().getTorchlight(10, 8, 15), 14);
			chunk.setBlock(10, 8, 15, 0); settle(world);
			TS_ASSERT_EQUALS(chunk.lightmap().getTorchlight(8, 8, 15), 4);
			TS_ASSERT_EQUALS(chunk.lightmap().getTorchlight(9, 8, 15), 3);
			chunk.setBlock(8, 8, 15, 0); settle(world);
			TS_ASSERT_EQUALS(chunk.lightmap().getTorchlight(9, 8, 15), 0);
		}

		void testRawTickingBlocksAreBoundedAndCallbacksMayReplaceThem() {
			ScriptEngine scripts;
			Registry definitions; Registry::setInstance(definitions); registerAir(definitions);
			auto &crop = definitions.registerBlock<ServerBlock>("test:crop");
			PlayerList players; Dimension dimension{0, "test:world", "world"};
			ServerWorld world(players, dimension, 42); Server server; WorldController worlds(definitions);
			ServerCommandHandler commands(scripts, server, worlds, players, definitions); world.setServer(&commands);
			auto &chunk = world.getOrCreateChunk(0, 0, 0);
			std::set<int> visited;
			scripts.lua().set_function("tick", [&](const glm::ivec3 &pos) {
				visited.insert(pos.y * 16 + pos.x);
				world.setBlock(pos.x, pos.y, pos.z, 0);
				world.setBlock(15, 15, 1, crop.id());
			});
			crop.setOnTick(scripts.lua()["tick"]);
			for (int i = 0; i < 80; ++i) chunk.setBlockRaw(i % 16, i / 16, 1, crop.id());
			chunk.setInitialized(true); chunk.initializeTickingBlocks(); chunk.tick();
			TS_ASSERT_EQUALS(visited.size(), 64u);
			chunk.tick(); TS_ASSERT_EQUALS(visited.size(), 81u);
			TS_ASSERT_EQUALS(chunk.getBlock(0, 0, 1), 0);
		}

		void testInactiveChunksPauseAndResumeAroundThePlayer() {
			ScriptEngine scripts;
			Registry definitions; Registry::setInstance(definitions); registerAir(definitions);
			auto &crop = definitions.registerBlock<ServerBlock>("test:crop");
			unsigned ticks = 0;
			scripts.lua().set_function("tick", [&]() { ++ticks; }); crop.setOnTick(scripts.lua()["tick"]);
			PlayerList players; Dimension dimension{0, "test:world", "world"}; ServerWorld world(players, dimension, 42);
			Server server; WorldController worlds(definitions); ServerCommandHandler commands(scripts, server, worlds, players, definitions);
			world.setServer(&commands);
			auto &chunk = world.getOrCreateChunk(10, 0, 0); chunk.setBlockRaw(1, 1, 1, crop.id());
			chunk.setInitialized(true); chunk.initializeTickingBlocks();
			world.tickActiveChunks(); TS_ASSERT_EQUALS(ticks, 0u);
			ClientInfo client{1, Network::LocalSession::connect()}; auto *player = players.connectPlayer("tester", client);
			TS_ASSERT(player); if (!player) return;
			player->setReady(true); player->setPosition(1, 1, 1);
			world.tickActiveChunks(); TS_ASSERT_EQUALS(ticks, 0u);
			player->setPosition(161, 1, 1); world.tickActiveChunks(); TS_ASSERT_EQUALS(ticks, 1u);
			players.disconnectPlayer("tester"); world.tickActiveChunks(); TS_ASSERT_EQUALS(ticks, 1u);
		}

		void testGenerationRevisionPersistsAndLegacyFilesKeepTheirTerrain() {
			namespace fs = std::filesystem;
			struct RestoreDirectory { fs::path original = fs::current_path(); ~RestoreDirectory() { fs::current_path(original); } } restore;
			fs::path directory = fs::temp_directory_path() / ("oldworld-revision-test-" + std::to_string(getpid()));
			fs::create_directories(directory); fs::current_path(directory);
			Registry definitions; Registry::setInstance(definitions); registerAir(definitions);
			PlayerList players; Dimension dimension{0, "test:world", "world"}; std::deque<ServerWorld> worlds;
			worlds.emplace_back(players, dimension, 42); WorldSaveBasicBackend backend(worlds, players);
			TS_ASSERT_EQUALS(worlds.front().generationRevision(), 1);
			backend.save("expansion"); worlds.front().setGenerationRevision(0); backend.load("expansion");
			TS_ASSERT_EQUALS(worlds.front().generationRevision(), 1);
			Network::Packet legacy; legacy << u32(0) << s32(42) << u32(0) << u16(0) << sf::Uint64(6000);
			std::ofstream file("saves/legacy.dat", std::ios::binary); file.write(static_cast<const char *>(legacy.getData()), legacy.getDataSize()); file.close();
			backend.load("legacy"); TS_ASSERT_EQUALS(worlds.front().generationRevision(), 0);
			backend.save("legacy-resaved"); worlds.front().setGenerationRevision(1); backend.load("legacy-resaved");
			TS_ASSERT_EQUALS(worlds.front().generationRevision(), 0);
		}
	private:
		sol::state lua;
};

#endif
