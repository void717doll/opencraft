#ifndef EXPLORATIONTESTS_HPP_
#define EXPLORATIONTESTS_HPP_

#include <cxxtest/TestSuite.h>
#include <filesystem>
#include <set>
#include <tuple>
#include <unistd.h>
#include "common/core/GameClock.hpp"
#include "common/core/Registry.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/RareMobComponent.hpp"
#include "common/scene/component/RotationComponent.hpp"
#include "common/scene/component/MobComponent.hpp"
#include "common/world/Dimension.hpp"
#include "common/world/Exploration.hpp"
#include "common/world/RetroOre.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerItem.hpp"
#include "server/world/WorldController.hpp"
#include "server/world/OreTerrain.hpp"
#include "server/world/RuinsTerrain.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerWorld.hpp"

class ExplorationTests : public CxxTest::TestSuite {
	static void definitions(Registry &registry) {
		Registry::setInstance(registry);
		registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
		registry.registerItem<Item>({}, BLOCK_AIR, "air");
		for (const char *id : {"stone", "dirt", "gravel", "coal_ore", "iron_ore", "copper_ore", "gold_ore",
			"redstone_ore", "diamond_ore", "lapis_ore", "cobblestone", "cobblestone_mossy", "chest"})
			registry.registerBlock<ServerBlock>(std::string("default:") + id);
		for (const char *id : {"coal", "iron_ingot", "bread", "gold_ingot", "treasure_note"})
			registry.registerItem<Item>({}, std::string("default:") + id, id);
	}

public:
	void setUp() override {
		static LoggerHandler logger; LoggerHandler::setInstance(logger);
		static GameClock clock; GameClock::setInstance(clock);
	}

	void testVeinClippingPreservesShapeAndRandomSequence() {
		for (int seed = 0; seed < 16; ++seed) {
			retro::Random full(seed), clipped(seed);
			std::set<std::tuple<int,int,int>> expected, actual;
			retro::oreVein(full,-16,-16,29,32,[&](int x,int y,int z) {
				if (z >= 32 && z <= 63 && x >= -8 && x <= -3 && y >= -9 && y <= -5) expected.emplace(x,y,z);
			});
			retro::oreVein(clipped,-16,-16,29,32,[&](int x,int y,int z) { actual.emplace(x,y,z); },retro::OreBounds{-8,-9,32,-3,-5,63});
			TS_ASSERT(expected == actual);
			TS_ASSERT_EQUALS(full.next(32),clipped.next(32));
		}
	}

	void testOrePocketsAreConnectedAndCopperOutnumbersIron() {
		Registry registry; definitions(registry);
		PlayerList players; Dimension dimension{0,"test:world","test"}; ServerWorld world(players,dimension,42);
		u16 stone=registry.getBlockFromStringID("default:stone").id();
		u16 coal=registry.getBlockFromStringID("default:coal_ore").id();
		u16 copper=registry.getBlockFromStringID("default:copper_ore").id();
		u16 iron=registry.getBlockFromStringID("default:iron_ore").id();
		unsigned copperCount=0,ironCount=0,coalPairs=0,dirtCount=0,gravelCount=0;
		for (int cx=-2;cx<=1;++cx) for (int cy=-1;cy<=1;++cy) for (int cz=0;cz<4;++cz) {
			auto &chunk=world.getOrCreateChunk(cx,cy,cz);
			for (int z=0;z<32;++z) for (int y=0;y<16;++y) for (int x=0;x<16;++x) chunk.setBlockRaw(x,y,z,stone);
			chunk.setBlockRaw(0,0,0,0);
			decorateOreTerrain(chunk,42);
			TS_ASSERT_EQUALS(chunk.getBlock(0,0,0),0);
			for (int z=0;z<32;++z) for (int y=0;y<16;++y) for (int x=0;x<16;++x) {
				u16 id=chunk.getBlock(x,y,z);
				copperCount+=id==copper; ironCount+=id==iron;
				dirtCount+=id==registry.getBlockFromStringID("default:dirt").id();
				gravelCount+=id==registry.getBlockFromStringID("default:gravel").id();
				if (id==coal && x<15 && chunk.getBlock(x+1,y,z)==coal) ++coalPairs;
				if (id==copper) TS_ASSERT(z+cz*32>=8 && z+cz*32<=85);
			}
		}
		TS_ASSERT(copperCount>ironCount); TS_ASSERT(ironCount>50);
		TS_ASSERT(coalPairs>100); TS_ASSERT(dirtCount>500); TS_ASSERT(gravelCount>200);
	}

	void testRuinsCrossChunkBoundariesAndProtectChestContents() {
		Registry registry; definitions(registry);
		PlayerList players; Dimension dimension{0,"test:world","test"}; ServerWorld world(players,dimension,42);
		std::optional<Vector3i> center;
		for (int x=-5;x<=5 && !center;++x) for (int y=-5;y<=5 && !center;++y) {
			auto candidate=exploration::ruin(42,x,y);
			if (candidate && (candidate->x&15)>=12) center=candidate;
		}
		TS_ASSERT(center); if (!center) return;
		for (int dx=-5;dx<=5;++dx) for (int dy=-10;dy<=5;++dy) {
			int x=center->x+dx,y=center->y+dy;
			auto &column=world.heightmap().getOrCreateChunk((x&-16)/16,(y&-16)/16);
			column.blocks().fill(1); column.setLandHeight(x&15,y&15,64);
		}
		u16 stone=registry.getBlockFromStringID("default:stone").id();
		for (int cz=0;cz<2;++cz) for (int cy=(center->y-9)&-16;cy<=center->y+4;cy+=16)
			for (int cx=(center->x-4)&-16;cx<=center->x+4;cx+=16) {
				auto &chunk=world.getOrCreateChunk(cx/16,cy/16,cz);
				for (int z=0;z<32;++z) for (int y=0;y<16;++y) for (int x=0;x<16;++x) chunk.setBlockRaw(x,y,z,stone);
				chunk.setModified(true);
				decorateRuinsTerrain(chunk,world.heightmap(),42);
			}
		TS_ASSERT_EQUALS(world.getBlock(center->x,center->y,center->z+1),0);
		TS_ASSERT_EQUALS(world.getBlock(center->x+4,center->y,center->z+1),registry.getBlockFromStringID("default:cobblestone").id());
		auto *chunk=world.getChunkAtBlockPos(center->x+2,center->y+2,center->z+1);
		TS_ASSERT(chunk); if (!chunk) return;
		auto *data=chunk->getBlockData((center->x+2)&15,(center->y+2)&15,(center->z+1)&31);
		TS_ASSERT(data); if (!data) return;
		TS_ASSERT_EQUALS(data->inventory.getStack(0,0).stringID(),"default:coal");
		data->inventory.setStack(0,0,"default:gold_ingot",9);
		decorateRuinsTerrain(*static_cast<ServerChunk *>(chunk),world.heightmap(),42);
		TS_ASSERT_EQUALS(data->inventory.getStack(0,0).amount(),9);
	}

	void testRuinsRejectWaterBesideTheEntrance() {
		Registry registry; definitions(registry);
		PlayerList players; Dimension dimension{0,"test:world","test"}; ServerWorld world(players,dimension,42);
		std::optional<Vector3i> center;
		for (int x=0;x<8 && !center;++x) center=exploration::ruin(42,x,0);
		TS_ASSERT(center); if (!center) return;
		for (int dx=-5;dx<=5;++dx) for (int dy=-10;dy<=5;++dy) {
			int x=center->x+dx,y=center->y+dy;
			auto &column=world.heightmap().getOrCreateChunk((x&-16)/16,(y&-16)/16);
			column.blocks().fill(1); column.setLandHeight(x&15,y&15,64);
		}
		int wetX=center->x,wetY=center->y-10;
		auto &base=world.heightmap().getOrCreateChunk((wetX&-16)/16,(wetY&-16)/16);
		base.blocks()[((wetX&15)*16+(wetY&15))*128+center->z+1]=9;
		auto &chunk=world.getOrCreateChunk((center->x&-16)/16,(center->y&-16)/16,center->z/32);
		u16 stone=registry.getBlockFromStringID("default:stone").id();
		for (int z=0;z<32;++z) for (int y=0;y<16;++y) for (int x=0;x<16;++x) chunk.setBlockRaw(x,y,z,stone);
		decorateRuinsTerrain(chunk,world.heightmap(),42);
		TS_ASSERT_EQUALS(chunk.getBlock(center->x&15,center->y&15,(center->z+1)&31),stone);
	}

	void testTreasureNotesKeepSeparateSavedDestinations() {
		struct Restore {std::filesystem::path original=std::filesystem::current_path(); ~Restore(){std::filesystem::current_path(original);}} restore;
		auto directory=std::filesystem::temp_directory_path()/("opencraft-notes-"+std::to_string(getpid()));
		std::filesystem::remove_all(directory); std::filesystem::create_directories(directory); std::filesystem::current_path(directory);
		auto first=exploration::saveNote("test",{-214,300,20});
		auto second=exploration::saveNote("test",{128,-512,30});
		TS_ASSERT(first); TS_ASSERT_DIFFERS(first,second);
		auto target=exploration::loadNote("test",first);
		TS_ASSERT(target); if (target) TS_ASSERT(*target == Vector3i(-214,300,20));
		TS_ASSERT(!exploration::loadNote("other",first));
		TS_ASSERT_EQUALS(exploration::cellAt(-1),-1); TS_ASSERT_EQUALS(exploration::cellAt(-256),-1);
	}

	void testReadingANoteAssignsOneDestinationWithoutConsumingIt() {
		struct Restore {std::filesystem::path original=std::filesystem::current_path(); ~Restore(){std::filesystem::current_path(original);}} restore;
		auto directory=std::filesystem::temp_directory_path()/("opencraft-note-use-"+std::to_string(getpid()));
		std::filesystem::remove_all(directory); std::filesystem::create_directories(directory); std::filesystem::current_path(directory);
		Registry registry; definitions(registry);
		auto &note=registry.registerItem<ServerItem>({},"test:note","Note");
		note.setMaxStackSize(1); note.addGroup("group:om_treasure_note",1);
		PlayerList players; ScriptEngine scripts; Server server; WorldController controller(registry);
		controller.init(players,42); controller.load("activation");
		ServerCommandHandler handler(scripts,server,controller,players,registry);
		ClientInfo client{0,std::make_shared<Network::LocalSession>()};
		auto &player=players.addPlayer("tester",false); players.connectPlayer("tester",client);
		player.inventory()=Inventory(9,4,"main"); player.setHeldItemSlot(0); player.setPosition(-1,128,60);
		player.inventory().setStack(0,0,"test:note",1);
		Dimension dimension{0,"test:notes","notes"}; ServerWorld world(players,dimension,42);
		Block air{0,BLOCK_AIR};
		TS_ASSERT(note.onItemActivated({0,0,0},air,player,world,client,handler));
		u16 id=player.heldItemStack().damage(); TS_ASSERT(id);
		auto destination=exploration::loadNote("activation",id); TS_ASSERT(destination);
		player.setPosition(2000,-2000,60);
		TS_ASSERT(note.onItemActivated({0,0,0},air,player,world,client,handler));
		TS_ASSERT_EQUALS(player.heldItemStack().damage(),id);
		TS_ASSERT_EQUALS(player.heldItemStack().amount(),1);
		TS_ASSERT(exploration::loadNote("activation",id)==destination);
	}

	void testRareMonstersSpawnWithPersistentNamesAndKeepNormalModels() {
		Registry registry; definitions(registry);
		auto model=registry.registerEntity("default:zombie"); auto &models=registry.entityRegistry();
		models.emplace<MobComponent>(model).health=20;
		models.emplace<BetaMobComponent>(model).kind=BetaMobKind::Zombie;
		models.emplace<DoubleBox>(model,-0.25,-0.25,0,0.5,0.5,1.8);
		models.emplace<RotationComponent>(model);
		models.emplace<DrawableDef>(model).setMobModel(2);
		PlayerList players; Dimension dimension{0,"test:world","test"}; ServerWorld world(players,dimension,42);
		world.getOrCreateChunk(0,0,0).setInitialized(true);
		auto &scene=world.scene().registry();
		for (unsigned attempt=0;attempt<3000 && scene.view<RareMobComponent>().size()<2;++attempt) {
			entt::entity entity=entt::null;
			TS_ASSERT(world.spawnMob("default:zombie",8,8,20,0,&entity));
			if (!scene.all_of<RareMobComponent>(entity)) scene.destroy(entity);
		}
		TS_ASSERT_EQUALS(scene.view<RareMobComponent>().size(),2);
		scene.view<RareMobComponent,MobComponent,DrawableDef>().each([&](auto,auto &rare,auto &mob,auto &visual) {
			TS_ASSERT(!rare.name.empty()); TS_ASSERT_EQUALS(mob.health,30); TS_ASSERT(visual.mobAppearance()&128);
			sf::Packet packet; packet<<rare<<u32(123); RareMobComponent restored; u32 marker=0; packet>>restored>>marker;
			TS_ASSERT_EQUALS(restored.name,rare.name); TS_ASSERT_EQUALS(marker,123u); TS_ASSERT(!restored.announced);
		});
		entt::entity extra=entt::null;
		TS_ASSERT(world.spawnMob("default:zombie",8,8,20,0,&extra));
		TS_ASSERT(!scene.all_of<RareMobComponent>(extra));
		registry.registerEntity("default:item_drop");
		scene.view<RareMobComponent,MobComponent>().each([](auto,auto &,auto &mob) { mob.health=0; });
		world.updateMobs();
		unsigned notes=0,ingots=0;
		scene.view<ItemStack>().each([&](auto,const auto &stack) {
			if (stack.stringID()=="default:treasure_note") notes+=stack.amount();
			if (stack.stringID()=="default:iron_ingot") ingots+=stack.amount();
		});
		TS_ASSERT_EQUALS(notes,2); TS_ASSERT_EQUALS(ingots,4);
	}
};
#endif
