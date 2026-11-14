#ifndef EXPERIENCETESTS_HPP_
#define EXPERIENCETESTS_HPP_
#include <cxxtest/TestSuite.h>
#include "common/world/Experience.hpp"
#include "common/core/GameClock.hpp"
#include "common/core/Registry.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/lua/LuaMod.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerItem.hpp"
#include "server/world/WorldController.hpp"

class ExperienceTests : public CxxTest::TestSuite {
public:
	void testScoreRewardsAndOverflow() {
		TS_ASSERT_EQUALS(experience::add(-5,2),2);
		TS_ASSERT_EQUALS(experience::add(2147483646,8),2147483647);
		for(unsigned roll=0;roll<30;++roll) {
			TS_ASSERT(experience::oreReward("default:coal_ore",roll)<=2);
			TS_ASSERT(experience::oreReward("default:diamond_ore",roll)>=3 && experience::oreReward("default:diamond_ore",roll)<=7);
			TS_ASSERT_EQUALS(experience::oreReward("default:iron_ore",roll),0u);
		}
	}
	void testOffhandSwapUseAndPlacementThroughServer() {
		static LoggerHandler logger; LoggerHandler::setInstance(logger);
		static GameClock clock; GameClock::setInstance(clock);
		ScriptEngine scripts; Registry registry; Registry::setInstance(registry);
		auto &air = registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0);
		air.isCollidable(false); air.isOpaque(false);
		registry.registerItem<Item>({}, BLOCK_AIR, "air");
		auto &stone = registry.registerBlock<ServerBlock>("test:stone");
		registry.registerItem<Item>({}, "test:stone", "stone").setIsBlock(true);
		registry.registerItem<ServerItem>({}, "test:food", "food").setFood(4, "");
		auto &pick = registry.registerItem<Item>({}, "test:pick", "pick");
		pick.addGroup("group:om_max_damage", 100); pick.addGroup("group:om_tool_type", 2);
		registry.registerDimension("test:world", "world");
		PlayerList players; auto &player = players.addPlayer("tester", false);
		player.setPosition(8.5, 8.5, 1);
		WorldController worlds(registry); Server server; ServerCommandHandler commands(scripts, server, worlds, players, registry);
		worlds.setServer(commands); worlds.init(players, 42);
		auto &world = worlds.getWorld(0); world.getOrCreateChunk(0, 0, 0).setInitialized(true);
		commands.setupCallbacks(); server.init(); server.setRunning(true); auto session = Network::LocalSession::connect();
		Network::Packet packet; packet << Network::Command::ClientConnect << std::string("tester");
		session->sendToServer(packet); server.handleGameEvents(); player.setReady(true); player.setHeldItemSlot(4);
		player.inventory().setItemStack(4, 0, ItemStack{"test:pick", 1, 23});
		packet.clear(); packet << Network::Command::PlayerSwapHands; session->sendToServer(packet); server.handleGameEvents();
		TS_ASSERT_EQUALS(player.offhand().getStack(0, 0).damage(), 23);
		TS_ASSERT_EQUALS(player.inventory().getStack(4, 0).amount(), 0);
		packet.clear(); packet << Network::Command::PlayerSwapHands; session->sendToServer(packet); server.handleGameEvents();
		TS_ASSERT_EQUALS(player.inventory().getStack(4, 0).damage(), 23);
		player.offhand().setStack(0, 0, "test:food", 1); player.setHealth(10);
		TS_ASSERT(player.shouldUseOffhand());
		packet.clear(); packet << Network::Command::ItemActivated << s32(8) << s32(8) << s32(1) << s8(-1) << u16(1280) << u16(720) << u8(2);
		session->sendToServer(packet); server.handleGameEvents();
		TS_ASSERT_EQUALS(player.health(), 14);
		TS_ASSERT_EQUALS(player.offhand().getStack(0, 0).amount(), 0);
		TS_ASSERT_EQUALS(player.heldItemStack().stringID(), "test:pick");
		TS_ASSERT_EQUALS(player.heldItemStack().damage(), 23);
		player.offhand().setStack(0, 0, "test:stone", 2);
		packet.clear(); packet << Network::Command::PlayerPlaceBlock << s32(9) << s32(8) << s32(1) << u32(stone.id());
		session->sendToServer(packet); server.handleGameEvents();
		TS_ASSERT_EQUALS(world.getBlock(9, 8, 1), stone.id());
		TS_ASSERT_EQUALS(player.offhand().getStack(0, 0).amount(), 1);
		TS_ASSERT_EQUALS(player.heldItemStack().damage(), 23);
		player.setCreative(true);
		packet.clear(); packet << Network::Command::PlayerPlaceBlock << s32(10) << s32(8) << s32(1) << u32(stone.id());
		session->sendToServer(packet); server.handleGameEvents();
		TS_ASSERT_EQUALS(world.getBlock(10, 8, 1), stone.id());
		TS_ASSERT_EQUALS(player.offhand().getStack(0, 0).amount(), 1);
		player.setCreative(false);
		player.inventory().setStack(4, 0, "test:food", 1);
		TS_ASSERT(!player.shouldUseOffhand());
		player.inventory().setStack(4, 0, "test:stone", 1);
		TS_ASSERT(!player.shouldUseOffhand());
		player.inventory().clearStack(4, 0);
		TS_ASSERT(player.shouldUseOffhand());
		player.offhand().setStack(0, 0, "test:stone", 97);
		player.inventory().setStack(0, 0, "test:stone", 10);
		LuaMod pickupMod("test", worlds); scripts.init();
		ItemStack collected("test:stone", 5);
		scripts.lua()["pickup_mod"] = &pickupMod;
		scripts.lua()["pickup_player"] = &player;
		scripts.lua()["pickup_stack"] = &collected;
		auto result = scripts.lua().safe_script("assert(pickup_mod:give_item_stack(pickup_player, pickup_stack):amount() == 0)", sol::script_pass_on_error);
		TS_ASSERT(result.valid());
		TS_ASSERT_EQUALS(player.offhand().getStack(0, 0).amount(), 99);
		TS_ASSERT_EQUALS(player.inventory().getStack(0, 0).amount(), 13);
		player.offhand().clearStack(0, 0);
		result = scripts.lua().safe_script("pickup_mod:give_item_stack(pickup_player, pickup_stack)", sol::script_pass_on_error);
		TS_ASSERT(result.valid());
		TS_ASSERT_EQUALS(player.offhand().getStack(0, 0).amount(), 0);
		TS_ASSERT_EQUALS(player.inventory().getStack(0, 0).amount(), 18);

	}
	void testMiningAwardsScoreOnlyWithTheRightSurvivalTool() {
		static LoggerHandler logger;LoggerHandler::setInstance(logger);static GameClock clock;GameClock::setInstance(clock);
		ScriptEngine scripts;Registry registry;Registry::setInstance(registry);
		auto &air=registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0);air.isCollidable(false);air.isOpaque(false);
		registry.registerItem<Item>({},BLOCK_AIR,"air");
		auto &ore=registry.registerBlock<ServerBlock>("default:diamond_ore");ore.getState(0).harvestRequirements(1);
		auto &pick=registry.registerItem<Item>({},"test:pick","pick");pick.setHarvestCapability(1);pick.addGroup("group:om_harvest_level",3);
		registry.registerDimension("test:world","world");
		PlayerList players;auto &player=players.addPlayer("tester",false);player.setPosition(8.5,8.5,1);
		player.inventory().setStack(0,0,"test:pick",1);player.setHeldItemSlot(0);
		WorldController worlds(registry);Server server;ServerCommandHandler commands(scripts,server,worlds,players,registry);
		worlds.setServer(commands);worlds.init(players,42);
		auto &world=worlds.getWorld(0);auto &chunk=world.getOrCreateChunk(0,0,0);chunk.setInitialized(true);
		commands.setupCallbacks();server.init();server.setRunning(true);auto session=Network::LocalSession::connect();
		Network::Packet packet;packet<<Network::Command::ClientConnect<<std::string("tester");session->sendToServer(packet);server.handleGameEvents();player.setReady(true);player.setHeldItemSlot(0);
		while(session->receiveOnClient(packet)) {}
		auto mine=[&]() {
			world.setBlock(9,8,1,ore.id());packet.clear();packet<<Network::Command::PlayerDigBlock<<s32(9)<<s32(8)<<s32(1);
			session->sendToServer(packet);server.handleGameEvents();
		};
		mine();TS_ASSERT(player.score()>=3 && player.score()<=7);s32 earned=player.score();
		bool synchronized=false;while(session->receiveOnClient(packet)) {Network::Command command;packet>>command;if(command==Network::Command::PlayerScore){s32 value;packet>>value;TS_ASSERT_EQUALS(value,earned);synchronized=true;}}
		TS_ASSERT(synchronized);
		player.inventory().clearStack(0,0);mine();TS_ASSERT_EQUALS(player.score(),earned);
		player.inventory().setStack(0,0,"test:pick",1);player.setCreative(true);mine();TS_ASSERT_EQUALS(player.score(),earned);
	}
};
#endif
