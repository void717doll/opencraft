#ifndef BUILDINGVARIANTSTESTS_HPP_
#define BUILDINGVARIANTSTESTS_HPP_

#include <filesystem>
#include <cxxtest/TestSuite.h>
#include "common/core/GameClock.hpp"
#include "common/core/Registry.hpp"
#include "common/inventory/Recipe.hpp"
#include "common/world/BetaMining.hpp"
#include "common/world/BlockSupport.hpp"
#include "common/world/Dimension.hpp"
#include "common/world/StairGeometry.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/lua/ServerModLoader.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/WorldController.hpp"

class BuildingVariantsTests : public CxxTest::TestSuite {
public:
	void testSpeciesRecipesSlabMergingDropsAndTools() {
		struct Restore {std::filesystem::path path=std::filesystem::current_path(); ~Restore(){std::filesystem::current_path(path);}} restore;
		std::filesystem::current_path(OM_TEST_SOURCE_DIR);
		static LoggerHandler logger; LoggerHandler::setInstance(logger); static GameClock clock; GameClock::setInstance(clock);
		ScriptEngine scripts; Registry registry; Registry::setInstance(registry);
		auto &air=registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0); air.isCollidable(false); air.isOpaque(false);
		registry.registerItem<Item>({},BLOCK_AIR,"air").setIsBlock(true);
		WorldController worlds(registry); ServerModLoader loader(scripts,worlds); loader.loadMods();
		PlayerList players; Dimension dimension{0,"test:shapes","shapes"}; ServerWorld world(players,dimension,42);
		auto &chunk=world.getOrCreateChunk(0,0,0); chunk.setInitialized(true);
		const auto &axe=registry.getItemFromStringID("default:diamond_axe");
		const auto &pick=registry.getItemFromStringID("default:diamond_pickaxe");
		for(const char *name:{"oak","birch","spruce","cherry","jungle","acacia","dark_oak","amethyst"}) {
			std::string base=std::string("default:")+name;
			std::string parent=base+(std::string(name)=="amethyst" ? "_block" : "_planks");
			Inventory grid(3,3); for(int x=0;x<3;++x) grid.setStack(x,0,parent,1);
			auto *recipe=registry.getRecipe(grid,"craft"); TS_ASSERT(recipe);
			if(recipe) {TS_ASSERT_EQUALS(recipe->result().stringID(),base+"_slab");TS_ASSERT_EQUALS(recipe->result().amount(),3);}
			const auto &slab=registry.getBlockFromStringID(base+"_slab");
			world.setBlock(8,8,1,slab.id()); Vector3i destination{8,8,2};
			TS_ASSERT(resolveSlabPlacement(world,destination,slab));TS_ASSERT_EQUALS(destination,Vector3i(8,8,1));
			world.setBlockState(8,8,1,1); auto drops=world.getBlockState(8,8,1)->getItemDrops();
			TS_ASSERT_EQUALS(drops.size(),1u); if(!drops.empty()){TS_ASSERT_EQUALS(drops[0].stringID(),base+"_slab");TS_ASSERT_EQUALS(drops[0].amount(),2);}
			world.setBlock(8,8,1,0); world.setData(8,8,1,0);
			for(bool mirrored:{false,true}) {
				grid=Inventory(3,3);
				for(int y=0;y<3;++y) for(int x=0;x<=y;++x) grid.setStack(mirrored ? 2-x : x,y,parent,1);
				recipe=registry.getRecipe(grid,"craft");TS_ASSERT(recipe);
				if(recipe){TS_ASSERT_EQUALS(recipe->result().stringID(),base+"_stairs");TS_ASSERT_EQUALS(recipe->result().amount(),4);}
			}
			const auto &stairs=registry.getBlockFromStringID(base+"_stairs");
			drops=stairs.getState(0).getItemDrops();TS_ASSERT_EQUALS(drops.size(),1u);
			if(!drops.empty()) TS_ASSERT_EQUALS(drops[0].stringID(),parent);
			if(std::string(name)!="oak") {
				bool wood=std::string(name)!="amethyst";
				TS_ASSERT_EQUALS(slab.hasGroup("group:om_material_wood"),wood);
				TS_ASSERT(retro::breakTime(slab.getState(0),wood ? axe : pick)<retro::breakTime(slab.getState(0),wood ? pick : axe));
				TS_ASSERT(retro::breakTime(stairs.getState(0),wood ? axe : pick)<retro::breakTime(stairs.getState(0),wood ? pick : axe));
			}
		}
	}
};
#endif
