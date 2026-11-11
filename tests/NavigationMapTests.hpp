#ifndef NAVIGATIONMAPTESTS_HPP_
#define NAVIGATIONMAPTESTS_HPP_

#include <cxxtest/TestSuite.h>
#include <filesystem>
#include <unistd.h>
#include "common/world/NavigationMap.hpp"
#include "common/core/GameClock.hpp"
#include "common/core/Registry.hpp"
#include "common/network/LocalSession.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerItem.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerWorld.hpp"
#include "server/world/WorldController.hpp"

class NavigationMapTests : public CxxTest::TestSuite {
public:
	void setUp() override {
		static LoggerHandler logger; LoggerHandler::setInstance(logger);
		static GameClock clock; GameClock::setInstance(clock);
	}
	void testMapActivationConsumesOneEmptyMapAndKeepsTheFilledIdentity() {
		struct Restore {std::filesystem::path original=std::filesystem::current_path(); ~Restore(){std::filesystem::current_path(original);}} restore;
		auto directory=std::filesystem::temp_directory_path()/("openminer-map-use-"+std::to_string(getpid()));
		std::filesystem::remove_all(directory); std::filesystem::create_directories(directory); std::filesystem::current_path(directory);
		Registry definitions; Registry::setInstance(definitions);
		auto &air=definitions.registerBlock<ServerBlock>(BLOCK_AIR);
		definitions.registerItem<Item>({},BLOCK_AIR,"air");
		auto &filled=definitions.registerItem<ServerItem>({},"default:map","Map"); filled.setMaxStackSize(1); filled.addGroup("group:om_map",1);
		auto &empty=definitions.registerItem<ServerItem>({},"default:empty_map","Empty Map"); empty.addGroup("group:om_map",1);
		PlayerList players; ScriptEngine scripts; Server server; WorldController controller(definitions);
		controller.init(players,42); controller.load("activation");
		ServerCommandHandler handler(scripts,server,controller,players,definitions);
		ClientInfo client{0,std::make_shared<Network::LocalSession>()};
		auto &player=players.addPlayer("tester",false); players.connectPlayer("tester",client);
		player.inventory()=Inventory(9,4,"main"); player.setHeldItemSlot(0); player.setPosition(-1,128,60);
		player.inventory().setStack(0,0,"default:empty_map",2);
		Dimension dimension{0,"test:maps","maps"}; ServerWorld world(players,dimension,42);
		TS_ASSERT(!ghc::filesystem::exists(navigation::mapPath("activation",1)));
		TS_ASSERT(empty.onItemActivated({0,0,0},air,player,world,client,handler));
		TS_ASSERT_EQUALS(player.inventory().getStack(0,0).amount(),1);
		const auto &map=player.inventory().getStack(1,0);
		TS_ASSERT_EQUALS(map.stringID(),"default:map"); TS_ASSERT(map.damage());
		u16 id=map.damage(); player.setHeldItemSlot(1);
		TS_ASSERT(filled.onItemActivated({0,0,0},air,player,world,client,handler));
		TS_ASSERT_EQUALS(player.heldItemStack().damage(),id);
		player.setHeldItemSlot(0);
		TS_ASSERT(empty.onItemActivated({0,0,0},air,player,world,client,handler));
		TS_ASSERT_EQUALS(player.heldItemStack().stringID(),"default:map");
		TS_ASSERT_DIFFERS(player.heldItemStack().damage(),id);
	}
	void testCardinalArrowsAndOffMapDots() {
		navigation::Map map; map.x=64; map.y=64;
		for (auto direction : {std::make_pair(0.,90.f),{90.,0.f},{180.,-90.f},{-90.,180.f}}) {
			auto marker = navigation::marker(map,64,64,direction.first);
			TS_ASSERT(!marker.outside); TS_ASSERT_EQUALS(marker.rotation,direction.second);
		}
		auto northwest = navigation::marker(map,0,127,90);
		TS_ASSERT_EQUALS(northwest.x,0); TS_ASSERT_EQUALS(northwest.y,0); TS_ASSERT(!northwest.outside);
		auto southeast = navigation::marker(map,127,0,0);
		TS_ASSERT_EQUALS(southeast.x,127); TS_ASSERT_EQUALS(southeast.y,127); TS_ASSERT(!southeast.outside);
		auto outside = navigation::marker(map,-5,180,90);
		TS_ASSERT(outside.outside); TS_ASSERT_EQUALS(outside.x,0); TS_ASSERT_EQUALS(outside.y,0);
		map.scale=8; map.x=512; map.y=512;
		TS_ASSERT(!navigation::marker(map,1023,1023,90).outside);
		TS_ASSERT(navigation::marker(map,1024,1023,90).outside);
	}
	void testNewMapsAndLegacyMapsKeepTheirOwnScale() {
		struct Restore {std::filesystem::path original=std::filesystem::current_path(); ~Restore(){std::filesystem::current_path(original);}} restore;
		auto directory=std::filesystem::temp_directory_path()/("openminer-navigation-"+std::to_string(getpid()));
		std::filesystem::remove_all(directory); std::filesystem::create_directories(directory); std::filesystem::current_path(directory);
		u16 id=navigation::create("test",-1,128,0);
		TS_ASSERT(id); navigation::Map map;
		TS_ASSERT(navigation::load("test",id,map));
		TS_ASSERT_EQUALS(map.scale,1); TS_ASSERT_EQUALS(map.x,-64); TS_ASSERT_EQUALS(map.y,192);
		map.scale=8; map.colors[123]=7;
		TS_ASSERT(navigation::save("test",id,map));
		navigation::Map restored; TS_ASSERT(navigation::load("test",id,restored));
		TS_ASSERT_EQUALS(restored.scale,8); TS_ASSERT_EQUALS(restored.colors[123],7);
		// recreate the original header to exercise saves from before scale was stored.
		std::ifstream input(navigation::mapPath("test",id).string(),std::ios::binary);
		std::string bytes((std::istreambuf_iterator<char>(input)),{}); input.close();
		bytes.replace(0,4,"OMAP"); bytes.erase(16,4);
		std::ofstream output(navigation::mapPath("test",id).string(),std::ios::binary|std::ios::trunc);
		output.write(bytes.data(),std::streamsize(bytes.size())); output.close();
		TS_ASSERT(navigation::load("test",id,restored));
		TS_ASSERT_EQUALS(restored.scale,8); TS_ASSERT_EQUALS(restored.colors[123],7);
		TS_ASSERT(navigation::create("test",-1,128,0)!=id);
	}
};
#endif
