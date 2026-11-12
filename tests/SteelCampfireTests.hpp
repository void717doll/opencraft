#ifndef STEELCAMPFIRETESTS_HPP_
#define STEELCAMPFIRETESTS_HPP_

#include <cxxtest/TestSuite.h>
#include <filesystem>

#include "common/core/GameClock.hpp"
#include "common/core/LoggerHandler.hpp"
#include "common/core/Registry.hpp"
#include "common/world/BetaMining.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/lua/ServerModLoader.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/PlayerSurvival.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/WorldController.hpp"

class SteelCampfireTests : public CxxTest::TestSuite {
	struct Fixture {
		std::filesystem::path original = std::filesystem::current_path();
		ScriptEngine scripts;
		Registry definitions;
		PlayerList players;
		WorldController worlds{definitions};
		ServerModLoader loader{scripts, worlds};
		Server server;
		ServerCommandHandler commands{scripts, server, worlds, players, definitions};
		Fixture() {
			std::filesystem::current_path(OM_TEST_SOURCE_DIR);
			Registry::setInstance(definitions);
			auto &air = definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0);
			air.isOpaque(false); air.isCollidable(false);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air").setIsBlock(true);
			loader.loadMods(); scripts.luaCore().setRegistry(&definitions);
			server.init(); worlds.setServer(commands); worlds.init(players, 42);
			auto &world = worlds.getWorld(0);
			auto &chunk = world.getOrCreateChunk(0, 0, 0);
			u16 stone = definitions.getBlockFromStringID("default:stone").id();
			for (int z = 0; z < CHUNK_HEIGHT; ++z) for (int y = 0; y < CHUNK_DEPTH; ++y)
				for (int x = 0; x < CHUNK_WIDTH; ++x) chunk.setBlockRaw(x, y, z, z == 0 ? stone : 0);
			chunk.setInitialized(true);
		}
		~Fixture() { std::filesystem::current_path(original); }
		const ServerBlock &place(const char *id, int x) {
			auto &world = worlds.getWorld(0);
			auto &block = static_cast<const ServerBlock &>(definitions.getBlockFromStringID(id));
			world.setBlock(x, 8, 1, block.id()); block.onBlockPlaced({x, 8, 1}, world);
			return block;
		}
		void tick(const ServerBlock &block, int x, unsigned count) {
			auto &world = worlds.getWorld(0);
			for (unsigned step = 0; step < count; ++step)
				block.onTick({x, 8, 1}, static_cast<ServerChunk &>(*world.getChunkAtBlockPos(x, 8, 1)), world, commands);
		}
	};

	public:
		void setUp() override {
			static LoggerHandler logger; LoggerHandler::setInstance(logger);
			static GameClock clock; GameClock::setInstance(clock);
		}

		void testRealSteelDefinitionsKeepTheCopperLadderAndStackLimits() {
			Fixture f;
			auto &obsidian = f.definitions.getBlockFromStringID("default:obsidian").getState(0);
			auto &iron = f.definitions.getBlockFromStringID("default:iron_ore").getState(0);
			const auto &steel = f.definitions.getItemFromStringID("default:steel_pickaxe");
			const auto &diamond = f.definitions.getItemFromStringID("default:diamond_pickaxe");
			const auto &stone = f.definitions.getItemFromStringID("default:stone_pickaxe");
			const auto &copper = f.definitions.getItemFromStringID("default:copper_pickaxe");
			TS_ASSERT(retro::canHarvest(obsidian, steel));
			TS_ASSERT(!retro::canHarvest(obsidian, copper));
			TS_ASSERT(!retro::canHarvest(iron, stone));
			TS_ASSERT(retro::canHarvest(iron, copper));
			TS_ASSERT_EQUALS(steel.miningSpeed(), stone.miningSpeed());
			TS_ASSERT_EQUALS(steel.getGroupValue("group:om_max_damage"), 5000);
			TS_ASSERT_EQUALS(steel.maxStackSize(), 1);
			f.scripts.lua()["steel"] = &steel;
			f.scripts.lua()["ingot"] = &f.definitions.getItemFromStringID("default:steel_ingot");
			f.scripts.lua()["food"] = &f.definitions.getItemFromStringID("default:baked_potato");
			auto result = f.scripts.lua().safe_script(
				"assert(steel:max_stack_size() == 1 and ingot:max_stack_size() == 99 and food:max_stack_size() == 1)",
				sol::script_pass_on_error);
			TS_ASSERT(result.valid());
		}

		void testRealCookingCallbacksAndStoredInventoryRemoval() {
			Fixture f; auto &world = f.worlds.getWorld(0);
			auto &blast = f.place("default:blast_furnace", 6);
			auto *data = world.getBlockData(6, 8, 1);
			TS_ASSERT(data); if (!data) return;
			data->inventory.setStack(0, 0, "default:iron_ingot", 2);
			data->inventory.setStack(2, 0, "default:coal", 1);
			f.tick(blast, 6, 100);
			TS_ASSERT_EQUALS(data->inventory.getStack(1, 0).stringID(), "default:molten_iron");
			TS_ASSERT_EQUALS(data->inventory.getStack(1, 0).amount(), 1);
			TS_ASSERT_EQUALS(data->meta.get<int>("current_burn_time"), 800);
			auto &campfire = f.place("default:campfire", 8);
			data = world.getBlockData(8, 8, 1);
			TS_ASSERT(data); if (!data) return;
			data->inventory.setStack(0, 0, "default:potato", 1);
			f.tick(campfire, 8, 600);
			TS_ASSERT_EQUALS(data->inventory.getStack(0, 0).stringID(), "default:baked_potato");
			TS_ASSERT_EQUALS(data->inventory.getStack(0, 0).amount(), 1);
			world.setBlock(8, 8, 1, 0);
			TS_ASSERT(!world.getBlockData(8, 8, 1));
			unsigned food = 0;
			world.scene().registry().view<ItemStack>().each([&](auto, const auto &stack) {
				if (stack.stringID() == "default:baked_potato") food += stack.amount();
			});
			TS_ASSERT_EQUALS(food, 1);
		}

		void testCampfireContactHurtsWithoutPersistentBurning() {
			Fixture f; auto &world = f.worlds.getWorld(0);
			f.place("default:campfire", 8);
			DoubleBox body{8.2, 8.2, 1.4375, 0.6, 0.6, 1.8};
			auto environment = playerEnvironment(world, body);
			TS_ASSERT(environment.loaded && environment.campfire && environment.grounded);
			PlayerSurvivalState survival;
			TS_ASSERT_EQUALS(survival.tick(environment), 1);
			TS_ASSERT_EQUALS(std::string(survival.damageCause), "campfire");
			TS_ASSERT_EQUALS(survival.fireTicks, -20);
			world.setBlockState(8, 8, 1, 1);
			environment = playerEnvironment(world, body);
			TS_ASSERT(!environment.campfire && environment.grounded);
			TS_ASSERT_EQUALS(survival.tick(environment), 0);
		}
};

#endif // STEELCAMPFIRETESTS_HPP_
