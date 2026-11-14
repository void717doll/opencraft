#ifndef PLANTPHYSICSTESTS_HPP_
#define PLANTPHYSICSTESTS_HPP_

#include <cxxtest/TestSuite.h>
#include <filesystem>
#include "common/core/GameClock.hpp"
#include "common/core/Registry.hpp"
#include "common/world/BlockSupport.hpp"
#include "common/world/BetaPlayerMovement.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/lua/ServerModLoader.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/WorldController.hpp"

class PlantPhysicsTests : public CxxTest::TestSuite {
	struct Fixture {
		std::filesystem::path directory = std::filesystem::current_path();
		ScriptEngine scripts;
		Registry registry;
		PlayerList players;
		WorldController worlds{registry};
		Fixture() {
			std::filesystem::current_path(OM_TEST_SOURCE_DIR);
			Registry::setInstance(registry);
			auto &air = registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0);
			air.isOpaque(false); air.isCollidable(false);
			registry.registerItem<Item>({},BLOCK_AIR,"air").setIsBlock(true);
			ServerModLoader loader(scripts,worlds); loader.loadMods();
			worlds.init(players,42);
			auto &world=worlds.getWorld(0);
			auto &center=world.getOrCreateChunk(0,0,0); center.setInitialized(true);
			for(unsigned side=0;side<6;++side) center.getSurroundingChunk(side)->setInitialized(true);
		}
		~Fixture() { std::filesystem::current_path(directory); }
		u16 id(const char *name) { return registry.getBlockFromStringID(name).id(); }
	};
public:
	void setUp() override {
		static LoggerHandler logger; LoggerHandler::setInstance(logger);
		static GameClock clock; GameClock::setInstance(clock);
	}
	void testGrassAndFernsAreReplaceableButFlowersAndSaplingsArePreserved() {
		Fixture f;
		for(const char *name:{"default:tallgrass","default:fern"}) {
			const auto &state=f.registry.getBlockFromStringID(name).getState(0);
			TS_ASSERT(isReplaceableBlock(state)); TS_ASSERT(!state.isCollidable());
		}
		for(const char *name:{"default:rose","default:dandelion","default:sapling","default:jungle_sapling","default:mushroom_brown"}) {
			const auto &state=f.registry.getBlockFromStringID(name).getState(0);
			TS_ASSERT(!isReplaceableBlock(state)); TS_ASSERT(!state.isCollidable());
		}
		TS_ASSERT(isReplaceableBlock(f.registry.getBlockFromStringID("default:water").getState(0)));
		auto &world=f.worlds.getWorld(0);
		world.setBlock(8,8,1,f.id("default:tallgrass"));
		DoubleBox player{7,8.2,1,0.6,0.6,1.8};
		auto movement=retro::movePlayer(world,player,{2,0,0},false,false);
		TS_ASSERT_DELTA(movement.x,2,0.00001);
	}
	void testTallGrassAndFernsBreakUnderSolidCover() {
		Fixture f; auto &world = f.worlds.getWorld(0);
		for (const char *plant : {"default:tallgrass", "default:fern"}) {
			world.setBlock(8,8,1,f.id("default:dirt"));
			world.setBlock(8,8,2,f.id(plant));
			world.setBlock(8,8,3,f.id("default:torch"));
			TS_ASSERT(blockHasSupport(world,{8,8,2},f.registry.getBlockFromStringID(plant).getState(0),0));
			world.setBlock(8,8,3,f.id("default:stone"));
			TS_ASSERT(!blockHasSupport(world,{8,8,2},f.registry.getBlockFromStringID(plant).getState(0),0));
			world.blockPhysics().tick(8192);
			TS_ASSERT_EQUALS(world.getBlock(8,8,2),0);
			world.setBlock(8,8,3,0);
			world.setBlock(8,8,2,f.id(plant));
			world.blockPhysics().tick(8192);
			TS_ASSERT_EQUALS(world.getBlock(8,8,2),f.id(plant));
		}
	}
	void testLeavesDecayWithinFourConnectionsAndWaitForLoadedNeighbors() {
		Fixture f; auto &world=f.worlds.getWorld(0);
		u16 leaves=f.id("default:oak_leaves"), log=f.id("default:oak_wood");
		world.setBlock(6,8,8,log);
		for (int x=7;x<=11;++x) world.setBlock(x,8,8,leaves);
		world.blockPhysics().markLeaves({8,8,8},4);
		for(int tick=0;tick<8;++tick) world.blockPhysics().tick(8192);
		TS_ASSERT_EQUALS(world.getBlock(10,8,8),leaves);
		TS_ASSERT_EQUALS(world.getBlock(11,8,8),0);
		world.setBlock(6,8,8,0);
		for(int tick=0;tick<8;++tick) world.blockPhysics().tick(8192);
		TS_ASSERT_EQUALS(world.getBlock(7,8,8),0);
		TS_ASSERT_EQUALS(world.getBlock(8,8,8),0);
		TS_ASSERT_EQUALS(world.getBlock(9,8,8),0);
		TS_ASSERT_EQUALS(world.getBlock(10,8,8),0);
		world.setBlock(15,8,8,leaves);
		auto *neighbor=world.getChunkAtBlockPos(16,8,8); neighbor->setInitialized(false);
		world.blockPhysics().markLeaves({15,8,8},1);
		world.blockPhysics().tick(8192);
		TS_ASSERT_EQUALS(world.getBlock(15,8,8),leaves);
		neighbor->setInitialized(true);
		world.blockPhysics().tick(8192);
		TS_ASSERT_EQUALS(world.getBlock(15,8,8),0);
		const auto &fern=f.registry.getBlockFromStringID("default:fern").getState(0);
		TS_ASSERT(fern.colorMultiplier().g > fern.colorMultiplier().r);
		TS_ASSERT(fern.colorMultiplier().g > fern.colorMultiplier().b);
		const auto &shears=f.registry.getItemFromStringID("default:shears");
		TS_ASSERT_EQUALS(fern.getItemDrops(&shears).front().stringID(),"default:fern");
		for(int attempt=0;attempt<100;++attempt) for(const auto &drop:fern.getItemDrops()) TS_ASSERT_DIFFERS(drop.stringID(),"default:fern");
	}
	void testSoilPlantsRejectStoneAndDropWhenTheirGroundChanges() {
		Fixture f; auto &world=f.worlds.getWorld(0);
		const auto &rose=f.registry.getBlockFromStringID("default:rose").getState(0);
		world.setBlock(8,8,1,f.id("default:dirt"));
		TS_ASSERT(blockHasSupport(world,{8,8,2},rose,0));
		world.setBlock(8,8,2,rose.block().id());
		world.setBlock(8,8,1,f.id("default:stone"));
		TS_ASSERT(!blockHasSupport(world,{8,8,2},rose,0));
		world.blockPhysics().tick(8192);
		TS_ASSERT_EQUALS(world.getBlock(8,8,2),0);
		unsigned flowers=0;
		world.scene().registry().view<ItemStack>().each([&](const auto &stack) { if(stack.stringID()=="default:rose") flowers+=stack.amount(); });
		TS_ASSERT_EQUALS(flowers,1u);
		for(const char *name:{"default:tallgrass","default:fern","default:dandelion","default:sapling"})
			TS_ASSERT(!blockHasSupport(world,{8,8,2},f.registry.getBlockFromStringID(name).getState(0),0));
	}
	void testWaterWashesFlowersAndKeepsTheirDrops() {
		Fixture f; auto &world=f.worlds.getWorld(0);
		for(int y=5;y<=11;++y) for(int x=5;x<=11;++x) world.setBlock(x,y,0,f.id("default:dirt"));
		world.setBlock(8,8,1,f.id("default:rose"));
		world.setBlock(8,7,1,f.id("default:water"));
		for(unsigned tick=0;tick<30;++tick) world.blockPhysics().tick(8192);
		TS_ASSERT_EQUALS(world.getBlock(8,8,1),f.id("default:water_flowing"));
		unsigned flowers=0;
		world.scene().registry().view<ItemStack>().each([&](const auto &stack) { if(stack.stringID()=="default:rose") flowers+=stack.amount(); });
		TS_ASSERT_EQUALS(flowers,1u);
	}
	void testAmethystNeedsAFullTouchingSupportFace() {
		Fixture f; auto &world=f.worlds.getWorld(0);
		if(!f.registry.hasBlock("default:amethyst_cluster")) { TS_FAIL("amethyst module must be loaded"); return; }
		const auto &cluster=f.registry.getBlockFromStringID("default:amethyst_cluster").getState(0);
		world.setBlock(8,8,1,f.id("default:oak_fence"));
		TS_ASSERT(!blockHasSupport(world,{8,8,2},cluster,0));
		world.setBlock(8,8,1,f.id("default:stone"));
		TS_ASSERT(blockHasSupport(world,{8,8,2},cluster,0));
		world.setBlock(8,8,2,cluster.block().id());
		world.setBlock(8,7,2,f.id("default:water"));
		for(unsigned tick=0;tick<10;++tick) world.blockPhysics().tick(8192);
		TS_ASSERT_EQUALS(world.getBlock(8,8,2),cluster.block().id());
	}
};
#endif
