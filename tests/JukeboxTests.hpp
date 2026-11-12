#ifndef JUKEBOXTESTS_HPP_
#define JUKEBOXTESTS_HPP_

#include <cxxtest/TestSuite.h>
#include <filesystem>
#include <unistd.h>
#include "common/core/GameClock.hpp"
#include "common/core/LoggerHandler.hpp"
#include "common/core/Registry.hpp"
#include "common/inventory/Recipe.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/lua/ServerModLoader.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/WorldController.hpp"

class JukeboxTests : public CxxTest::TestSuite {
	public:
		void testDiscInsertionEjectionPersistenceAndBreakUseActualModCallbacks() {
			static LoggerHandler logger; LoggerHandler::setInstance(logger);
			static GameClock clock; GameClock::setInstance(clock);
			namespace fs = std::filesystem;
			struct Restore { fs::path original = fs::current_path(); ~Restore() { fs::current_path(original); } } restore;
			fs::current_path(OM_TEST_SOURCE_DIR);
			ScriptEngine scripts; Registry definitions; Registry::setInstance(definitions);
			auto &air = definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0); air.isOpaque(false); air.isCollidable(false);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air").setIsBlock(true);
			WorldController worlds(definitions); ServerModLoader loader(scripts, worlds); loader.loadMods(); scripts.luaCore().setRegistry(&definitions);
			PlayerList players; Server server; server.init(); auto session = Network::LocalSession::connect(); ClientInfo client{1, session};
			ServerCommandHandler commands(scripts, server, worlds, players, definitions); worlds.setServer(commands); worlds.init(players, 42);
			auto *player = players.connectPlayer("tester", client); TS_ASSERT(player); if (!player) return;
			player->setReady(true); player->setPosition(5.5, 3.5, 129); player->setHeldItemSlot(0);
			auto &world = worlds.getWorld(0); world.getOrCreateChunk(0, 0, 4).setInitialized(true);
			auto &jukebox = static_cast<const ServerBlock &>(definitions.getBlockFromStringID("default:jukebox"));
			world.setBlock(5, 5, 129, jukebox.id());
			player->inventory().setStack(0, 0, "default:record_13", 1);
			TS_ASSERT(jukebox.onBlockActivated({5, 5, 129}, *player, world, client, commands));
			TS_ASSERT_EQUALS(player->heldItemStack().amount(), 0);
			auto *data = world.getBlockData(5, 5, 129); TS_ASSERT(data); if (!data) return;
			TS_ASSERT_EQUALS(data->inventory.getStack(0, 0).stringID(), "default:record_13");
			bool heard = false; Network::Packet packet; Network::Command command;
			while (session->receiveOnClient(packet)) {
				packet >> command;
				if (command != Network::Command::Jukebox) continue;
				u16 dimension; s32 x, y, z; u8 record; packet >> dimension >> x >> y >> z >> record;
				TS_ASSERT_EQUALS(record, 1); TS_ASSERT_EQUALS(dimension, 0); TS_ASSERT_EQUALS(x, 5); TS_ASSERT_EQUALS(z, 129); heard = true;
			}
			TS_ASSERT(heard);
			fs::path directory = fs::temp_directory_path() / ("oldworld-jukebox-test-" + std::to_string(getpid()));
			fs::create_directories(directory); fs::current_path(directory); worlds.save("record");
			data->inventory.clearStack(0, 0); worlds.load("record");
			TS_ASSERT_EQUALS(world.getBlockData(5, 5, 129)->inventory.getStack(0, 0).stringID(), "default:record_13");
			world.useJukebox(5, 5, 129, *player);
			TS_ASSERT_EQUALS(world.getBlockData(5, 5, 129)->inventory.getStack(0, 0).amount(), 0);
			TS_ASSERT_EQUALS(world.scene().registry().view<ItemStack>().size(), 1u);
			bool stopped = false;
			while (session->receiveOnClient(packet)) {
				packet >> command; if (command != Network::Command::Jukebox) continue;
				u16 dimension; s32 x, y, z; u8 record; packet >> dimension >> x >> y >> z >> record; if (!record) stopped = true;
			}
			TS_ASSERT(stopped);
			player->setCreative(true); player->inventory().setStack(0, 0, "default:record_cat", 1);
			world.useJukebox(5, 5, 129, *player); TS_ASSERT_EQUALS(player->heldItemStack().amount(), 1);
			world.setBlock(5, 5, 129, 0); TS_ASSERT_EQUALS(world.scene().registry().view<ItemStack>().size(), 2u);
			TS_ASSERT(!world.getBlockData(5, 5, 129));
			Inventory grid(3, 3);
			for (int y = 0; y < 3; ++y) for (int x = 0; x < 3; ++x) grid.setStack(x, y, x == 1 && y == 1 ? "default:diamond" : "default:oak_planks", 1);
			const Recipe *recipe = definitions.getRecipe(grid); TS_ASSERT(recipe); if (recipe) TS_ASSERT_EQUALS(recipe->result().stringID(), "default:jukebox");
			grid.clearStack(1, 1); recipe = definitions.getRecipe(grid); TS_ASSERT(recipe); if (recipe) TS_ASSERT_EQUALS(recipe->result().stringID(), "default:chest");
			TS_ASSERT_EQUALS(definitions.getItemFromStringID("default:record_13").maxStackSize(), 1);
			const auto &spawner = definitions.getBlockFromStringID("default:mob_spawner").getState(0);
			TS_ASSERT(spawner.getItemDrops().empty());
		}
};

#endif
