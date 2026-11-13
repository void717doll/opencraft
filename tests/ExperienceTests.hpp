#ifndef EXPERIENCETESTS_HPP_
#define EXPERIENCETESTS_HPP_
#include <cxxtest/TestSuite.h>
#include "common/world/Experience.hpp"
#include "common/core/GameClock.hpp"
#include "common/core/Registry.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/WorldController.hpp"

class ExperienceTests : public CxxTest::TestSuite {
public:
	void testModernMcpLevelCostsAndOverflow() {
		for(auto pair:{std::pair<int,unsigned>{0,0},{16,0},{17,1},{255,15},{272,16},{825,30},{887,31}}) {
			auto xp=experience::progress(pair.first);TS_ASSERT_EQUALS(xp.level,pair.second);TS_ASSERT(xp.points<xp.capacity);
		}
		TS_ASSERT_EQUALS(experience::progress(16).points,16u);
		TS_ASSERT_EQUALS(experience::progress(17).points,0u);
		TS_ASSERT_EQUALS(experience::capacity(16),20u);TS_ASSERT_EQUALS(experience::capacity(31),69u);
		TS_ASSERT_EQUALS(experience::add(-5,2),2);
		TS_ASSERT_EQUALS(experience::add(2147483646,8),2147483647);
		auto high=experience::progress(2147483647);TS_ASSERT(high.points<high.capacity);
		for(unsigned roll=0;roll<30;++roll) {
			TS_ASSERT(experience::oreReward("default:coal_ore",roll)<=2);
			TS_ASSERT(experience::oreReward("default:diamond_ore",roll)>=3 && experience::oreReward("default:diamond_ore",roll)<=7);
			TS_ASSERT_EQUALS(experience::oreReward("default:iron_ore",roll),0u);
		}
	}
	void testMiningAwardsExperienceOnlyWithTheRightSurvivalTool() {
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
