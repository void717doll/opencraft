#ifndef ROPECACHETESTS_HPP_
#define ROPECACHETESTS_HPP_
#include <cxxtest/TestSuite.h>
#include <filesystem>
#include <unistd.h>
#include "common/core/GameClock.hpp"
#include "common/core/Registry.hpp"
#include "common/inventory/Recipe.hpp"
#include "common/world/BetaPlayerMovement.hpp"
#include "common/world/Dimension.hpp"
#include "common/world/Exploration.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/lua/ServerModLoader.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/WorldController.hpp"
#include "server/world/RopeInteraction.hpp"
#include "server/world/RuinsTerrain.hpp"

class RopeCacheTests : public CxxTest::TestSuite {
	struct RopeWorld {
		Registry definitions; PlayerList players; Dimension dimension{0,"test:rope","rope"};
		std::unique_ptr<ServerWorld> world;
		u16 stone=0,rope=0;
		RopeWorld() {
			Registry::setInstance(definitions);
			definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
			definitions.registerItem<Item>({},BLOCK_AIR,"air");
			definitions.registerItem<Item>({},"default:climbing_rope","rope");
			definitions.registerEntity("default:item_drop");
			stone=definitions.registerBlock<ServerBlock>("default:stone").id();
			auto &block=definitions.registerBlock<ServerBlock>("default:rope"); rope=block.id();
			block.addGroup("group:om_rope",1); block.addGroup("group:climbable",1);
			block.addGroup("group:attached_node",4); block.addGroup("group:om_hanging_chain",1);
			block.getState(0).isCollidable(false); block.getState(0).isOpaque(false);
			block.getState(0).itemDrop("default:climbing_rope");
			world=std::make_unique<ServerWorld>(players,dimension,42);
			for(int z=0;z<2;++z) world->getOrCreateChunk(0,0,z).setInitialized(true);
		}
	};
public:
	void setUp() override {
		static LoggerHandler logger; LoggerHandler::setInstance(logger);
		static GameClock clock; GameClock::setInstance(clock);
	}
	void testRopeLowersInBatchesStopsAt32AndReturnsEverySection() {
		RopeWorld fixture; auto &world=*fixture.world;
		Player player; player.inventory()=Inventory(9,4,"main"); player.setHeldItemSlot(0);
		player.inventory().setStack(0,0,"default:climbing_rope",99);
		world.setBlock(0,8,40,fixture.stone);
		TS_ASSERT(!useClimbingRope(world,player,{0,8,40},0));
		TS_ASSERT_EQUALS(player.heldItemStack().amount(),99);
		for(int i=0;i<4;++i) TS_ASSERT(useClimbingRope(world,player,{0,8,40},5));
		TS_ASSERT_EQUALS(player.heldItemStack().amount(),67);
		for(int z=8;z<40;++z) TS_ASSERT_EQUALS(world.getBlock(0,8,z),fixture.rope);
		TS_ASSERT(!useClimbingRope(world,player,{0,8,39},5));
		TS_ASSERT_EQUALS(world.getBlock(0,8,7),0);
		world.setBlock(0,8,40,0);
		for(int tick=0;tick<80;++tick) world.blockPhysics().tick(256);
		for(int z=8;z<40;++z) TS_ASSERT_EQUALS(world.getBlock(0,8,z),0);
		unsigned returned=0;
		world.scene().registry().view<ItemStack>().each([&](auto,const auto &stack) {
			if(stack.stringID()=="default:climbing_rope") returned+=stack.amount();
		});
		TS_ASSERT_EQUALS(returned,32);
		player.setCreative(true); world.setBlock(10,10,40,fixture.stone);
		TS_ASSERT(useClimbingRope(world,player,{10,10,40},5));
		TS_ASSERT_EQUALS(player.heldItemStack().amount(),67);
	}
	void testRopeStopsAtObstaclesAndUnloadedTerrainWithoutSpendingExtras() {
		RopeWorld fixture; auto &world=*fixture.world;
		Player player; player.inventory()=Inventory(9,4,"main"); player.setHeldItemSlot(0);
		player.inventory().setStack(0,0,"default:climbing_rope",20);
		world.setBlock(8,8,40,fixture.stone); world.setBlock(8,8,35,fixture.stone);
		TS_ASSERT(useClimbingRope(world,player,{8,8,40},5));
		TS_ASSERT_EQUALS(player.heldItemStack().amount(),16);
		TS_ASSERT(!useClimbingRope(world,player,{8,8,39},5));
		TS_ASSERT_EQUALS(world.getBlock(8,8,35),fixture.stone);
		world.getOrCreateChunk(0,0,0).setInitialized(false);
		world.setBlock(5,5,33,fixture.stone);
		TS_ASSERT(useClimbingRope(world,player,{5,5,33},5));
		TS_ASSERT_EQUALS(player.heldItemStack().amount(),15);
		TS_ASSERT_EQUALS(world.getBlock(5,5,31),0);
	}
	void testRopeClimbsWithoutAWallAndSneakHoldsPosition() {
		RopeWorld fixture; auto &world=*fixture.world;
		world.setBlock(8,8,20,fixture.rope);
		auto environment=retro::movementEnvironment(world,{8.2,8.2,20,0.6,0.6,1.8});
		TS_ASSERT(environment.rope); TS_ASSERT(environment.ladder);
		Vector3d velocity{0,0,-1};
		retro::acceleratePlayer(velocity,0,0,0,true,false,false,environment);
		TS_ASSERT_DELTA(velocity.z,0.2,0.0001);
		velocity.z=-1;
		retro::acceleratePlayer(velocity,0,0,0,false,true,false,environment);
		TS_ASSERT_EQUALS(velocity.z,0);
	}
	void testDescendingRopeAndSteppingOffDoesNotAccumulateFallDamage() {
		RopeWorld fixture; auto &world=*fixture.world;
		auto &player=fixture.players.addPlayer("tester",true); ClientInfo client{1,{}};
		player.setClient(&client); player.setReady(true); player.setPosition(8.5,8.5,20);
		for(int x=0;x<16;++x) for(int y=0;y<16;++y) world.setBlock(x,y,0,fixture.stone);
		for(int z=1;z<=20;++z) world.setBlock(8,8,z,fixture.rope);
		player.survival().fallDistance=10;
		for(int z=19;z>=1;--z) TS_ASSERT(world.moveSurvivalPlayer(player,8.5,8.5,z));
		TS_ASSERT(world.moveSurvivalPlayer(player,9.5,8.5,1));
		TS_ASSERT_EQUALS(player.health(),20); TS_ASSERT_EQUALS(player.survival().fallDistance,0);
	}
	void testLegacyRuinNotesAndHighBuriedCacheCoordinatesStayReadable() {
		struct Restore {std::filesystem::path original=std::filesystem::current_path(); ~Restore(){std::filesystem::current_path(original);}} restore;
		auto path=std::filesystem::temp_directory_path()/("opencraft-cache-notes-"+std::to_string(getpid()));
		std::filesystem::create_directories(path); std::filesystem::current_path(path);
		u16 id=exploration::saveNote("test",{-20,90,100},true);
		TS_ASSERT(id); TS_ASSERT(exploration::noteIsCache("test",id));
		auto target=exploration::loadNote("test",id); TS_ASSERT(target);
		if(target) TS_ASSERT(*target==Vector3i(-20,90,100));
		std::ofstream legacy(exploration::notePath("test",u16(id+1)).string()); legacy<<"1 2 20\n"; legacy.close();
		TS_ASSERT(exploration::loadNote("test",u16(id+1)));
		TS_ASSERT(!exploration::noteIsCache("test",u16(id+1)));
	}
	void testCachesHaveSuppliesRareDiscsWornToolsAndNeverRefill() {
		struct Restore {std::filesystem::path original=std::filesystem::current_path(); ~Restore(){std::filesystem::current_path(original);}} restore;
		std::filesystem::current_path(OM_TEST_SOURCE_DIR);
		ScriptEngine scripts; Registry registry; Registry::setInstance(registry);
		registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
		registry.registerItem<Item>({},BLOCK_AIR,"air").setIsBlock(true);
		WorldController worlds(registry); ServerModLoader loader(scripts,worlds); loader.loadMods();
		TS_ASSERT(registry.getItemFromStringID("default:climbing_rope").canBeActivated());
		Inventory grid(3,3); for(u16 y=0;y<3;++y) grid.setStack(0,y,"default:string",1);
		const Recipe *recipe=registry.getRecipe(grid); TS_ASSERT(recipe);
		if(recipe) TS_ASSERT_EQUALS(recipe->result().amount(),8);
		PlayerList players; Dimension dimension{0,"test:cache","cache"};
		unsigned caches=0,discs=0,worn=0;
		for(int seed=0;seed<1000;++seed) {
			auto candidate=exploration::cache(seed,0,0); if(!candidate) continue;
			ServerWorld world(players,dimension,seed);
			auto &column=world.heightmap().getOrCreateChunk(candidate->x/16,candidate->y/16);
			column.blocks().fill(0);
			for(int x=0;x<16;++x) for(int y=0;y<16;++y) for(int z=0;z<=64;++z) column.blocks()[(x*16+y)*128+z]=1;
			column.refreshHeight();
			auto &chunk=world.getOrCreateChunk(candidate->x/16,candidate->y/16,1);
			for(int z=0;z<32;++z) for(int y=0;y<16;++y) for(int x=0;x<16;++x) chunk.setBlockRaw(x,y,z,registry.getBlockFromStringID("default:stone").id());
			decorateCacheTerrain(chunk,world.heightmap(),seed);
			auto *data=world.getBlockData(candidate->x,candidate->y,60);
			TS_ASSERT(data); if(!data) continue;
			++caches;
			TS_ASSERT(data->meta.hasInt("treasure_cache"));
			TS_ASSERT_EQUALS(data->inventory.getStack(0,0).stringID(),"default:coal");
			for(const auto &stack:data->inventory.items()) if(stack.amount()) {
				TS_ASSERT_DIFFERS(stack.stringID(),"default:apple"); TS_ASSERT_DIFFERS(stack.stringID(),"default:apple_golden");
				if(stack.item().hasGroup("group:om_record")) ++discs;
				if(stack.maxDamage()) { ++worn; TS_ASSERT(stack.damage()>=stack.maxDamage()/2); TS_ASSERT(stack.damage()<stack.maxDamage()); }
			}
			data->inventory.setStack(0,0,"default:coal",1);
			decorateCacheTerrain(chunk,world.heightmap(),seed);
			TS_ASSERT_EQUALS(data->inventory.getStack(0,0).amount(),1);
		}
		TS_ASSERT(caches>200); TS_ASSERT(discs>0 && discs<caches/10); TS_ASSERT(worn>20);
	}
};
#endif
