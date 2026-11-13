#ifndef AMETHYSTTESTS_HPP_
#define AMETHYSTTESTS_HPP_

#include <cxxtest/TestSuite.h>
#include <filesystem>
#include "common/core/GameClock.hpp"
#include "common/core/Registry.hpp"
#include "common/world/BlockSupport.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/lua/ServerModLoader.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/AmethystTerrain.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/WorldController.hpp"

class AmethystTests : public CxxTest::TestSuite {
public:
	void setUp() override {
		static LoggerHandler logger; LoggerHandler::setInstance(logger);
		static GameClock clock; GameClock::setInstance(clock);
		BlockGeometry::initOrientation();
	}
	void testBuddingAmethystGrowsMountedStagesAndDropsShards() {
		struct Restore {std::filesystem::path original=std::filesystem::current_path(); ~Restore(){std::filesystem::current_path(original);}} restore;
		std::filesystem::current_path(OM_TEST_SOURCE_DIR);
		ScriptEngine scripts; Registry registry; Registry::setInstance(registry);
		registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
		registry.registerItem<Item>({},BLOCK_AIR,"air").setIsBlock(true);
		WorldController worlds(registry); ServerModLoader loader(scripts,worlds); loader.loadMods();
		PlayerList players; worlds.init(players,42); auto &world=worlds.getWorld(0);
		auto &chunk=world.getOrCreateChunk(0,0,0); chunk.setInitialized(true);
		auto &budding=static_cast<const ServerBlock &>(registry.getBlockFromStringID("default:budding_amethyst"));
		world.setBlock(8,8,8,budding.id());
		Server server; ServerCommandHandler commands(scripts,server,worlds,players,registry);
		auto result=scripts.lua().safe_script("math.random=function(n) return 1 end",sol::script_pass_on_error); TS_ASSERT(result.valid());
		for (const char *name:{"small_amethyst_bud","medium_amethyst_bud","large_amethyst_bud","amethyst_cluster"}) {
			budding.onTick({8,8,8},chunk,world,commands);
			const auto *state=world.getBlockState(8,8,9);
			TS_ASSERT_EQUALS(state->block().stringID(),std::string("default:")+name);
			TS_ASSERT(blockHasSupport(world,{8,8,9},*state,world.getData(8,8,9)));
		}
		const auto drops=world.getBlockState(8,8,9)->getItemDrops();
		TS_ASSERT_EQUALS(drops.size(),1u);
		if (!drops.empty()) { TS_ASSERT_EQUALS(drops.front().stringID(),"default:amethyst_shard"); TS_ASSERT_EQUALS(drops.front().amount(),4); }
		TS_ASSERT(budding.getState(0).getItemDrops().empty());
		TS_ASSERT_EQUALS(Chunk::lightEmission(*world.getBlockState(8,8,9)),5);
		world.setBlock(8,8,8,0); world.blockPhysics().tick(256);
		TS_ASSERT_EQUALS(world.getBlock(8,8,9),0);
	}
	void testAmethystUsesSixAnchorsAndGenerationPreservesCaveSpace() {
		Registry registry; Registry::setInstance(registry);
		auto &air=registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0); air.isCollidable(false); air.isOpaque(false);
		registry.registerItem<Item>({},BLOCK_AIR,"air");
		u16 stone=registry.registerBlock<ServerBlock>("default:stone").id();
		u16 amethyst=registry.registerBlock<ServerBlock>("default:amethyst_block").id();
		u16 budding=registry.registerBlock<ServerBlock>("default:budding_amethyst").id();
		for (const char *id:{"small_amethyst_bud","medium_amethyst_bud","large_amethyst_bud","amethyst_cluster"}) {
			auto &cluster=registry.registerBlock<ServerBlock>(std::string("default:")+id);
			cluster.setRotatable(true); cluster.param().allocateBits(BlockParam::Rotation,5);
			cluster.addGroup("group:attached_node",1); cluster.addGroup("group:om_amethyst_cluster",1);
			cluster.getState(0).isOpaque(false); cluster.getState(0).isCollidable(false);
		}
		PlayerList players; Dimension dimension{0,"test:world","world"};
		ServerWorld first(players,dimension,42), second(players,dimension,42);
		unsigned count=0;
		for (int cx=-4;cx<4;++cx) for (int cy=-2;cy<2;++cy) {
			auto prepare=[&](ServerWorld &world) -> ServerChunk & {
				auto &base=world.heightmap().getOrCreateChunk(cx,cy); base.blocks().fill(1);
				auto &chunk=world.getOrCreateChunk(cx,cy,0);
				for (int x=0;x<CHUNK_WIDTH;++x) for (int y=0;y<CHUNK_DEPTH;++y) {
					base.setLandHeight(s8(x),s8(y),70);
					for (int z=0;z<CHUNK_HEIGHT;++z) {
						bool cave=z>=8 && z<=20;
						chunk.setBlockRaw(x,y,z,cave?0:stone);
						if(cave) base.blocks()[(x*16+y)*128+z]=0;
					}
				}
				return chunk;
			};
			auto &a=prepare(first); auto &b=prepare(second);
			decorateAmethyst(a,first.heightmap(),42); decorateAmethyst(b,second.heightmap(),42);
			a.setInitialized(true); b.setInitialized(true);
			for(int x=0;x<CHUNK_WIDTH;++x) for(int y=0;y<CHUNK_DEPTH;++y) for(int z=0;z<CHUNK_HEIGHT;++z) {
				TS_ASSERT_EQUALS(a.data(x,y,z),b.data(x,y,z));
				u16 block=a.getBlock(x,y,z); bool cave=z>=8 && z<=20;
				if (block && block!=stone && block!=amethyst && block!=budding) {
					++count; TS_ASSERT(cave);
					TS_ASSERT(blockHasSupport(first,{cx*16+x,cy*16+y,z},*a.getBlockState(x,y,z),a.getData(x,y,z)));
				} else if (!cave) { TS_ASSERT(block==stone || block==amethyst || block==budding); }
				else { TS_ASSERT_EQUALS(block,0); }
			}
		}
		TS_ASSERT(count>0);
		auto &chunk=first.getOrCreateChunk(0,0,0);
		auto &cluster=registry.getBlockFromStringID("default:amethyst_cluster");
		static const Vector3i sides[]={{0,0,1},{0,0,-1},{0,1,0},{0,-1,0},{1,0,0},{-1,0,0}};
		static constexpr u16 rotations[]={0,4,8,12,20,16};
		for (unsigned face=0;face<6;++face) {
			for (auto side:sides) chunk.setBlockRaw(8-side.x,8-side.y,12-side.z,stone);
			chunk.setBlockRaw(8,8,12,cluster.id()); u16 data=cluster.param().setParam(BlockParam::Rotation,0,rotations[face]);
			TS_ASSERT(blockHasSupport(first,{8,8,12},cluster.getState(0),data));
			auto anchor=Vector3i{8,8,12}-sides[face]; chunk.setBlockRaw(anchor.x,anchor.y,anchor.z,0);
			TS_ASSERT(!blockHasSupport(first,{8,8,12},cluster.getState(0),data));
		}
	}
};
#endif
