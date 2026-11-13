#ifndef PISTONWORKTESTS_HPP_
#define PISTONWORKTESTS_HPP_

#include <cxxtest/TestSuite.h>
#include "common/core/Registry.hpp"
#include "common/core/GameClock.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerWorld.hpp"

class PistonWorkTests : public CxxTest::TestSuite {
	struct CountWorld : ServerWorld {
		using ServerWorld::ServerWorld;
		mutable unsigned lookups=0;
		Chunk *getChunk(int x,int y,int z) const override { ++lookups; return ServerWorld::getChunk(x,y,z); }
	};
	struct Fixture {
		Registry registry;
		PlayerList players;
		Dimension dimension{0,"test:world","world"};
		std::unique_ptr<CountWorld> world;
		u16 piston,head,lever,bedrock;
		Fixture() {
			Registry::setInstance(registry);
			auto &air=registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0); air.isCollidable(false); air.isOpaque(false);
			auto &base=registry.registerBlock<ServerBlock>("default:piston"); piston=base.id();
			base.addGroup("group:om_piston",1); base.addGroup("group:om_redstone_device",1);
			base.addState(); base.param().allocateBits(BlockParam::State,1);
			auto &top=registry.registerBlock<ServerBlock>("default:piston_head"); head=top.id();
			top.addGroup("group:om_piston_head",1); top.addGroup("group:om_redstone_device",1);
			top.setRotatable(true); top.param().allocateBits(BlockParam::Rotation,5); top.param().allocateBits(BlockParam::State,1);
			auto &power=registry.registerBlock<ServerBlock>("default:lever"); lever=power.id();
			power.addGroup("group:om_power_source",2); power.getState(0).isOpaque(false); power.getState(0).isCollidable(false);
			power.addState(); power.param().allocateBits(BlockParam::State,1);
			bedrock=registry.registerBlock<ServerBlock>("default:bedrock").id();
			world=std::make_unique<CountWorld>(players,dimension,42);
			auto &chunk=world->getOrCreateChunk(0,0,0); chunk.setBlockRaw(8,8,8,piston); chunk.setBlockRaw(7,8,8,lever); chunk.setInitialized(true);
		}
		void settle() {
			for(unsigned i=0;i<5;++i) { world->updateRedstoneCircuits(); world->updateRailDevices(); }
		}
	};
public:
	void setUp() override {
		static LoggerHandler logger; LoggerHandler::setInstance(logger);
		static GameClock clock; GameClock::setInstance(clock);
	}
	void testSettledPistonSleepsAndWakesOnPowerChanges() {
		Fixture f; f.world->queueRedstone({8,8,8}); f.settle();
		f.world->lookups=0; for(int i=0;i<20;++i) f.world->updateRailDevices(); TS_ASSERT_EQUALS(f.world->lookups,0u);
		f.world->setBlockState(7,8,8,1); f.settle();
		TS_ASSERT_EQUALS(f.world->getBlockState(8,8,8)->id(),1); TS_ASSERT_EQUALS(f.world->getBlock(8,8,9),f.head);
		f.world->lookups=0; for(int i=0;i<20;++i) f.world->updateRailDevices(); TS_ASSERT_EQUALS(f.world->lookups,0u);
		f.world->setBlockState(7,8,8,0); f.settle();
		TS_ASSERT_EQUALS(f.world->getBlockState(8,8,8)->id(),0); TS_ASSERT_EQUALS(f.world->getBlock(8,8,9),0);
	}
	void testSleepingPistonWakesWhenQuasiPowerChangesThreeBlocksAway() {
		Fixture f; f.world->setBlock(9,8,9,f.bedrock); f.world->setBlock(9,8,10,f.lever); f.settle();
		f.world->setBlockState(9,8,10,1); f.settle();
		TS_ASSERT_EQUALS(f.world->getBlockState(8,8,8)->id(),1);
		f.world->setBlockState(9,8,10,0); f.settle();
		TS_ASSERT_EQUALS(f.world->getBlockState(8,8,8)->id(),0);
	}
	void testBlockedPistonRetriesUntilObstructionIsGone() {
		Fixture f; f.world->setBlock(8,8,9,f.bedrock); f.world->setBlockState(7,8,8,1); f.settle();
		TS_ASSERT_EQUALS(f.world->getBlockState(8,8,8)->id(),0);
		f.world->getOrCreateChunk(0,0,0).setBlockRaw(8,8,9,0);
		f.world->updateRailDevices();
		TS_ASSERT_EQUALS(f.world->getBlockState(8,8,8)->id(),1); TS_ASSERT_EQUALS(f.world->getBlock(8,8,9),f.head);
	}
};
#endif
