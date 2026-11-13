#ifndef TREEBIOMETESTS_HPP_
#define TREEBIOMETESTS_HPP_

#include <filesystem>
#include <set>
#include <cxxtest/TestSuite.h>
#include "common/core/GameClock.hpp"
#include "common/core/Registry.hpp"
#include "common/world/Heightmap.hpp"
#include "common/world/TreeVariantShape.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/lua/ServerModLoader.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/TerrainBiomeSampler.hpp"
#include "server/world/WorldController.hpp"

class TreeBiomeTests : public CxxTest::TestSuite {
public:
	void setUp() override {
		static LoggerHandler logger; LoggerHandler::setInstance(logger);
		static GameClock clock; GameClock::setInstance(clock);
	}
	void testDecorationBiomesKeepBetaGroundAndOccurInSeparateRegions() {
		struct Restore { std::filesystem::path path=std::filesystem::current_path(); ~Restore(){std::filesystem::current_path(path);} } restore;
		std::filesystem::current_path(OM_TEST_SOURCE_DIR);
		ScriptEngine scripts; Registry registry; Registry::setInstance(registry);
		registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
		registry.registerItem<Item>({},BLOCK_AIR,"air");
		WorldController worlds(registry); ServerModLoader loader(scripts,worlds); loader.loadMods();
		const auto &dimension=registry.dimensions().front();
		std::set<std::string> variants;
		unsigned seasonal=0,cherry=0;
		for (int seed:{0,42,-99}) {
			TerrainBiomeSampler sampler(dimension,seed);
			for (int x=-4096; x<=4096; x+=256) for (int y=-4096; y<=4096; y+=256) {
				for (auto base:{retro::Biome::Rainforest,retro::Biome::Savanna,retro::Biome::Forest,retro::Biome::SeasonalForest}) {
					const auto &original=registry.getBiome(sampler.getBetaBiomeIndex(base));
					const auto &decorated=registry.getBiome(sampler.getDecorationBiomeIndex(base,x,y));
					variants.insert(decorated.stringID());
					if(base==retro::Biome::SeasonalForest) {++seasonal;if(decorated.stringID()=="default:cherry_grove")++cherry;}
					TS_ASSERT_EQUALS(original.getTopBlockID(),decorated.getTopBlockID());
					TS_ASSERT_EQUALS(original.getGroundBlockID(),decorated.getGroundBlockID());
					TS_ASSERT_EQUALS(original.getDeepBlockID(),decorated.getDeepBlockID());
					TS_ASSERT_EQUALS(original.getLiquidBlockID(),decorated.getLiquidBlockID());
				}
			}
			Heightmap heightmap(seed);
			for (int cx:{-1,0,17}) {
				auto &chunk=heightmap.getOrCreateChunk(cx,2);
				auto original=chunk.blocks();
				for (int x=0;x<16;++x) for (int y=0;y<16;++y)
					sampler.getDecorationBiomeIndex(chunk.biomeAt(x,y),cx*16+x,32+y);
				TS_ASSERT(original==chunk.blocks());
			}
		}
		TS_ASSERT(cherry>0 && cherry*50<seasonal);
		TS_ASSERT(registry.getBiomeFromStringID("default:cherry_grove").getTrees().front().probability<0.005);
		for (const char *id:{"default:jungle","default:acacia_savanna","default:birch_forest","default:dark_forest","default:cherry_grove"})
			TS_ASSERT(variants.count(id));
	}
	void testNaturalVariantTreesUseTheSameCrownsOnFlatBetaGround() {
		struct Restore { std::filesystem::path path=std::filesystem::current_path(); ~Restore(){std::filesystem::current_path(path);} } restore;
		std::filesystem::current_path(OM_TEST_SOURCE_DIR);
		ScriptEngine scripts; Registry registry; Registry::setInstance(registry);
		registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
		registry.registerItem<Item>({},BLOCK_AIR,"air");
		WorldController worlds(registry); ServerModLoader loader(scripts,worlds); loader.loadMods();
		for (const char *species:{"jungle","acacia","dark_oak"}) {
			std::string treeID=std::string("default:")+species;
			auto &biome=registry.registerBiome(std::string("test:")+species,species);
			biome.setTopBlockID(registry.getBlockFromStringID("default:grass").id());
			biome.setGroundBlockID(registry.getBlockFromStringID("default:dirt").id());
			biome.setDeepBlockID(registry.getBlockFromStringID("default:stone").id());
			biome.setLiquidBlockID(registry.getBlockFromStringID("default:water").id());
			auto &placement=biome.addTree(); placement.treeID=registry.getTreeFromStringID(treeID).id(); placement.probability=1;
			Dimension dimension{0,"test:trees","trees"}; dimension.addBiome(biome.stringID());
			PlayerList players; ServerWorld world(players,dimension,42);
			auto &terrain=world.heightmap().getOrCreateChunk(0,0); terrain.blocks().fill(0);
			for (int x=0;x<16;++x) for (int y=0;y<16;++y) {
				terrain.setLandHeight(s8(x),s8(y),72);
				for (int z=0;z<=72;++z) terrain.blocks()[(x*16+y)*128+z]=z==72 ? 2 : 1;
			}
			auto &chunk=world.getOrCreateChunk(0,0,2); world.generateChunk(chunk);
			unsigned roots=0; u16 log=registry.getBlockFromStringID(treeID+"_wood").id();
			for (int x=0;x<16;++x) for (int y=0;y<16;++y) if (chunk.getBlock(x,y,9)==log) ++roots;
			TS_ASSERT(roots>0);
			if (treeID=="default:dark_oak") TS_ASSERT_EQUALS(roots%4,0u);
		}
	}
	void testDistinctiveCrownsAreDeterministicAndKeepLogsInsideTheirBounds() {
		for (const char *id:{"default:jungle","default:acacia","default:dark_oak"}) {
			retro::Random a(42),b(42);
			auto cells=retro::variantTreeShape(a,id,7), same=retro::variantTreeShape(b,id,7);
			TS_ASSERT_EQUALS(cells.size(),same.size());
			unsigned baseLogs=0,offsetLogs=0;
			for (std::size_t i=0;i<cells.size();++i) {
				TS_ASSERT_EQUALS(cells[i].position,same[i].position); TS_ASSERT_EQUALS(cells[i].log,same[i].log);
				const auto &p=cells[i].position;
				TS_ASSERT(p.x>=-4 && p.x<=4 && p.y>=-4 && p.y<=4 && p.z>=0 && p.z<=8);
				if (cells[i].log) { if(!p.z) ++baseLogs; if(p.x || p.y) ++offsetLogs; }
			}
			TS_ASSERT_EQUALS(baseLogs,std::string(id)=="default:dark_oak" ? 4u : 1u);
			if(std::string(id)=="default:acacia") TS_ASSERT(offsetLogs>0);
			if(std::string(id)=="default:jungle") TS_ASSERT_EQUALS(offsetLogs,0u);
		}
	}
};
#endif
