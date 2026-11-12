#ifndef BEDSCLOCKTESTS_HPP_
#define BEDSCLOCKTESTS_HPP_

#include <cxxtest/TestSuite.h>
#include "server/world/save/ChunkDatabase.hpp"
#include <filesystem>
#include <fstream>
#include <set>
#include <unistd.h>

#include "common/core/GameClock.hpp"
#include "common/core/GameTime.hpp"
#include "common/core/LoggerHandler.hpp"
#include "common/core/Registry.hpp"
#include "common/scene/component/MobComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/world/ClockAnimation.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/lua/ServerModLoader.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/WorldController.hpp"

class BedsClockTests : public CxxTest::TestSuite {
	struct Fixture {
		std::filesystem::path original = std::filesystem::current_path();
		ScriptEngine scripts;
		Registry definitions;
		PlayerList players;
		WorldController worlds{definitions};
		ServerModLoader loader{scripts, worlds};
		Server server;
		ServerCommandHandler commands{scripts, server, worlds, players, definitions};
		ClientInfo client{1, {}};
		ServerPlayer *player;
		Fixture() {
			std::filesystem::current_path(OM_TEST_SOURCE_DIR);
			Registry::setInstance(definitions);
			auto &air = definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0);
			air.isOpaque(false); air.isCollidable(false);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air").setIsBlock(true);
			loader.loadMods(); scripts.luaCore().setRegistry(&definitions);
			server.init();
			client = ClientInfo{1, Network::LocalSession::connect()};
			worlds.setServer(commands); worlds.init(players, 42);
			player = players.connectPlayer("Player", client);
			player->setReady(true); player->setPosition(8.5, 6.5, 2);
			player->setRotation(0, 0);
			flat(worlds.getWorld(0));
		}
		~Fixture() { std::filesystem::current_path(original); }
		void flat(ServerWorld &world, int cx = 0) {
			auto &chunk = world.getOrCreateChunk(cx, 0, 0);
			u16 stone = definitions.getBlockFromStringID("default:stone").id();
			for (int z = 0; z < CHUNK_HEIGHT; ++z) for (int y = 0; y < CHUNK_DEPTH; ++y)
				for (int x = 0; x < CHUNK_WIDTH; ++x) chunk.setBlockRaw(x, y, z, z < 2 ? stone : 0);
			chunk.setInitialized(true);
		}
	};

	public:
		void setUp() override {
			static LoggerHandler logger; LoggerHandler::setInstance(logger);
			static GameClock clock; GameClock::setInstance(clock);
		}

		void testBedRotationPairRemovalAndLoadedBoundaryPlacement() {
			Fixture f; auto &world = f.worlds.getWorld(0);
			constexpr Vector3i directions[] = {{1,0,0},{0,1,0},{-1,0,0},{0,-1,0}};
			for (unsigned rotation = 0; rotation < 4; ++rotation) {
				f.player->setRotation(float(rotation * 90), 0);
				u16 data = 0;
				TS_ASSERT(world.placeBed(*f.player, {8,8,2}, data));
				Vector3i head = Vector3i{8,8,2} + directions[rotation];
				const auto *state = world.getBlockState(head.x, head.y, head.z);
				TS_ASSERT_EQUALS(state->id(), 1);
				TS_ASSERT_EQUALS(state->getItemDrops().front().stringID(), "default:bed");
				world.setBlock(head.x, head.y, head.z, 0);
				TS_ASSERT_EQUALS(world.getBlock(8,8,2), 0);
			}
			f.player->setRotation(0,0); u16 data = 0;
			TS_ASSERT(!world.placeBed(*f.player, {15,8,2}, data));
			TS_ASSERT_EQUALS(world.getBlock(15,8,2), 0);
			f.flat(world,1);
			TS_ASSERT(world.placeBed(*f.player, {15,8,2}, data));
			world.setBlock(16,8,1,0);
			world.blockPhysics().enqueueNeighbors({16,8,1});
			for (unsigned tick = 0; tick < 8; ++tick) world.blockPhysics().tick(4096);
			TS_ASSERT_EQUALS(world.getBlock(15,8,2),0);
			TS_ASSERT_EQUALS(world.getBlock(16,8,2),0);
			unsigned drops = 0;
			world.scene().registry().view<ItemStack>().each([&](auto, const auto &stack) {
				if (stack.stringID() == "default:bed") drops += stack.amount();
			});
			TS_ASSERT_EQUALS(drops, 1);
		}

		void testNightSleepGuardsRespawnAndNetherExplosion() {
			Fixture f; auto &world = f.worlds.getWorld(0); u16 data = 0;
			TS_ASSERT(world.placeBed(*f.player, {8,8,2}, data));
			GameTime::setTicks(6000);
			TS_ASSERT_EQUALS(world.sleepInBed(*f.player,{8,8,2}), "you can only sleep at night");
			TS_ASSERT(!f.player->bedSpawn());
			GameTime::setTicks(14000);
			auto &registry = world.scene().registry();
			auto monster = registry.create();
			registry.emplace<MobComponent>(monster).hostile = true;
			registry.emplace<PositionComponent>(monster, 10,8,2,0);
			TS_ASSERT_EQUALS(world.sleepInBed(*f.player,{8,8,2}), "you cannot sleep while monsters are nearby");
			registry.destroy(monster);
			u16 stone = f.definitions.getBlockFromStringID("default:stone").id();
			for (int y = 7; y <= 9; ++y) for (int x = 7; x <= 10; ++x)
				if (y != 8 || (x != 8 && x != 9)) world.setBlock(x,y,2,stone);
			TS_ASSERT_EQUALS(world.sleepInBed(*f.player,{8,8,2}), "this bed is obstructed");
			for (int y = 7; y <= 9; ++y) for (int x = 7; x <= 10; ++x)
				if (y != 8 || (x != 8 && x != 9)) world.setBlock(x,y,2,0);
			TS_ASSERT_EQUALS(world.sleepInBed(*f.player,{8,8,2}), "respawn point set");
			TS_ASSERT_EQUALS(GameTime::getTicks(),24000);
			TS_ASSERT(f.player->bedSpawn());
			f.player->setPosition(2,2,2); f.player->setHealth(0); f.commands.respawnPlayer(1);
			TS_ASSERT_EQUALS(f.player->health(),20);
			TS_ASSERT(std::abs(f.player->x()-9) < 3);
			world.setBlock(8,8,2,0);
			f.player->setHealth(0); f.commands.respawnPlayer(1);
			TS_ASSERT(!f.player->bedSpawn());
			auto &nether = f.worlds.getWorld(1); f.flat(nether);
			f.player->setDimension(1); f.player->setPosition(8.5,6.5,2);
			TS_ASSERT(nether.placeBed(*f.player,{8,8,2},data));
			nether.sleepInBed(*f.player,{8,8,2});
			TS_ASSERT_EQUALS(nether.getBlock(8,8,2),0);
			TS_ASSERT_EQUALS(nether.getBlock(9,8,2),0);
		}

		void testBedRespawnTrailerAndLegacySaveDefault() {
			Fixture f;
			f.player->setBedSpawn({8,8,2});
			auto directory = std::filesystem::temp_directory_path() / ("oldworld-bed-test-" + std::to_string(getpid()));
			std::filesystem::create_directories(directory); std::filesystem::current_path(directory);
			f.worlds.save("bed"); f.player->clearBedSpawn(); f.worlds.load("bed");
			TS_ASSERT(f.player->bedSpawn());
			if (f.player->bedSpawn()) TS_ASSERT_EQUALS(*f.player->bedSpawn(),Vector3i(8,8,2));
			ChunkDatabase database("saves/bed/map.sqlite", true);
			auto state = database.readState();
			std::string data(static_cast<const char *>(state.getData()), state.getDataSize());
			auto marker = data.rfind("BED1"); TS_ASSERT_DIFFERS(marker,std::string::npos);
			if (marker == std::string::npos) return;
			std::ofstream legacy("saves/legacy.dat",std::ios::binary); legacy.write(data.data(),marker); legacy.close();
			f.worlds.load("legacy"); TS_ASSERT(!f.player->bedSpawn());
		}

		void testClockShowsDayCycleAndErraticNetherFrames() {
			for (const auto &sample : {std::pair<u64,unsigned>{0,48},{6000,0},{12000,16},{18000,32}}) {
				ClockAnimation clock; TS_ASSERT_EQUALS(clock.update(sample.first,0),sample.second);
			}
			ClockAnimation clock;
			std::set<unsigned> frames;
			for (u64 tick = 0; tick < 200; ++tick) {
				unsigned frame = clock.update(tick,1); TS_ASSERT(frame < 64);
				TS_ASSERT_EQUALS(clock.update(tick,1),frame); frames.insert(frame);
			}
			TS_ASSERT(frames.size() > 8);
		}
};

#endif // BEDSCLOCKTESTS_HPP_
