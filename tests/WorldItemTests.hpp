#ifndef WORLDITEMTESTS_HPP_
#define WORLDITEMTESTS_HPP_

#include <cxxtest/TestSuite.h>
#include "server/world/save/ChunkDatabase.hpp"
#include <filesystem>
#include <fstream>
#include <unistd.h>
#include "common/world/BetaMining.hpp"
#include "server/world/save/WorldSaveBasicBackend.hpp"
#include "server/lua/LuaMod.hpp"
#include "server/scene/EntityWrapper.hpp"
#include "server/scene/controller/CollisionController.hpp"
#include "client/scene/component/ItemVisualComponent.hpp"
#include "common/core/Registry.hpp"
#include "common/core/GameClock.hpp"
#include "common/scene/component/PhysicalComponent.hpp"
#include "common/scene/component/TntComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/lua/ServerModLoader.hpp"
#include "common/inventory/Recipe.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerWorld.hpp"
#include "server/world/WorldController.hpp"

class WorldItemTests : public CxxTest::TestSuite {
	public:
		void setUp() override {
			static LoggerHandler logger; LoggerHandler::setInstance(logger);
			static GameClock clock; GameClock::setInstance(clock);
		}

		void testRegisteredContentRecipesAndCreativeInventory() {
			try {
			namespace fs = std::filesystem;
			struct RestoreDirectory { fs::path original = fs::current_path(); ~RestoreDirectory() { fs::current_path(original); } } restore;
			fs::current_path(OM_TEST_SOURCE_DIR);
			ScriptEngine scripts;
			Registry definitions; Registry::setInstance(definitions);
			definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air").setIsBlock(true);
			WorldController worlds(definitions); ServerModLoader loader(scripts, worlds);
			loader.loadMods(); scripts.luaCore().setRegistry(&definitions);
			for (const char *id : {"apple","cookie","bread","porkchop_raw","mushroom_stew","potato","tomato","baked_potato","barley_bread","vegetable_stew"})
				TS_ASSERT_EQUALS(definitions.getItemFromStringID(std::string("default:")+id).maxStackSize(),1);
			TS_ASSERT_EQUALS(definitions.getItemFromStringID("default:copper_ingot").maxStackSize(),99);
			for (const char *id : {"firework_rocket","carrot","melon","emerald","shield"})
				TS_ASSERT(!definitions.hasItem(std::string("default:")+id));
			Inventory furnace(1,1); furnace.setStack(0,0,"default:copper_ore",1);
			Inventory grid(3,3);
			for (u16 x=0;x<3;++x) grid.setStack(x,0,"default:copper_ingot",1);
			grid.setStack(1,1,"default:stick",1); grid.setStack(1,2,"default:stick",1);
			const Recipe *smelt = definitions.getRecipe(furnace), *craft = definitions.getRecipe(grid);
			TS_ASSERT(smelt); TS_ASSERT(craft);
			if (smelt) TS_ASSERT_EQUALS(smelt->result().stringID(), "default:copper_ingot");
			if (craft) TS_ASSERT_EQUALS(craft->result().stringID(), "default:copper_pickaxe");
			Server server; server.init(); auto session=Network::LocalSession::connect(); ClientInfo client{1,session}; client.creative=true;
			sol::protected_function show=scripts.lua()["show_creative_inventory"];
			auto result=show(0,std::ref(client)); TS_ASSERT(result.valid());
			PlayerList players; worlds.init(players,42);
			auto &world=worlds.getWorld(0); auto &chunk=world.getOrCreateChunk(0,0,1); world.generateChunk(chunk);
			u16 copper=definitions.getBlockFromStringID("default:copper_ore").id(); unsigned count=0;
			for(int z=0;z<CHUNK_HEIGHT;++z) for(int y=0;y<CHUNK_DEPTH;++y) for(int x=0;x<CHUNK_WIDTH;++x) count+=chunk.getBlock(x,y,z)==copper;
			TS_ASSERT(count>0);
			} catch (const Exception &e) { TS_FAIL(e.what()); } catch (const std::exception &e) { TS_FAIL(e.what()); }
		}

		void testCopperGatesIronWithoutSkippingDiamondTier() {
			Item stone{1, {}, "test:stone", "stone"}, copper{2, {}, "test:copper", "copper"};
			for (Item *item : {&stone, &copper}) { item->setHarvestCapability(1); item->addGroup("group:om_harvest_level", 1); }
			copper.addGroup("group:om_copper_tool", 1);
			Block ore{1,"default:copper_ore"}, iron{2,"default:iron_ore"}, diamond{3,"default:diamond_ore"};
			for (Block *block : {&ore,&iron,&diamond}) block->getState(0).harvestRequirements(1);
			TS_ASSERT(retro::canHarvest(ore.getState(0), stone));
			TS_ASSERT(!retro::canHarvest(iron.getState(0), stone));
			TS_ASSERT(retro::canHarvest(iron.getState(0), copper));
			TS_ASSERT(!retro::canHarvest(diamond.getState(0), copper));
		}

		void testCreativeAndArmorSaveRoundTripAndLegacyDefault() {
			try {
			namespace fs = std::filesystem;
			struct RestoreDirectory { fs::path original = fs::current_path(); ~RestoreDirectory() { fs::current_path(original); } } restore;
			fs::path directory = fs::temp_directory_path() / ("oldworld-mode-test-" + std::to_string(getpid()));
			fs::create_directories(directory); fs::current_path(directory);
			Registry definitions; Registry::setInstance(definitions);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air");
			auto &helmet = definitions.registerItem<Item>({}, "test:helmet", "helmet");
			helmet.addGroup("group:om_armor_slot", 1); helmet.addGroup("group:om_max_damage",100);
			PlayerList players; auto &player = players.addPlayer("tester", false);
			player.setScore(281); player.offhand().setItemStack(0,0,ItemStack{"test:helmet",1,23});
			player.setHealth(13); player.armor().setItemStack(0,0,ItemStack{"test:helmet",1,17});
			Dimension dimension{0,"test:world","world"}; std::deque<ServerWorld> worlds;
			worlds.emplace_back(players, dimension, 42); worlds.front().setCreative(true);
			WorldSaveBasicBackend backend(worlds,players); backend.save("creative");
			worlds.front().setCreative(false); player.setHealth(1); player.setScore(0); player.offhand().clearStack(0,0); player.armor().clearStack(0,0);
			backend.load("creative");
			TS_ASSERT(worlds.front().isCreative()); TS_ASSERT_EQUALS(player.health(),13);
			TS_ASSERT_EQUALS(player.armor().getStack(0,0).damage(),17);
			TS_ASSERT_EQUALS(player.score(),281);
			TS_ASSERT_EQUALS(player.offhand().getStack(0,0).damage(),23);
			ChunkDatabase database("saves/creative/map.sqlite", true);
			auto state = database.readState();
			std::string legacy(static_cast<const char *>(state.getData()), state.getDataSize());
			auto cut=legacy.find("SCOR"); TS_ASSERT_DIFFERS(cut,std::string::npos);
			std::ofstream output("saves/legacy.dat",std::ios::binary); output.write(legacy.data(),cut); output.close();
			worlds.front().setCreative(false); player.armor().clearStack(0,0); backend.load("legacy");
			TS_ASSERT(!worlds.front().isCreative()); TS_ASSERT_EQUALS(player.health(),13);
			TS_ASSERT_EQUALS(player.armor().getStack(0,0).amount(),0);
			TS_ASSERT_EQUALS(player.offhand().getStack(0,0).amount(),0);
			player.setCreative(true); player.damage(100); TS_ASSERT_EQUALS(player.health(),20);
			player.setCreative(false); player.damage(5); TS_ASSERT_EQUALS(player.health(),15);
			} catch (const Exception &e) { TS_FAIL(e.what()); } catch (const std::exception &e) { TS_FAIL(e.what()); }
		}

		void testArmorReductionWearAndBreakage() {
			Registry definitions; Registry::setInstance(definitions);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air");
			Player player;
			const u16 points[] = {3,8,6,3};
			for (u16 slot = 0; slot < 4; ++slot) {
				std::string id = "test:armor_" + std::to_string(slot);
				auto &item = definitions.registerItem<Item>({}, id, "armor");
				item.addGroup("group:om_armor_slot", slot + 1);
				item.addGroup("group:om_armor_points", points[slot]);
				item.addGroup("group:om_max_damage", 100);
				player.armor().setStack(0, slot, id, 1);
			}
			TS_ASSERT_EQUALS(player.armorValue(), 20);
			TS_ASSERT_EQUALS(player.absorbDamage(10), 2);
			TS_ASSERT_EQUALS(player.armor().getStack(0,0).damage(), 10);
			TS_ASSERT_EQUALS(player.armorValue(), 18);
			player.absorbDamage(100);
			TS_ASSERT_EQUALS(player.armorValue(), 0);
			TS_ASSERT_EQUALS(player.armor().getStack(0,0).amount(), 0);
		}

		void testTntFuseAndPersistence() {
			Registry definitions; Registry::setInstance(definitions);
			definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
			auto &tnt = definitions.registerBlock<ServerBlock>("default:tnt");
			definitions.registerItem<Item>({}, BLOCK_AIR, "air");
			PlayerList players; Dimension dimension{0,"test:world","world"};
			ServerWorld world(players, dimension, 42);
			auto &chunk = world.getOrCreateChunk(0,0,0); chunk.setInitialized(true);
			chunk.setBlockRaw(8,8,5,tnt.id());
			TS_ASSERT(world.primeTnt({8,8,5}, 3));
			TS_ASSERT_EQUALS(world.getBlock(8,8,5),0);
			auto &registry = world.scene().registry();
			world.updateTnt(); world.updateTnt();
			TS_ASSERT_EQUALS(registry.view<TntComponent>().size(),1u);
			world.updateTnt();
			TS_ASSERT_EQUALS(registry.view<TntComponent>().size(),0u);
			TntComponent original; original.age=39; original.velocity={1,2,3};
			sf::Packet packet; packet << original << u32(123);
			TntComponent restored; u32 marker=0; packet >> restored >> marker;
			TS_ASSERT_EQUALS(marker,123u); TS_ASSERT_EQUALS(restored.age,39); TS_ASSERT_EQUALS(restored.velocity.z,3);
		}

		void testStackLimitsAndLegacyOverflow() {
			Registry definitions; Registry::setInstance(definitions);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air");
			auto &food = definitions.registerItem<Item>({}, "test:food", "food"); food.setMaxStackSize(1);
			auto &item = definitions.registerItem<Item>({}, "test:item", "item");
			TS_ASSERT_EQUALS(item.maxStackSize(), 99);
			Inventory bag(3,1); bag.setStack(0,0,"test:food",5);
			auto overflow = bag.normalizeStacks();
			TS_ASSERT_EQUALS(overflow.size(), 1u);
			TS_ASSERT_EQUALS(overflow[0].amount(), 2);
			for (const auto &stack : bag.items()) TS_ASSERT_EQUALS(stack.amount(), 1);
			bag.clearStack(0,0); bag.clearStack(1,0); bag.clearStack(2,0);
			bag.addStack("test:item", 200);
			TS_ASSERT_EQUALS(bag.getStack(0,0).amount(),99);
			TS_ASSERT_EQUALS(bag.getStack(1,0).amount(),99);
			TS_ASSERT_EQUALS(bag.getStack(2,0).amount(),2);
			definitions.registerBlock<ServerBlock>(BLOCK_AIR);
			definitions.registerBlock<ServerBlock>("test:before");
			definitions.reserveBlockID<ServerBlock>();
			const auto &after = definitions.registerBlock<ServerBlock>("test:after");
			TS_ASSERT_EQUALS(after.id(),3);
			TS_ASSERT_EQUALS(definitions.getBlock(2).id(),0);
		}

		void testNearbyItemPickupHonorsDelayAndDeadPlayers() {
			Registry definitions; Registry::setInstance(definitions);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air");
			definitions.registerItem<Item>({}, "test:item", "item");
			definitions.registerEntity("test:drop");
			PlayerList players; ScriptEngine scripts; WorldController worlds(definitions);
			Server server; server.init(); auto session = Network::LocalSession::connect();
			ClientInfo client{1, session}; auto *player = players.connectPlayer("tester", client);
			ServerCommandHandler commands(scripts, server, worlds, players, definitions);
			player->setReady(true); player->setPosition(8, 8, 5);
			entt::registry registry;
			auto item = registry.create();
			registry.emplace<PositionComponent>(item, 9.1, 8, 5.2, 0);
			registry.emplace<DoubleBox>(item, -0.125, -0.125, -0.125, 0.25, 0.25, 0.25);
			registry.emplace<std::string>(item, "test:drop");
			registry.emplace<ItemStack>(item, "test:item", 1);
			auto &body = registry.emplace<PhysicalComponent>(item);
			unsigned pickups = 0;
			scripts.lua()["pickup"] = [&](EntityWrapper &entity, ServerPlayer &, ServerCommandHandler &) {
				++pickups;
				registry.destroy(entity.id());
			};
			definitions.addEntityCallbackContainer("test:drop").collisionCallback = scripts.lua()["pickup"];
			CollisionController collisions(players); collisions.setServer(&commands);
			collisions.update(registry);
			TS_ASSERT_EQUALS(pickups, 0u);
			body.pickupDelay = 0; player->setHealth(0);
			collisions.update(registry);
			TS_ASSERT_EQUALS(pickups, 0u);
			player->setHealth(20);
			collisions.update(registry);
			TS_ASSERT_EQUALS(pickups, 1u);
			TS_ASSERT(!registry.valid(item));
			definitions.getEntityCallbackContainer("test:drop").collisionCallback = sol::protected_function{};
		}

		void testPickupUsesBackpackAndPreservesPartialRemainder() {
			Registry definitions; Registry::setInstance(definitions);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air");
			definitions.registerItem<Item>({}, "test:item", "item");
			PlayerList players; ScriptEngine scripts; WorldController worlds(definitions);
			Server server; server.init(); auto session = Network::LocalSession::connect();
			ClientInfo client{1, session}; auto *player = players.connectPlayer("tester", client);
			ServerCommandHandler commands(scripts, server, worlds, players, definitions);
			worlds.setServer(commands);
			LuaMod mod("test", worlds);
			auto &lua = scripts.lua();
			LuaMod::initUsertype(lua);
			lua["mod"] = &mod;
			lua["player"] = player;
			for (u16 x = 0; x < 9; ++x) player->inventory().setStack(x, 0, "test:item", 99);
			ItemStack drop{"test:item", 4};
			lua["stack"] = &drop;
			auto result = lua.safe_script("return mod:give_item_stack(player, stack)", sol::script_pass_on_error);
			TS_ASSERT(result.valid()); if (!result.valid()) return;
			TS_ASSERT_EQUALS(result.get<ItemStack>().amount(), 0);
			TS_ASSERT_EQUALS(player->inventory().getStack(0, 1).amount(), 4);
			for (u16 y = 1; y < 4; ++y)
				for (u16 x = 0; x < 9; ++x) player->inventory().setStack(x, y, "test:item", 99);
			player->inventory().setStack(8, 3, "test:item", 98);
			result = lua.safe_script("return mod:give_item_stack(player, stack)", sol::script_pass_on_error);
			TS_ASSERT(result.valid()); if (!result.valid()) return;
			TS_ASSERT_EQUALS(result.get<ItemStack>().amount(), 3);
			TS_ASSERT_EQUALS(player->inventory().getStack(8, 3).amount(), 99);
			Network::Packet packet;
			while (session->receiveOnClient(packet)) {}
			result = lua.safe_script("return mod:give_item_stack(player, stack)", sol::script_pass_on_error);
			TS_ASSERT(result.valid()); if (!result.valid()) return;
			TS_ASSERT_EQUALS(result.get<ItemStack>().amount(), 4);
			TS_ASSERT(!session->receiveOnClient(packet));
		}

		void testItemInterpolationDoesNotDependOnRenderFrequency() {
			ItemVisualComponent low, high;
			low.update({0, 0, 1}, 0); high.update({0, 0, 1}, 0);
			low.update({1, 0, 1}, 50); high.update({1, 0, 1}, 50);
			for (u64 now = 51; now < 75; ++now) high.update({1, 0, 1}, now);
			TS_ASSERT_DELTA(low.update({1, 0, 1}, 75).x, 0.5, 1e-9);
			TS_ASSERT_DELTA(high.update({1, 0, 1}, 75).x, 0.5, 1e-9);
			TS_ASSERT_DELTA(low.update({2, 0, 1}, 100).x, 1, 1e-9);
			TS_ASSERT_DELTA(low.update({2, 0, 1}, 150).x, 2, 1e-9);
			TS_ASSERT_DELTA(low.update({20, 0, 1}, 151).x, 20, 1e-9);
		}
		void testFatalDamageWaitsForRespawnAndDoesNotRepeatDrops() {
			Registry definitions; Registry::setInstance(definitions);
			definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air");
			definitions.registerItem<Item>({}, "test:tool", "tool");
			definitions.registerEntity("default:item_drop");
			definitions.registerDimension("test:world", "world");
			PlayerList players; ScriptEngine scripts; WorldController worlds(definitions); worlds.init(players, 42);
			Server server; server.init(); auto session = Network::LocalSession::connect();
			ClientInfo client{1, session}; auto *player = players.connectPlayer("tester", client);
			TS_ASSERT(player); if (!player) return;
			player->setPosition(8, 8, 5); player->setScore(123);
			player->inventory().setItemStack(0, 0, {"test:tool", 1, 37});
			player->offhand().setItemStack(0, 0, {"test:tool", 1, 19});
			ServerCommandHandler commands(scripts, server, worlds, players, definitions);
			auto &registry = worlds.getWorld(0).scene().registry();
			for (int i = 0; i < 128; ++i) registry.emplace<ItemStack>(registry.create(), "test:tool", 1);
			commands.damagePlayer(1, 20, "mob:zombie");
			TS_ASSERT_EQUALS(player->health(), 0);
			TS_ASSERT_EQUALS(player->score(), 123);
			TS_ASSERT_EQUALS(player->inventory().getStack(0, 0).amount(), 0);
			TS_ASSERT_EQUALS(player->offhand().getStack(0, 0).amount(), 0);
			TS_ASSERT_EQUALS(registry.view<ItemStack>().size(), 130u);
			commands.damagePlayer(1, 20);
			TS_ASSERT_EQUALS(registry.view<ItemStack>().size(), 130u);
			TS_ASSERT(worlds.getWorld(0).dropPlayerStack(*player, {"test:tool", 1, 41}));
			TS_ASSERT_EQUALS(registry.view<ItemStack>().size(), 131u);
			commands.sendPlayerScore(1);
			Network::Packet packet; Network::Command command;
			bool sawDead = false, sawScore = false;
			unsigned deaths = 0;
			while (session->receiveOnClient(packet)) {
				packet >> command;
				if (command == Network::Command::PlayerDeath) { std::string message; packet >> message; TS_ASSERT_EQUALS(message, "tester was slain by zombie"); ++deaths; }
				if (command == Network::Command::PlayerHealth) { u16 health; packet >> health; sawDead = health == 0; }
				if (command == Network::Command::PlayerScore) { s32 score; packet >> score; sawScore = score == 123; }
			}
			TS_ASSERT(sawDead && sawScore);
			TS_ASSERT_EQUALS(deaths, 1u);
			commands.respawnPlayer(1);
			TS_ASSERT_EQUALS(player->health(), 20);
			TS_ASSERT_EQUALS(player->score(), 0);
			TS_ASSERT_EQUALS(player->x(), 0.5);
			TS_ASSERT_EQUALS(player->y(), 0.5);
			TS_ASSERT_EQUALS(player->dimension(), 0);
			double respawnHeight = player->z();
			commands.respawnPlayer(1);
			TS_ASSERT_EQUALS(player->z(), respawnHeight);
		}

		void testDamageAndToolBreakHaveExplicitFeedbackMessages() {
			Registry definitions;
			Registry::setInstance(definitions);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air");
			auto &tool = definitions.registerItem<Item>({}, "test:tool", "tool");
			tool.addGroup("group:om_max_damage", 59);
			PlayerList players;
			ScriptEngine scripts;
			WorldController worlds(definitions);
			Server server;
			server.init();
			auto session = Network::LocalSession::connect();
			ClientInfo client{1, session};
			auto *player = players.connectPlayer("tester", client);
			TS_ASSERT(player);
			if (!player) return;
			ServerCommandHandler commands(scripts, server, worlds, players, definitions);
			commands.damagePlayer(1, 2);
			Network::Packet packet;
			Network::Command command;
			TS_ASSERT(session->receiveOnClient(packet));
			packet >> command;
			TS_ASSERT(command == Network::Command::PlayerHurt);
			TS_ASSERT(session->receiveOnClient(packet));
			u16 health;
			packet >> command >> health;
			TS_ASSERT(command == Network::Command::PlayerHealth);
			TS_ASSERT_EQUALS(health, 18);
			commands.damagePlayer(1, 2);
			TS_ASSERT(!session->receiveOnClient(packet));
			player->setHeldItemSlot(0);
			player->inventory().setItemStack(0, 0, {"test:tool", 1, 59});
			commands.wearHeldItem(*player, 1);
			TS_ASSERT_EQUALS(player->heldItemStack().amount(), 0);
			TS_ASSERT(session->receiveOnClient(packet));
			packet >> command;
			TS_ASSERT(command == Network::Command::PlayerInvUpdate);
			TS_ASSERT(session->receiveOnClient(packet));
			packet >> command;
			TS_ASSERT(command == Network::Command::PlayerToolBroken);
			TS_ASSERT(!session->receiveOnClient(packet));
		}

		void testHeldDropPreservesDamageAndDoesNotConsumeOnFailure() {
			Registry definitions;
			Registry::setInstance(definitions);
			definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air");
			definitions.registerItem<Item>({}, "test:tool", "tool");
			auto model = definitions.registerEntity("default:item_drop");
			definitions.entityRegistry().emplace<DoubleBox>(model, -0.125, -0.125, -0.125, 0.25, 0.25, 0.25);
			PlayerList players;
			Dimension dimension{0, "test:world", "world"};
			ServerWorld world(players, dimension, 42);
			world.getOrCreateChunk(0, 0, 0).setInitialized(true);
			auto &player = players.addPlayer("tester", true);
			player.setPosition(8, 8, 5);
			player.setRotation(90, 0);
			player.setHeldItemSlot(0);
			player.inventory().setItemStack(0, 0, {"test:tool", 2, 37});
			TS_ASSERT(!world.dropHeldItem(player));
			TS_ASSERT_EQUALS(player.heldItemStack().amount(), 2);
			player.setReady(true);
			TS_ASSERT(world.dropHeldItem(player));
			TS_ASSERT_EQUALS(player.heldItemStack().amount(), 1);
			TS_ASSERT_EQUALS(player.heldItemStack().damage(), 37);
			auto &registry = world.scene().registry();
			TS_ASSERT_EQUALS(registry.view<ItemStack>().size(), 1u);
			registry.view<ItemStack, PositionComponent, PhysicalComponent>().each([&](auto, auto &stack, auto &position, auto &body) {
				TS_ASSERT_EQUALS(stack.amount(), 1);
				TS_ASSERT_EQUALS(stack.damage(), 37);
				TS_ASSERT_DELTA(position.z, 6.325, 0.001);
				TS_ASSERT_DELTA(body.velocity.x, 0, 0.001);
				TS_ASSERT_DELTA(body.velocity.y, 6, 0.001);
				TS_ASSERT_DELTA(body.velocity.z, 2, 0.001);
				TS_ASSERT_EQUALS(body.pickupDelay, 40);
			});
			TS_ASSERT(world.dropHeldItem(player));
			TS_ASSERT_EQUALS(player.heldItemStack().amount(), 0);
			TS_ASSERT(!world.dropHeldItem(player));
			player.inventory().setItemStack(0, 0, {"test:tool", 2, 37});
			player.setHeldItemSlot(255);
			TS_ASSERT(!world.dropHeldItem(player));
			player.setHeldItemSlot(0);
			player.setPosition(200, 200, 5);
			TS_ASSERT(!world.dropHeldItem(player));
			player.setPosition(8, 8, 5);
			while (registry.view<ItemStack>().size() < 128) registry.emplace<ItemStack>(registry.create(), "test:tool", 1);
			TS_ASSERT(!world.dropHeldItem(player));
			TS_ASSERT_EQUALS(player.heldItemStack().amount(), 2);
			TS_ASSERT_EQUALS(player.heldItemStack().damage(), 37);
		}

		void testMergingKeepsTheLongerPickupDelay() {
			Registry definitions;
			Registry::setInstance(definitions);
			definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air");
			definitions.registerItem<Item>({}, "test:stone", "stone");
			auto model = definitions.registerEntity("default:item_drop");
			definitions.entityRegistry().emplace<DoubleBox>(model, -0.125, -0.125, -0.125, 0.25, 0.25, 0.25);
			PlayerList players;
			Dimension dimension{0, "test:world", "world"};
			ServerWorld world(players, dimension, 42);
			world.getOrCreateChunk(0, 0, 0).setInitialized(true);
			TS_ASSERT(world.dropWorldItem({8, 8, 5}, "test:stone", 1));
			TS_ASSERT(world.dropWorldItem({8, 8, 5}, "test:stone", 1));
			auto &registry = world.scene().registry();
			unsigned index = 0;
			registry.view<ItemStack>().each([&](auto entity, auto &) {
				auto &body = registry.emplace<PhysicalComponent>(entity);
				body.pickupDelay = index++ ? 40 : 0;
			});
			world.updateItems();
			TS_ASSERT_EQUALS(registry.view<ItemStack>().size(), 1u);
			registry.view<ItemStack, PhysicalComponent>().each([&](auto, auto &stack, auto &body) {
				TS_ASSERT_EQUALS(stack.amount(), 2);
				TS_ASSERT_EQUALS(body.pickupDelay, 39);
			});
		}

		void testPhysicalStatePacketBoundary() {
			PhysicalComponent body;
			body.velocity = {1, -2, 3}; body.age = 77; body.pickupDelay = 4;
			body.forceOrigin = {-10, 20, -30}; body.forceDirection = {0, 1, 0};
			sf::Packet packet;
			packet << body << u32(0x12345678);
			PhysicalComponent restored;
			u32 marker = 0;
			packet >> restored >> marker;
			TS_ASSERT_EQUALS(marker, 0x12345678u);
			TS_ASSERT_EQUALS(restored.age, 77u);
			TS_ASSERT_EQUALS(restored.pickupDelay, 4);
			TS_ASSERT_EQUALS(restored.velocity.y, -2);
			TS_ASSERT_EQUALS(restored.forceOrigin.z, -30);
			TS_ASSERT_EQUALS(restored.forceDirection.y, 1);
		}

		void testItemGravityMergeAndUnloadedFreeze() {
			Registry definitions;
			Registry::setInstance(definitions);
			definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
			definitions.registerBlock<ServerBlock>("test:stone");
			definitions.registerItem<Item>({}, BLOCK_AIR, "air");
			definitions.registerItem<Item>({}, "test:stone", "stone");
			auto model = definitions.registerEntity("default:item_drop");
			definitions.entityRegistry().emplace<DoubleBox>(model, -0.125, -0.125, -0.125, 0.25, 0.25, 0.25);
			PlayerList players;
			Dimension dimension{0, "test:world", "world"};
			ServerWorld world(players, dimension, 42);
			auto &chunk = world.getOrCreateChunk(0, 0, 0);
			for (int x = 0; x < 16; ++x)
				for (int y = 0; y < 16; ++y) chunk.setBlockRaw(x, y, 0, 1);
			chunk.setInitialized(true);
			TS_ASSERT(world.dropWorldItem({8, 8, 5}, "test:stone", 17));
			TS_ASSERT(world.dropWorldItem({8.2, 8, 5}, "test:stone", 23));
			world.updateItems();
			auto &registry = world.scene().registry();
			TS_ASSERT_EQUALS(registry.view<ItemStack>().size(), 1u);
			for (int tick = 0; tick < 100; ++tick) world.updateItems();
			registry.view<ItemStack, PositionComponent, PhysicalComponent>().each([&](auto, auto &stack, auto &position, auto &body) {
				TS_ASSERT_EQUALS(stack.amount(), 40);
				TS_ASSERT_DELTA(position.z, 1.125, 0.001);
				TS_ASSERT_EQUALS(body.pickupDelay, 0);
				TS_ASSERT_DELTA(body.velocity.z, 0, 0.001);
			});
			TS_ASSERT(world.dropWorldItem({200, 200, 5}, "test:stone", 1));
			world.updateItems();
			registry.view<ItemStack, PositionComponent>().each([&](auto entity, auto &, auto &position) {
				if (position.x != 200) return;
				TS_ASSERT_DELTA(position.z, 5.3, 0.001);
				TS_ASSERT(!registry.all_of<PhysicalComponent>(entity));
			});
		}
};

#endif // WORLDITEMTESTS_HPP_
