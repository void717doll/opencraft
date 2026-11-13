#ifndef MINECARTTESTS_HPP_
#define MINECARTTESTS_HPP_

#include <cxxtest/TestSuite.h>
#include <filesystem>
#include <unistd.h>
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/NetworkComponent.hpp"
#include "server/world/save/WorldSaveBasicBackend.hpp"
#include "common/core/Registry.hpp"
#include "common/core/GameClock.hpp"
#include "common/scene/RetroMobMesh.hpp"
#include "common/scene/component/MinecartComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/scene/component/RotationComponent.hpp"
#include "common/world/BetaRails.hpp"
#include "common/world/BlockSupport.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerWorld.hpp"

class MinecartTests : public CxxTest::TestSuite {
	struct Fixture {
		Registry definitions;
		PlayerList players;
		Dimension dimension{0,"test:carts","carts"};
		std::unique_ptr<ServerWorld> world;
		u16 stone, rail, poweredRail, detectorRail, lever, torch, plate;
		Fixture() {
			Registry::setInstance(definitions);
			definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
			definitions.registerItem<Item>({},BLOCK_AIR,"air");
			stone=definitions.registerBlock<ServerBlock>("test:stone").id();
			auto &block=definitions.registerBlock<ServerBlock>("default:rail"); rail=block.id();
			block.addGroup("group:om_rail",1); block.addGroup("group:attached_node",3);
			block.getState(0).isCollidable(false); block.getState(0).isOpaque(false);
			block.getState(0).drawType(BlockDrawType::Rail);
			for (int i=1;i<10;++i) block.addState();
			block.param().allocateBits(BlockParam::State,4);
			definitions.registerItem<Item>({},"default:rail","rail");
			for (auto entry : {std::make_pair("default:powered_rail",2),std::make_pair("default:detector_rail",3)}) {
				auto &special=definitions.registerBlock<ServerBlock>(entry.first);
				if(entry.second==2) poweredRail=special.id(); else detectorRail=special.id();
				special.addGroup("group:om_rail",1); special.addGroup("group:om_rail_type",entry.second);
				special.getState(0).isCollidable(false); special.getState(0).isOpaque(false);
				for (int i=1;i<16;++i) special.addState();
				special.param().allocateBits(BlockParam::State,4);
			}
			for (auto entry : {std::make_pair("default:redstone_torch",1),std::make_pair("default:lever",2),std::make_pair("default:stone_pressure_plate",3)}) {
				auto &source=definitions.registerBlock<ServerBlock>(entry.first);
				if(entry.second==1) torch=source.id(); else if(entry.second==2) lever=source.id(); else plate=source.id();
				source.addGroup("group:om_power_source",entry.second);
				source.getState(0).isCollidable(false); source.getState(0).isOpaque(false);
				source.addState(); source.param().allocateBits(BlockParam::State,1);
			}

			auto &cart=definitions.registerItem<Item>({},"default:minecart","cart");
			cart.addGroup("group:om_minecart",1); cart.setMaxStackSize(1);
			world=std::make_unique<ServerWorld>(players,dimension,42);
			auto &chunk=world->getOrCreateChunk(0,0,0);
			for (int y=0;y<CHUNK_DEPTH;++y) for(int x=0;x<CHUNK_WIDTH;++x) chunk.setBlockRaw(x,y,3,stone);
			chunk.setInitialized(true);
		}
		void track(int x,int y,int z,unsigned shape) {
			world->setBlock(x,y,z,rail); world->setBlockState(x,y,z,u16(shape));
		}
		entt::entity cart(double x,double y,double z,Vector3d velocity={0,0,0}) {
			auto &registry=world->scene().registry(); auto entity=registry.create();
			registry.emplace<MinecartComponent>(entity).velocity=velocity;
			registry.emplace<PositionComponent>(entity,x,y,z,0);
			registry.emplace<RotationComponent>(entity);
			return entity;
		}
	};
public:
	void setUp() override {
		static LoggerHandler logger; LoggerHandler::setInstance(logger);
		static GameClock clock; GameClock::setInstance(clock);
	}
	void testRailCurvesSlopesAndProjectionJoinAtBoundaries() {
		TS_ASSERT_EQUALS(retro::railShape({{-2,-2,0,0}}),1u);
		TS_ASSERT_EQUALS(retro::railShape({{0,0,-2,-2}}),0u);
		TS_ASSERT_EQUALS(retro::railShape({{-2,0,-2,0}}),6u);
		TS_ASSERT_EQUALS(retro::railShape({{-2,0,0,-2}}),7u);
		TS_ASSERT_EQUALS(retro::railShape({{0,-2,0,-2}}),8u);
		TS_ASSERT_EQUALS(retro::railShape({{0,-2,-2,0}}),9u);
		TS_ASSERT_EQUALS(retro::railShape({{-2,-2,0,1}}),2u);
		TS_ASSERT_EQUALS(retro::railShape({{-2,-2,1,0}}),3u);
		TS_ASSERT_EQUALS(retro::railShape({{1,0,-2,-2}}),4u);
		TS_ASSERT_EQUALS(retro::railShape({{0,1,-2,-2}}),5u);
		TS_ASSERT_EQUALS(retro::railPoint(2,{1,0.5,0}),Vector3d(1,0.5,1));
		TS_ASSERT_EQUALS(retro::railPoint(6,{0.5,1,0}),Vector3d(0.5,1,0));
		TS_ASSERT_EQUALS(retro::railPoint(6,{1,0.5,0}),Vector3d(1,0.5,0));
	}
	void testCartSlopeGravityAndSpeedLimit() {
		Vector3d uphill{0.2,0,0}; retro::alignCartVelocity(uphill,2);
		TS_ASSERT_DELTA(uphill.x,0.2-1.0/128,0.000001);
		Vector3d stopped{0,0,0}; retro::alignCartVelocity(stopped,2);
		TS_ASSERT(stopped.x<0);
		Vector3d fast{8,0,0}; retro::alignCartVelocity(fast,1);
		TS_ASSERT_DELTA(fast.x,2.0,0.000001);
	}
	void testCartSavePreservesMotionButNeverRiderAttachment() {
		MinecartComponent original; original.velocity={0.12,-0.2,0.03}; original.damage=12; original.rider="tester";
		sf::Packet packet; original.serialize(packet);
		MinecartComponent restored; restored.deserialize(packet);
		TS_ASSERT(packet); TS_ASSERT_EQUALS(restored.velocity,original.velocity);
		TS_ASSERT_EQUALS(restored.damage,12); TS_ASSERT(restored.rider.empty());
	}
	void testSavedWorldRestoresCartComponentsAndClearsOccupancy() {
		try {
			namespace fs=std::filesystem;
			struct RestoreDirectory { fs::path previous=fs::current_path(); ~RestoreDirectory() { fs::current_path(previous); } } restore;
			fs::path directory=fs::temp_directory_path()/("oldworld-cart-save-"+std::to_string(getpid()));
			fs::create_directories(directory); fs::current_path(directory);
			Fixture f; std::deque<ServerWorld> worlds; worlds.emplace_back(f.players,f.dimension,42);
			auto &registry=worlds.front().scene().registry(); auto entity=registry.create();
			auto &cart=registry.emplace<MinecartComponent>(entity); cart.velocity={0.1,0.2,0}; cart.rider="tester";
			registry.emplace<PositionComponent>(entity,3.5,4.5,5.0625,0);
			registry.emplace<RotationComponent>(entity); registry.emplace<NetworkComponent>(entity,entity);
			registry.emplace<DrawableDef>(entity).setMobModel(32);
			registry.emplace<DoubleBox>(entity,-0.49,-0.49,0,0.98,0.98,0.7);
			WorldSaveBasicBackend backend(worlds,f.players); backend.save("railway"); registry.clear(); backend.load("railway");
			TS_ASSERT_EQUALS(registry.view<MinecartComponent>().size(),1u);
			registry.view<MinecartComponent,PositionComponent,DrawableDef,DoubleBox>().each([&](auto &restored,auto &position,auto &drawable,auto &box) {
				TS_ASSERT(restored.rider.empty()); TS_ASSERT_DELTA(restored.velocity.x,0.1,0.000001);
				TS_ASSERT_DELTA(position.z,5.0625,0.000001); TS_ASSERT_EQUALS(drawable.mobModel(),32);
				TS_ASSERT_DELTA(box.sizeX,0.98,0.000001);
			});
		} catch (const Exception &error) { TS_FAIL(error.what()); } catch (const std::exception &error) { TS_FAIL(error.what()); }
	}
	void testCartModelHasSixBetaPiecesAndNoSecondLayer() {
		auto mesh=retro::mobMesh(32); TS_ASSERT_EQUALS(mesh.size(),216u);
		TS_ASSERT(retro::mobMesh(32,0,0,0,0,1).empty());
		for (const auto &vertex:mesh) {
			TS_ASSERT(vertex.z>=-0.001 && vertex.z<=0.7);
			TS_ASSERT(std::isfinite(vertex.u) && std::isfinite(vertex.v));
		}
	}
	void testRailConnectionsUpdateAndUnsupportedRailDrops() {
		Fixture f; f.track(5,5,4,0); f.track(6,5,4,0);
		f.world->refreshRail({5,5,4}); TS_ASSERT_EQUALS(f.world->getBlockState(5,5,4)->id(),1);
		f.world->setBlock(7,5,4,f.stone); f.track(7,5,5,1);
		f.world->refreshRail({6,5,4}); TS_ASSERT_EQUALS(f.world->getBlockState(6,5,4)->id(),2);
		f.world->setBlock(5,5,3,0); f.world->blockPhysics().tick(256);
		TS_ASSERT_EQUALS(f.world->getBlock(5,5,4),0);
	}
	void testOrdinaryCartMovesAndSlowsOnFlatRails() {
		Fixture f; for(int x=2;x<14;++x) f.track(x,8,4,1);
		auto entity=f.cart(3.5,8.5,4.0625,{0.3,0,0});
		for(int i=0;i<10;++i) f.world->updateMinecarts();
		auto &position=f.world->scene().registry().get<PositionComponent>(entity);
		auto &cart=f.world->scene().registry().get<MinecartComponent>(entity);
		TS_ASSERT(position.x>5.8 && position.x<6.1); TS_ASSERT_DELTA(position.y,8.5,0.00001);
		TS_ASSERT_DELTA(position.z,4.0625,0.00001); TS_ASSERT(cart.velocity.x<0.21);
	}
	void testStationaryCartDoesNotResendItsPosition() {
		Fixture f; for (int x=2; x<14; ++x) f.track(x,8,4,1);
		auto entity=f.cart(8.5,8.5,4.0625);
		auto &position=f.world->scene().registry().get<PositionComponent>(entity);
		position.isUpdated=false;
		f.world->updateMinecarts();
		TS_ASSERT(!position.isUpdated);
		f.world->scene().registry().get<MinecartComponent>(entity).velocity={0.1,0,0};
		f.world->updateMinecarts();
		TS_ASSERT(position.isUpdated);
	}
	void testCartFollowsCurveIntoTheConnectedTrack() {
		Fixture f; for(int x=2;x<7;++x) f.track(x,8,4,1);
		f.track(7,8,4,7); for(int y=9;y<15;++y) f.track(7,y,4,0);
		auto entity=f.cart(6.8,8.5,4.0625,{0.3,0,0});
		for(int i=0;i<10;++i) f.world->updateMinecarts();
		const auto &position=f.world->scene().registry().get<PositionComponent>(entity);
		TS_ASSERT_DELTA(position.x,7.5,0.00001); TS_ASSERT(position.y>9.3);
	}
	void testCartStopsAtSolidWallAndUnloadedBoundary() {
		Fixture f; for(int x=2;x<16;++x) f.track(x,8,4,1);
		f.world->setBlock(8,8,4,f.stone);
		auto entity=f.cart(7,8.5,4.0625,{0.4,0,0});
		for(int i=0;i<5;++i) f.world->updateMinecarts();
		auto &position=f.world->scene().registry().get<PositionComponent>(entity);
		TS_ASSERT(position.x<7.52);
		position.x=15.3; f.world->scene().registry().get<MinecartComponent>(entity).velocity={0.4,0,0};
		f.world->updateMinecarts(); TS_ASSERT(position.x<15.52);
	}
	void testSlopeCarriesCartUpAndDownWithoutHittingItsSupport() {
		Fixture f; f.track(5,8,4,1); f.track(6,8,4,2);
		for(int x=7;x<13;++x) { f.world->setBlock(x,8,4,f.stone); f.track(x,8,5,1); }
		auto entity=f.cart(5.75,8.5,4.0625,{0.4,0,0});
		for(int i=0;i<8;++i) f.world->updateMinecarts();
		auto &position=f.world->scene().registry().get<PositionComponent>(entity);
		TS_ASSERT(position.x>7); TS_ASSERT_DELTA(position.z,5.0625,0.00001);
		position.x=7.3; f.world->scene().registry().get<MinecartComponent>(entity).velocity={-0.3,0,0};
		for(int i=0;i<8;++i) f.world->updateMinecarts();
		TS_ASSERT(position.x<6); TS_ASSERT_DELTA(position.z,4.0625,0.00001);
	}
	void testPlacementConsumesOneCartAndRejectsDuplicates() {
		Fixture f; f.track(6,6,4,1);
		auto &player=f.players.addPlayer("tester",false); player.inventory().resize(9,4);
		player.setHeldItemSlot(0); player.inventory().setStack(0,0,"default:minecart",1);
		TS_ASSERT(f.world->placeMinecart(player,{6,6,4})); TS_ASSERT_EQUALS(player.heldItemStack().amount(),0);
		player.inventory().setStack(0,0,"default:minecart",1);
		TS_ASSERT(!f.world->placeMinecart(player,{6,6,4})); TS_ASSERT_EQUALS(player.heldItemStack().amount(),1);
		TS_ASSERT(!f.world->placeMinecart(player,{6,7,4}));
	}
	void testPoweredRailSignalExtendsEightConnectedRailsAndClears() {
		Fixture f;
		for(int x=1;x<15;++x) { f.world->setBlock(x,8,4,f.poweredRail); f.world->setBlockState(x,8,4,1); }
		f.world->setBlock(1,7,4,f.lever); f.world->setBlockState(1,7,4,1);
		for(int x=1;x<15;++x) f.world->refreshPoweredRail({x,8,4});
		TS_ASSERT_EQUALS(f.world->getBlockState(8,8,4)->id(),9);
		TS_ASSERT_EQUALS(f.world->getBlockState(9,8,4)->id(),1);
		f.world->setBlockState(1,7,4,0);
		for(int x=1;x<15;++x) f.world->refreshPoweredRail({x,8,4});
		TS_ASSERT_EQUALS(f.world->getBlockState(1,8,4)->id(),1);
		TS_ASSERT_EQUALS(f.world->getBlockState(8,8,4)->id(),1);
	}
	void testPowerCannotCrossOrdinaryRailOrDisconnectedAxis() {
		Fixture f;
		for(int x=2;x<7;++x) { f.world->setBlock(x,8,4,f.poweredRail); f.world->setBlockState(x,8,4,1); }
		f.world->setBlock(2,7,4,f.lever); f.world->setBlockState(2,7,4,1);
		f.track(4,8,4,1); f.world->refreshPoweredRail({5,8,4});
		TS_ASSERT_EQUALS(f.world->getBlockState(5,8,4)->id(),1);
		f.world->setBlock(4,8,4,f.poweredRail); f.world->setBlockState(4,8,4,0);
		f.world->refreshPoweredRail({5,8,4}); TS_ASSERT_EQUALS(f.world->getBlockState(5,8,4)->id(),1);
	}
	void testDetectorActivatesOnCartAndTurnsOffAfterItLeaves() {
		Fixture f; f.world->setBlock(5,8,4,f.detectorRail); f.world->setBlockState(5,8,4,1);
		f.world->setBlock(6,8,4,f.poweredRail); f.world->setBlockState(6,8,4,1);
		auto entity=f.cart(5.5,8.5,4.0625);
		f.world->watchRailDevice({5,8,4}); f.world->updateRailDevices();
		TS_ASSERT_EQUALS(f.world->getBlockState(5,8,4)->id(),9);
		f.world->refreshPoweredRail({6,8,4}); TS_ASSERT_EQUALS(f.world->getBlockState(6,8,4)->id(),9);
		f.world->scene().registry().get<PositionComponent>(entity).y=10;
		f.world->updateRailDevices(); TS_ASSERT_EQUALS(f.world->getBlockState(5,8,4)->id(),1);
		f.world->refreshPoweredRail({6,8,4}); TS_ASSERT_EQUALS(f.world->getBlockState(6,8,4)->id(),1);
	}
	void testRedstoneTorchInvertsPoweredSupportAndRestores() {
		Fixture f; f.world->setBlock(5,5,4,f.stone); f.world->setBlock(5,5,5,f.torch);
		f.world->watchRailDevice({5,5,5}); f.world->updateRailDevices();
		TS_ASSERT_EQUALS(f.world->getBlockState(5,5,5)->id(),0);
		f.world->setBlock(5,5,2,f.stone); f.world->setBlock(5,5,3,f.torch);
		f.world->updateRailDevices();
		f.world->updateRedstoneCircuits();
		TS_ASSERT_EQUALS(f.world->getBlockState(5,5,5)->id(),0);
		f.world->updateRedstoneCircuits();
		TS_ASSERT_EQUALS(f.world->getBlockState(5,5,5)->id(),1);
		f.world->setBlock(5,5,3,0); f.world->updateRailDevices();
		f.world->updateRedstoneCircuits();
		TS_ASSERT_EQUALS(f.world->getBlockState(5,5,5)->id(),1);
		f.world->updateRedstoneCircuits();
		TS_ASSERT_EQUALS(f.world->getBlockState(5,5,5)->id(),0);
	}
	void testStonePlateSensesPlayerButNotAnEmptyCart() {
		Fixture f; f.world->setBlock(5,5,4,f.plate); f.world->watchRailDevice({5,5,4});
		f.cart(5.5,5.5,4.0625); f.world->updateRailDevices();
		TS_ASSERT_EQUALS(f.world->getBlockState(5,5,4)->id(),0);
		ClientInfo client{1,{}}; auto &player=f.players.addPlayer("tester",false);
		player.setClient(&client); player.setReady(true); player.setPosition(5.5,5.5,4);
		f.world->updateRailDevices(); TS_ASSERT_EQUALS(f.world->getBlockState(5,5,4)->id(),1);
		player.setPosition(8,8,4); f.world->updateRailDevices(); TS_ASSERT_EQUALS(f.world->getBlockState(5,5,4)->id(),0);
	}
	void testPoweredAccelerationBrakeAndRestStartMatchModernMcp() {
		Fixture f; for(int x=1;x<15;++x) { f.world->setBlock(x,8,4,f.poweredRail); f.world->setBlockState(x,8,4,9); }
		auto entity=f.cart(4.5,8.5,4.0625,{0.1,0,0});
		f.world->updateMinecarts(); auto &cart=f.world->scene().registry().get<MinecartComponent>(entity);
		TS_ASSERT_DELTA(cart.velocity.x,0.156,0.000001);
		for(int x=1;x<15;++x) f.world->setBlockState(x,8,4,1);
		f.world->updateMinecarts(); TS_ASSERT_DELTA(cart.velocity.x,0.07488,0.000001);
		cart.velocity={0.005,0,0}; f.world->updateMinecarts(); TS_ASSERT_DELTA(cart.velocity.x,0,0.000001);
		f.world->setBlock(3,8,4,f.stone); f.world->setBlockState(4,8,4,9);
		auto &position=f.world->scene().registry().get<PositionComponent>(entity); position.x=4.5;
		f.world->updateMinecarts(); TS_ASSERT_DELTA(cart.velocity.x,0.02,0.000001);
		f.world->updateMinecarts(); TS_ASSERT(position.x>4.5);
	}
	void testPoweredRailLimitsMovementWithoutDiscardingMomentum() {
		Fixture f;
		for (int x=1; x<15; ++x) { f.world->setBlock(x,8,4,f.poweredRail); f.world->setBlockState(x,8,4,9); }
		auto entity=f.cart(4.5,8.5,4.0625,{1.2,0,0});
		f.world->updateMinecarts();
		const auto &position=f.world->scene().registry().get<PositionComponent>(entity);
		const auto &cart=f.world->scene().registry().get<MinecartComponent>(entity);
		TS_ASSERT_DELTA(position.x,4.9,0.000001);
		TS_ASSERT_DELTA(cart.velocity.x,1.212,0.000001);
	}
	void testJunctionSwitchesBranchWhenPowered() {
		Fixture f; f.track(8,8,4,0);
		f.track(8,7,4,0); f.track(8,9,4,0); f.track(9,8,4,1);
		f.world->refreshRail({8,8,4});
		TS_ASSERT_EQUALS(f.world->getBlockState(8,8,4)->id(),6);
		f.world->setBlock(7,8,4,f.lever); f.world->setBlockState(7,8,4,1);
		f.world->refreshRail({8,8,4});
		TS_ASSERT_EQUALS(f.world->getBlockState(8,8,4)->id(),9);
		f.world->setBlockState(7,8,4,0); f.world->refreshRail({8,8,4});
		TS_ASSERT_EQUALS(f.world->getBlockState(8,8,4)->id(),6);
	}
	void testTrackDoesNotPullConnectedNeighborOutOfItsLine() {
		Fixture f;
		for (int y=6; y<=10; ++y) f.track(8,y,4,0);
		f.track(9,8,4,1); f.world->refreshRail({9,8,4});
		TS_ASSERT_EQUALS(f.world->getBlockState(8,8,4)->id(),0);
		TS_ASSERT_EQUALS(f.world->getBlockState(9,8,4)->id(),0);
	}
	void testRailNeedsFullTopAndSlopeNeedsItsHighSupport() {
		Fixture f;
		auto &slab=f.definitions.registerBlock<ServerBlock>("test:slab");
		slab.getState(0).boundingBox({0,0,0,1,1,0.5});
		f.world->setBlock(6,6,3,slab.id());
		const auto &rail=f.definitions.getBlock(f.rail);
		TS_ASSERT(!blockHasSupport(*f.world,{6,6,4},rail.getState(0),0));
		slab.getState(0).boundingBox({0,0,0.5,1,1,0.5}); slab.getState(0).isOpaque(false);
		TS_ASSERT(blockHasSupport(*f.world,{6,6,4},rail.getState(0),0));
		TS_ASSERT(!blockHasSupport(*f.world,{6,6,4},rail.getState(2),0));
		f.world->setBlock(7,6,4,f.stone);
		TS_ASSERT(blockHasSupport(*f.world,{6,6,4},rail.getState(2),0));
		TS_ASSERT(!blockHasSupport(*f.world,{6,6,0},rail.getState(0),0));
		f.track(6,6,4,2); f.world->setBlock(7,6,4,0);
		for (int i=0; i<4; ++i) f.world->blockPhysics().tick(256);
		TS_ASSERT_EQUALS(f.world->getBlock(6,6,4),0);
	}
	void testDismountUsesClearLoadedFloorAndRefusesSealedExit() {
		Fixture f; auto entity=f.cart(6.5,6.5,4.0625);
		auto &player=f.players.addPlayer("tester",false); player.setPosition(6.5,6.5,4.2); player.setRidingMinecart(true);
		auto &cart=f.world->scene().registry().get<MinecartComponent>(entity); cart.rider=player.name();
		TS_ASSERT(f.world->dismountMinecart(player)); TS_ASSERT(!player.isRidingMinecart()); TS_ASSERT(cart.rider.empty());
		player.setPosition(6.5,6.5,4.2); player.setRidingMinecart(true); cart.rider=player.name();
		for(int z=4;z<=7;++z) for(int y=5;y<=7;++y) for(int x=5;x<=7;++x)
			if(x!=6||y!=6) f.world->setBlock(x,y,z,f.stone);
		TS_ASSERT(!f.world->dismountMinecart(player)); TS_ASSERT(player.isRidingMinecart());
	}
};
#endif
