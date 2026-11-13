#ifndef TREEVARIANTSTESTS_HPP_
#define TREEVARIANTSTESTS_HPP_

#include <cxxtest/TestSuite.h>
#include <filesystem>
#include "common/core/GameClock.hpp"
#include "common/core/GameTime.hpp"
#include "common/core/Registry.hpp"
#include "common/inventory/Recipe.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/lua/ServerModLoader.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/WorldController.hpp"

class TreeVariantsTests : public CxxTest::TestSuite {
public:
	void setUp() override {
		static LoggerHandler logger; LoggerHandler::setInstance(logger);
		static GameClock clock; GameClock::setInstance(clock);
	}
	void testRefiWoodVariantsCraftDropAndGrowFromTheirSaplings() {
		struct Restore { std::filesystem::path directory=std::filesystem::current_path(); u64 ticks=GameTime::getTicks(); ~Restore(){std::filesystem::current_path(directory); GameTime::setTicks(ticks);} } restore;
		std::filesystem::current_path(OM_TEST_SOURCE_DIR);
		ScriptEngine scripts; Registry registry; Registry::setInstance(registry);
		auto &air=registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0); air.isCollidable(false); air.isOpaque(false);
		registry.registerItem<Item>({},BLOCK_AIR,"air").setIsBlock(true);
		WorldController worlds(registry); ServerModLoader loader(scripts,worlds); loader.loadMods();
		scripts.luaCore().setRegistry(&registry); GameTime::setTicks(6000);
		for (const char *species:{"jungle","acacia","dark_oak"}) {
			std::string id=std::string("default:")+species;
			TS_ASSERT(registry.hasTree(id));
			Inventory grid(3,3); grid.setStack(0,0,id+"_wood",1);
			const Recipe *planks=registry.getRecipe(grid); TS_ASSERT(planks);
			if (planks) { TS_ASSERT_EQUALS(planks->result().stringID(),id+"_planks"); TS_ASSERT_EQUALS(planks->result().amount(),4); }
			Inventory furnace(3,1); furnace.setStack(0,0,id+"_wood",1); furnace.setStack(2,0,"default:coal",1);
			const Recipe *coal=registry.getRecipe(furnace); TS_ASSERT(coal);
			if (coal) TS_ASSERT_EQUALS(coal->result().stringID(),"default:charcoal");
			const auto &leaves=registry.getBlockFromStringID(id+"_leaves").getState(0);
			unsigned dropped=0;
			for (int i=0;i<2000;++i) for (const auto &drop:leaves.getItemDrops()) {
				++dropped; TS_ASSERT_EQUALS(drop.stringID(),id+"_sapling"); TS_ASSERT_EQUALS(drop.amount(),1);
			}
			TS_ASSERT(dropped>0 && dropped<250);
			const auto &shears=registry.getItemFromStringID("default:shears");
			auto sheared=leaves.getItemDrops(&shears); TS_ASSERT_EQUALS(sheared.size(),1u);
			if (!sheared.empty()) TS_ASSERT_EQUALS(sheared[0].stringID(),id+"_leaves");
			PlayerList players; Dimension dimension{0,"test:woods","woods"}; ServerWorld world(players,dimension,42);
			auto &chunk=world.getOrCreateChunk(0,0,4); chunk.setInitialized(true);
			for (int z=0;z<CHUNK_HEIGHT;++z) for (int y=0;y<CHUNK_DEPTH;++y) for (int x=0;x<CHUNK_WIDTH;++x) chunk.lightmap().setLightDataRaw(x,y,z,0xf0);
			world.setBlock(8,8,130,registry.getBlockFromStringID("default:dirt").id());
			world.setBlock(8,8,131,registry.getBlockFromStringID(id+"_sapling").id());
			TS_ASSERT(world.growTree(8,8,131,id));
			TS_ASSERT_EQUALS(world.getBlock(8,8,131),registry.getBlockFromStringID(id+"_wood").id());
		}
	}
};
#endif // TREEVARIANTSTESTS_HPP_
