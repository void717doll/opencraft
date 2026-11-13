#ifndef BLOCKBREAKINGSYNCTESTS_HPP_
#define BLOCKBREAKINGSYNCTESTS_HPP_

#include <cxxtest/TestSuite.h>
#include "common/core/GameClock.hpp"
#include "common/core/Registry.hpp"
#include "common/world/BetaPlayerMovement.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/WorldController.hpp"

class BlockBreakingSyncTests : public CxxTest::TestSuite {
public:
	void setUp() override {
		static LoggerHandler logger; LoggerHandler::setInstance(logger);
		static GameClock clock; GameClock::setInstance(clock);
	}
	void testGroundedMovementKeepsTheSameStepPathAsTheClient() {
		Registry definitions; Registry::setInstance(definitions);
		auto &air = definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0); air.isCollidable(false); air.isOpaque(false);
		u16 stone = definitions.registerBlock<ServerBlock>("test:stone").id();
		auto &bar = definitions.registerBlock<ServerBlock>("test:bar");
		bar.getState(0).boundingBox({0.45f,0,0,0.1f,1,0.5f});
		PlayerList players; auto &player = players.addPlayer("tester", false); ClientInfo client{1,{}};
		player.setClient(&client); player.setReady(true); player.setPosition(8.1,8.5,1);
		Dimension dimension{0,"test:world","world"}; ServerWorld world(players,dimension,42);
		auto &chunk = world.getOrCreateChunk(0,0,0);
		for (int y=0; y<CHUNK_DEPTH; ++y) for (int x=0; x<CHUNK_WIDTH; ++x) chunk.setBlockRaw(x,y,0,stone);
		chunk.setBlockRaw(8,8,1,bar.id()); chunk.setInitialized(true);
		DoubleBox body=player.hitbox(); body.move(player.x(),player.y(),player.z());
		Vector3d requested{0.8,0,0};
		Vector3d stepped=retro::movePlayer(world,body,requested,true,false);
		TS_ASSERT_DELTA(stepped.x,requested.x,0.000001); TS_ASSERT_DELTA(stepped.z,0,0.000001);
		TS_ASSERT(world.moveSurvivalPlayer(player,player.x()+stepped.x,player.y(),player.z()+stepped.z));
		chunk.setBlockRaw(10,8,1,stone); chunk.setBlockRaw(10,8,2,stone);
		TS_ASSERT(!world.moveSurvivalPlayer(player,10.5,8.5,1));
	}
	void testRejectedDigAcknowledgesTheAuthoritativeBlock() {
		ScriptEngine scripts; Registry definitions; Registry::setInstance(definitions);
		auto &air = definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0); air.isCollidable(false); air.isOpaque(false);
		definitions.registerItem<Item>({},BLOCK_AIR,"air");
		definitions.registerItem<Item>({},"test:ore","ore");
		auto &stone = definitions.registerBlock<ServerBlock>("test:stone");
		definitions.registerDimension("test:world","world");
		PlayerList players; auto &saved = players.addPlayer("tester",false); saved.setPosition(8.5,8.5,1);
		WorldController worlds(definitions); Server server;
		ServerCommandHandler commands(scripts,server,worlds,players,definitions);
		worlds.setServer(commands); worlds.init(players,42); worlds.setCreative(true);
		auto &world = worlds.getWorld(0); auto &chunk = world.getOrCreateChunk(0,0,0);
		chunk.setBlockRaw(14,8,1,stone.id()); chunk.setBlockRaw(9,8,1,stone.id()); chunk.setInitialized(true);
		auto *stored = world.addBlockData(14,8,1,2,1); stored->meta.setInt("marker",17);
		stored->inventory.setStack(0,0,"test:ore",3);
		commands.setupCallbacks(); server.init(); server.setRunning(true);
		auto session = Network::LocalSession::connect();
		Network::Packet packet; packet << Network::Command::ClientConnect << std::string("tester");
		session->sendToServer(packet); server.handleGameEvents(); saved.setReady(true);
		while (session->receiveOnClient(packet)) {}
		packet.clear(); packet << Network::Command::PlayerDigBlock << s32(14) << s32(8) << s32(1);
		session->sendToServer(packet); server.handleGameEvents();
		TS_ASSERT(session->receiveOnClient(packet));
		Network::Command command; s32 x,y,z; u32 block;
		packet >> command >> x >> y >> z >> block;
		TS_ASSERT_EQUALS(command,Network::Command::BlockUpdate); TS_ASSERT_EQUALS(x,14);
		TS_ASSERT_EQUALS(u16(block),stone.id()); TS_ASSERT_EQUALS(world.getBlock(14,8,1),stone.id());
		TS_ASSERT(session->receiveOnClient(packet));
		BlockMetadata metadata; packet >> command >> x >> y >> z >> metadata;
		TS_ASSERT_EQUALS(command,Network::Command::BlockDataUpdate); TS_ASSERT_EQUALS(metadata.get<int>("marker"),17);
		TS_ASSERT(session->receiveOnClient(packet));
		Inventory inventory; packet >> command >> x >> y >> z >> inventory;
		TS_ASSERT_EQUALS(command,Network::Command::BlockInvUpdate); TS_ASSERT_EQUALS(inventory.getStack(0,0).amount(),3);
		TS_ASSERT(saved.statistics().items.empty());
		packet.clear(); packet << Network::Command::PlayerDigBlock << s32(9) << s32(8) << s32(1);
		session->sendToServer(packet); server.handleGameEvents();
		TS_ASSERT(session->receiveOnClient(packet)); packet >> command >> x >> y >> z >> block;
		TS_ASSERT_EQUALS(command,Network::Command::BlockUpdate); TS_ASSERT_EQUALS(block,0u);
		TS_ASSERT_EQUALS(world.getBlock(9,8,1),0); TS_ASSERT_EQUALS(saved.statistics().items[stone.stringID()].mined,1u);
	}
};
#endif
