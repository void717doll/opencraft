#ifndef CHUNKSTREAMINGPERFORMANCETESTS_HPP_
#define CHUNKSTREAMINGPERFORMANCETESTS_HPP_

#include <cstring>
#include <filesystem>
#include <unistd.h>
#include <unordered_set>
#include <cxxtest/TestSuite.h>
#include "common/core/Registry.hpp"
#include "common/network/LocalSession.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerWorld.hpp"
#include "server/world/WorldController.hpp"
#include "server/world/save/ChunkDatabase.hpp"
#include "server/world/save/WorldSaveBasicBackend.hpp"

class ChunkStreamingPerformanceTests : public CxxTest::TestSuite {
public:
	void setUp() override {
		static LoggerHandler logger; LoggerHandler::setInstance(logger);
	}
	void testSavedSparseMetadataKeepsItsPositionsAndContents() {
		namespace fs = std::filesystem;
		struct Restore { fs::path original = fs::current_path(); ~Restore() { fs::current_path(original); } } restore;
		auto directory = fs::temp_directory_path() / ("opencraft-stream-save-" + std::to_string(getpid()));
		fs::create_directories(directory); fs::current_path(directory);
		Registry definitions; Registry::setInstance(definitions);
		definitions.registerBlock<ServerBlock>(BLOCK_AIR); definitions.registerItem<Item>({}, BLOCK_AIR, "air");
		PlayerList players; Dimension dimension{0, "test:world", "world"}; std::deque<ServerWorld> worlds;
		worlds.emplace_back(players, dimension, 42);
		auto &chunk = worlds.front().getOrCreateChunk(-2, 3, -1); chunk.setInitialized(true); chunk.setModified(true);
		chunk.addBlockData(1, 2, 3, 9, 3)->meta.setInt("marker", 17);
		chunk.addBlockData(15, 14, 31, 1, 1)->meta.setInt("marker", 29);
		WorldSaveBasicBackend backend(worlds, players); backend.save("stream");
		ChunkDatabase database("saves/stream/map.sqlite", true);
		unsigned chunks = 0;
		database.readChunks(0, [&](int x, int y, int z, sf::Packet &packet) {
			++chunks; TS_ASSERT_EQUALS(x, -2); TS_ASSERT_EQUALS(y, 3); TS_ASSERT_EQUALS(z, -1);
			u8 version; u16 palette; packet >> version >> palette;
			TS_ASSERT_EQUALS(version, 1);
			for (unsigned i = 0; i < palette; ++i) { std::string name; packet >> name; }
			for (unsigned i = 0; i < CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT; ++i) { u16 id, data; packet >> id >> data; }
			u16 count; packet >> count; TS_ASSERT_EQUALS(count, 2);
			std::unordered_set<unsigned> positions;
			u16 previous = 0;
			for (unsigned i = 0; i < count; ++i) {
				u16 position; Inventory inventory; BlockMetadata meta; packet >> position >> inventory >> meta;
				positions.insert(position);
				if (i) TS_ASSERT(position > previous);
				previous = position;
				if (position == (3 * 16 + 2) * 16 + 1) { TS_ASSERT_EQUALS(inventory.width(), 9); TS_ASSERT_EQUALS(meta.get<int>("marker"), 17); }
				else if (position == (31 * 16 + 14) * 16 + 15) { TS_ASSERT_EQUALS(inventory.width(), 1); TS_ASSERT_EQUALS(meta.get<int>("marker"), 29); }
				else TS_FAIL("unexpected saved metadata position");
			}
			TS_ASSERT_EQUALS(positions.size(), 2u); TS_ASSERT(packet.endOfPacket());
		});
		TS_ASSERT_EQUALS(chunks, 1u);
	}

	void testChunkBytesMatchTheLegacyWriterAndMetadataFollowsTheChunk() {
		Registry definitions; Registry::setInstance(definitions);
		definitions.registerBlock<ServerBlock>(BLOCK_AIR); definitions.registerItem<Item>({}, BLOCK_AIR, "air");
		u16 stone = definitions.registerBlock<ServerBlock>("test:stone").id();
		ScriptEngine scripts; PlayerList players; Dimension dimension{0, "test:world", "world"};
		ServerWorld world(players, dimension, 42); WorldController worlds(definitions); Server server;
		ServerCommandHandler commands(scripts, server, worlds, players, definitions); world.setServer(&commands);
		auto &chunk = world.getOrCreateChunk(-2, 3, -1);
		Network::Packet expected;
		expected << Network::Command::ChunkData << s32(-2) << s32(3) << s32(-1);
		for (int z = 0; z < CHUNK_HEIGHT; ++z) for (int y = 0; y < CHUNK_DEPTH; ++y) for (int x = 0; x < CHUNK_WIDTH; ++x) {
			u32 value = u32(stone) | (u32((x + y + z) * 991) << 16);
			u8 light = u8(x * y + z);
			chunk.setDataRaw(x, y, z, value); chunk.lightmap().setLightDataRaw(x, y, z, light);
			expected << value << light;
		}
		chunk.addBlockData(1, 2, 3, 9, 3); chunk.addBlockData(15, 14, 31, 1, 1);
		auto session = Network::LocalSession::create(); ClientInfo client{0, session}; chunk.sendData(client);
		Network::Packet actual; TS_ASSERT(session->receiveOnClient(actual));
		TS_ASSERT_EQUALS(actual.getDataSize(), expected.getDataSize());
		TS_ASSERT_EQUALS(std::memcmp(actual.getData(), expected.getData(), expected.getDataSize()), 0);
		std::unordered_set<Vector3i> metadata, inventories;
		while (session->receiveOnClient(actual)) {
			Network::Command command; s32 x, y, z; actual >> command >> x >> y >> z;
			if (command == Network::Command::BlockDataUpdate) metadata.emplace(x, y, z);
			else if (command == Network::Command::BlockInvUpdate) inventories.emplace(x, y, z);
			else TS_FAIL("unexpected chunk metadata packet");
		}
		TS_ASSERT_EQUALS(metadata.size(), 2u); TS_ASSERT_EQUALS(inventories.size(), 2u);
		TS_ASSERT(metadata.count({-31, 50, -29})); TS_ASSERT(metadata.count({-17, 62, -1}));
		TS_ASSERT(metadata == inventories);
	}
	void testSingleCellChangesUseADeltaAndResubscriptionUsesAFullSnapshot() {
		Registry definitions; Registry::setInstance(definitions);
		definitions.registerBlock<ServerBlock>(BLOCK_AIR); definitions.registerItem<Item>({},BLOCK_AIR,"air");
		ScriptEngine scripts; PlayerList players; Dimension dimension{0,"test:world","world"};
		ServerWorld world(players,dimension,42); WorldController worlds(definitions); Server server;
		ServerCommandHandler commands(scripts,server,worlds,players,definitions); world.setServer(&commands);
		auto session=Network::LocalSession::create(); ClientInfo client{0,session};
		auto &chunk=world.getOrCreateChunk(0,0,0); chunk.setInitialized(true);
		chunk.sendData(client); Network::Packet packet; TS_ASSERT(session->receiveOnClient(packet));
		chunk.setDataRaw(2,3,4,0x12340000); chunk.lightmap().setLightDataRaw(2,3,4,0x5a);
		chunk.sendChanges(client); TS_ASSERT(session->receiveOnClient(packet));
		TS_ASSERT_EQUALS(packet.getDataSize(),23u);
		Network::Command command; s32 x,y,z; u16 count,index; u32 block; u8 light;
		packet >> command >> x >> y >> z >> count >> index >> block >> light;
		TS_ASSERT(command==Network::Command::ChunkDelta); TS_ASSERT_EQUALS(count,1);
		TS_ASSERT_EQUALS(index,(4*16+3)*16+2); TS_ASSERT_EQUALS(block,0x12340000u); TS_ASSERT_EQUALS(light,0x5a);
		chunk.sendChanges(client); TS_ASSERT(!session->receiveOnClient(packet));
		chunk.setReadyToSend(); chunk.sendChanges(client); TS_ASSERT(session->receiveOnClient(packet));
		packet >> command; TS_ASSERT(command==Network::Command::ChunkData);
	}

	void testEvictionAndLazyReloadPreserveBlocksAndContainerMetadata() {
		namespace fs=std::filesystem;
		struct Restore { fs::path original=fs::current_path(); ~Restore() { fs::current_path(original); } } restore;
		auto directory=fs::temp_directory_path()/("opencraft-residency-"+std::to_string(getpid()));
		fs::create_directories(directory); fs::current_path(directory);
		Registry definitions; Registry::setInstance(definitions);
		definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isOpaque(false);
		definitions.registerItem<Item>({},BLOCK_AIR,"air");
		u16 stone=definitions.registerBlock<ServerBlock>("test:stone").id();
		PlayerList players; Dimension dimension{0,"test:world","world"}; std::deque<ServerWorld> worlds;
		worlds.emplace_back(players,dimension,42); auto &world=worlds.front();
		auto &chunk=world.getOrCreateChunk(-40,30,1); chunk.setInitialized(true); chunk.setBlockRaw(1,2,3,stone); chunk.setModified(true);
		chunk.addBlockData(1,2,3,9,3)->meta.setInt("marker",17);
		WorldSaveBasicBackend backend(worlds,players); backend.save("residency");
		chunk.getBlockData(1,2,3)->meta.setInt("marker",29);
		for(unsigned i=0;i<30;++i) world.evictDistantChunks();
		TS_ASSERT(!world.getChunk(-40,30,1));
		auto &reloaded=world.getOrCreateChunk(-40,30,1); TS_ASSERT(!reloaded.isInitialized()); world.generateChunk(reloaded);
		TS_ASSERT_EQUALS(reloaded.getBlock(1,2,3),stone);
		TS_ASSERT(reloaded.getBlockData(1,2,3));
		if(reloaded.getBlockData(1,2,3)) TS_ASSERT_EQUALS(reloaded.getBlockData(1,2,3)->meta.get<int>("marker"),29);
		std::deque<ServerWorld> reopened; reopened.emplace_back(players,dimension,42);
		WorldSaveBasicBackend reader(reopened,players); reader.load("residency");
		TS_ASSERT_EQUALS(reopened.front().chunks().size(),0u);
		auto &saved=reopened.front().getOrCreateChunk(-40,30,1); reopened.front().generateChunk(saved);
		TS_ASSERT_EQUALS(saved.getBlock(1,2,3),stone); TS_ASSERT_EQUALS(saved.getBlockData(1,2,3)->meta.get<int>("marker"),29);
	}

	void testVerticalTravelReleasesDistantChunksInTheSameColumn() {
		Registry definitions; Registry::setInstance(definitions);
		definitions.registerBlock<ServerBlock>(BLOCK_AIR); definitions.registerItem<Item>({},BLOCK_AIR,"air");
		PlayerList players; auto session=Network::LocalSession::create(); ClientInfo client{0,session};
		auto *player=players.connectPlayer("tester",client); player->setPosition(1,1,1); player->setReady(true);
		Dimension dimension{0,"test:world","world"}; ServerWorld world(players,dimension,42);
		world.getOrCreateChunk(0,0,0).setInitialized(true); world.getOrCreateChunk(0,0,100).setInitialized(true);
		unsigned saved=0; world.setChunkStorage({},[&](ServerChunk &) { ++saved; }); world.evictDistantChunks();
		TS_ASSERT(world.getChunk(0,0,0)); TS_ASSERT(!world.getChunk(0,0,100)); TS_ASSERT_EQUALS(saved,1u);
	}

};
#endif // CHUNKSTREAMINGPERFORMANCETESTS_HPP_
