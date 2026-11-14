#ifndef PORTALINTEGRATIONTESTS_HPP_
#define PORTALINTEGRATIONTESTS_HPP_

#include <cxxtest/TestSuite.h>
#include <filesystem>
#include <unistd.h>

#include "common/core/GameClock.hpp"
#include "common/core/GameTime.hpp"
#include "common/core/Registry.hpp"
#include "common/core/LoggerHandler.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/lua/ServerModLoader.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerItem.hpp"
#include "server/world/BucketInteraction.hpp"
#include "server/world/ServerWorld.hpp"
#include "server/world/WorldController.hpp"

class PortalIntegrationTests : public CxxTest::TestSuite {
	struct Fixture {
		std::filesystem::path original = std::filesystem::current_path();
		ScriptEngine scripts;
		Registry definitions;
		PlayerList players;
		WorldController worlds{definitions};
		ServerModLoader loader{scripts, worlds};
		Server server;
		ServerCommandHandler commands{scripts, server, worlds, players, definitions};
		std::shared_ptr<Network::LocalSession> session;
		ClientInfo client{1, {}};
		ServerPlayer *player = nullptr;
		Fixture() {
			std::filesystem::current_path(OM_TEST_SOURCE_DIR);
			Registry::setInstance(definitions);
			auto &air = definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0);
			air.isOpaque(false); air.isCollidable(false);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air").setIsBlock(true);
			loader.loadMods(); scripts.luaCore().setRegistry(&definitions);
			server.init();
			session = Network::LocalSession::connect();
			client = ClientInfo{1, session};
			worlds.setServer(commands); worlds.init(players, 42);
			player = players.connectPlayer("Player", client); player->setReady(true);
		}
		~Fixture() { std::filesystem::current_path(original); }
		void build(ServerWorld &world, Vector3i origin, bool ignite = true) {
			for (int x = -3; x <= 3; ++x) for (int y = -3; y <= 3; ++y) for (int z = -2; z <= 4; ++z) {
				Vector3i point = origin + Vector3i{x, y, z};
				world.getOrCreateChunk((point.x & -CHUNK_WIDTH) / CHUNK_WIDTH,
					(point.y & -CHUNK_DEPTH) / CHUNK_DEPTH, (point.z & -CHUNK_HEIGHT) / CHUNK_HEIGHT).setInitialized(true);
			}
			u16 obsidian = definitions.getBlockFromStringID("default:obsidian").id();
			for (int x = -1; x <= 2; ++x) for (int z = -1; z <= 3; ++z)
				if (x == -1 || x == 2 || z == -1 || z == 3) world.setBlock(origin.x + x, origin.y, origin.z + z, obsidian);
			if (ignite) world.ignitePortal(origin);
		}
	};
	public:
		void setUp() override {
			static LoggerHandler logger; LoggerHandler::setInstance(logger);
			static GameClock clock; GameClock::setInstance(clock);
		}

		void testActualPortalCallbackFollowsHighLinksAndPersistsBothEnds() {
			Fixture f; auto &source = f.worlds.getWorld(0); auto &target = f.worlds.getWorld(1);
			Vector3i from{16, 24, 130}, to{2, 3, 130}; f.build(source, from); f.build(target, to);
			f.player->setPosition(16.5, 22.5, 130);
			auto &portal = static_cast<const ServerBlock &>(f.definitions.getBlockFromStringID("default:portal"));
			TS_ASSERT(portal.onBlockActivated({from.x, from.y, from.z}, *f.player, source, f.client, f.commands));
			TS_ASSERT_EQUALS(f.player->dimension(), 1); TS_ASSERT_DELTA(f.player->z(), 130.5, 0.001);
			BlockData *outbound = source.getBlockData(from.x, from.y, from.z);
			BlockData *inbound = target.getBlockData(to.x, to.y, to.z);
			TS_ASSERT(outbound); TS_ASSERT(inbound); if (!outbound || !inbound) return;
			TS_ASSERT_EQUALS(outbound->meta.get<int>("portal_x"), to.x);
			TS_ASSERT_EQUALS(inbound->meta.get<int>("portal_x"), from.x);
			TS_ASSERT(!f.commands.usePortal(1, to.x, to.y, to.z));
			f.players.disconnectPlayer("Player"); f.players.addPlayer("returner", false);
			ClientInfo returnClient{2, f.session}; auto *returner = f.players.connectPlayer("returner", returnClient);
			TS_ASSERT(returner); if (!returner) return;
			returner->setReady(true); returner->setDimension(1); returner->setPosition(2.5, 1.5, 130);
			TS_ASSERT(f.commands.usePortal(2, to.x, to.y, to.z));
			TS_ASSERT_EQUALS(returner->dimension(), 0); TS_ASSERT_DELTA(returner->x(), 16.5, 0.001);
			std::filesystem::path directory = std::filesystem::temp_directory_path() / ("oldworld-portal-test-" + std::to_string(getpid()));
			std::filesystem::create_directories(directory); std::filesystem::current_path(directory);
			f.worlds.save("links"); outbound->meta.setInt("portal_x", 999); inbound->meta.setInt("portal_z", 999);
			f.worlds.load("links");
			TS_ASSERT_EQUALS(source.getBlockData(from.x, from.y, from.z)->meta.get<int>("portal_x"), to.x);
			TS_ASSERT_EQUALS(target.getBlockData(to.x, to.y, to.z)->meta.get<int>("portal_z"), from.z);
		}

		void testIgniterFindsSafeDestinationInGeneratedNether() {
			Fixture f; auto &source = f.worlds.getWorld(0); auto &target = f.worlds.getWorld(1);
			Vector3i from{8, 8, 64}; f.build(source, from, false);
			f.player->inventory().setStack(0, 0, "default:flint_and_steel", 1); f.player->setHeldItemSlot(0);
			auto &igniter = static_cast<const ServerItem &>(f.definitions.getItemFromStringID("default:flint_and_steel"));
			auto &obsidian = const_cast<Block &>(f.definitions.getBlockFromStringID("default:obsidian"));
			TS_ASSERT(igniter.onItemActivated({8, 8, 63}, obsidian, *f.player, source, f.client, f.commands));
			TS_ASSERT_EQUALS(f.player->heldItemStack().damage(), 1);
			f.player->setPosition(8.5, 6.5, 64); bool traveled = f.commands.usePortal(1, 8, 8, 64);
			TS_ASSERT(traveled); if (!traveled) return;
			TS_ASSERT_EQUALS(f.player->dimension(), 1);
			DoubleBox body = f.player->hitbox(); body.move(f.player->x(), f.player->y(), f.player->z());
			PlayerEnvironment environment = playerEnvironment(target, body);
			TS_ASSERT(environment.loaded); TS_ASSERT(!environment.suffocating); TS_ASSERT(!environment.lava);
			Vector3i origin, axis;
			bool found = target.portalFrameAt({int(std::floor(f.player->x())), int(std::floor(f.player->y())), int(std::floor(f.player->z()))}, origin, axis);
			TS_ASSERT(found); if (!found) return;
			TS_ASSERT_EQUALS(target.getBlock(origin.x, origin.y, origin.z - 1), obsidian.id());
		}

		void testSolidDestinationFailsWithoutExcavatingOrMovingPlayer() {
			Fixture f; auto &source = f.worlds.getWorld(0); auto &target = f.worlds.getWorld(1);
			f.build(source, {8, 8, 64}); f.player->setPosition(8.5, 6.5, 64);
			u16 obsidian = f.definitions.getBlockFromStringID("default:obsidian").id();
			for (int cy = -1; cy <= 1; ++cy) for (int cx = -1; cx <= 1; ++cx) for (int cz = 0; cz < 4; ++cz) {
				auto &chunk = target.getOrCreateChunk(cx, cy, cz);
				for (int z = 0; z < CHUNK_HEIGHT; ++z) for (int y = 0; y < CHUNK_DEPTH; ++y) for (int x = 0; x < CHUNK_WIDTH; ++x)
					chunk.setDataRaw(x, y, z, obsidian);
				chunk.setInitialized(true);
			}
			TS_ASSERT(!f.commands.usePortal(1, 8, 8, 64));
			TS_ASSERT_EQUALS(f.player->dimension(), 0); TS_ASSERT_DELTA(f.player->x(), 8.5, 0.001);
			TS_ASSERT(!source.getBlockData(8, 8, 64));
			for (const auto &entry : target.chunks()) {
				if (!entry.second->isInitialized()) continue;
				for (int z = 0; z < CHUNK_HEIGHT; ++z) for (int y = 0; y < CHUNK_DEPTH; ++y) for (int x = 0; x < CHUNK_WIDTH; ++x)
					TS_ASSERT_EQUALS(entry.second->getBlock(x, y, z), obsidian);
			}
		}

		void testHeldItemLuaHelperConsumesOnlyAvailableOwnedStacks() {
			Fixture f; f.player->inventory().setStack(0, 0, "default:bone_meal", 2); f.player->setHeldItemSlot(0);
			f.scripts.lua()["test_server"] = &f.commands; f.scripts.lua()["test_player"] = f.player;
			auto result = f.scripts.lua().safe_script("return test_server:consume_held_item(test_player, 1, 1)", sol::script_pass_on_error);
			TS_ASSERT(result.valid()); if (result.valid()) TS_ASSERT(result.get<bool>());
			TS_ASSERT_EQUALS(f.player->heldItemStack().amount(), 1);
			TS_ASSERT(!f.commands.consumeHeldItem(*f.player, 1, 2));
			TS_ASSERT(!f.commands.consumeHeldItem(*f.player, 2, 1));
			f.player->setCreative(true); TS_ASSERT(f.commands.consumeHeldItem(*f.player, 1, 1));
			TS_ASSERT_EQUALS(f.player->heldItemStack().amount(), 1);
			f.player->inventory().clearStack(0, 0); TS_ASSERT(!f.commands.consumeHeldItem(*f.player, 1, 1));
		}

		void testBeeContentIsRemoved() {
			Fixture f;
			for (const char *id : {"default:wild_hive", "default:beehive", "default:honeycomb", "default:honey", "default:beeswax", "default:bee_smoker"}) {
				TS_ASSERT(!f.definitions.hasBlock(id)); TS_ASSERT(!f.definitions.hasItem(id));
			}
		}

		void testBucketSourcesPlacementCreativeAndNetherEvaporation() {
			Fixture f; auto &world=f.worlds.getWorld(0); auto &nether=f.worlds.getWorld(1);
			world.getOrCreateChunk(0,0,2).setInitialized(true); nether.getOrCreateChunk(0,0,2).setInitialized(true);
			f.player->setHeldItemSlot(0);
			auto activate=[&](const char *id,int x,int y,int z,int face) {
				f.player->inventory().setStack(0,0,id,1);
				return useBucket(f.definitions.getItemFromStringID(id),{x,y,z},face,*f.player,world,f.commands);
			};
			u16 water=f.definitions.getBlockFromStringID("default:water").id();
			u16 flow=f.definitions.getBlockFromStringID("default:water_flowing").id();
			u16 stone=f.definitions.getBlockFromStringID("default:stone").id();
			world.setBlock(8,8,65,flow); TS_ASSERT(!activate("default:bucket_empty",8,8,65,2));
			world.setBlock(8,8,65,water); TS_ASSERT(activate("default:bucket_empty",8,8,65,2));
			TS_ASSERT_EQUALS(world.getBlock(8,8,65),0); TS_ASSERT_EQUALS(f.player->heldItemStack().stringID(),"default:bucket_water");
			world.setBlock(8,8,64,stone); TS_ASSERT(activate("default:bucket_water",8,8,64,2));
			TS_ASSERT_EQUALS(world.getBlock(8,8,65),water); TS_ASSERT_EQUALS(f.player->heldItemStack().stringID(),"default:bucket_empty");
			world.setBlock(8,8,65,stone); TS_ASSERT(!activate("default:bucket_lava",8,8,64,2));
			TS_ASSERT_EQUALS(f.player->heldItemStack().stringID(),"default:bucket_lava");
			TS_ASSERT(!activate("default:bucket_lava",15,8,64,0));
			f.player->setCreative(true); world.setBlock(8,8,65,water);
			TS_ASSERT(activate("default:bucket_empty",8,8,65,2)); TS_ASSERT_EQUALS(f.player->heldItemStack().stringID(),"default:bucket_empty");
			TS_ASSERT_EQUALS(world.getBlock(8,8,65),0); f.player->setCreative(false);
			nether.setBlock(8,8,64,stone); f.player->inventory().setStack(0,0,"default:bucket_water",1);
			TS_ASSERT(useBucket(f.player->heldItemStack().item(),{8,8,64},2,*f.player,nether,f.commands));
			TS_ASSERT_EQUALS(nether.getBlock(8,8,65),0); TS_ASSERT_EQUALS(f.player->heldItemStack().stringID(),"default:bucket_empty");
			TS_ASSERT(activate("default:bucket_milk",8,8,64,-1)); TS_ASSERT_EQUALS(f.player->heldItemStack().stringID(),"default:bucket_empty");
		}

		void testLavaFuelLeavesTheEmptyBucket() {
			Fixture f; auto &world=f.worlds.getWorld(0); auto &chunk=world.getOrCreateChunk(0,0,2);
			chunk.setInitialized(true); auto &furnace=static_cast<const ServerBlock &>(f.definitions.getBlockFromStringID("default:furnace"));
			world.setBlock(8,8,65,furnace.id());
			auto *data=world.getBlockData(8,8,65); TS_ASSERT(data); if(!data)return;
			data->inventory.setStack(0,0,"default:copper_ore",1); data->inventory.setStack(2,0,"default:bucket_lava",1);
			furnace.onTick({8,8,65},chunk,world,f.commands);
			TS_ASSERT_EQUALS(data->inventory.getStack(2,0).stringID(),"default:bucket_empty");
		}

		void testWriteCreativeExpansionGalleryFixture() {
			Fixture f; auto &world = f.worlds.getWorld(0); f.worlds.setCreative(true); f.player->setCreative(true);
			auto &chunk = world.getOrCreateChunk(0, 0, 4);
			u16 floor = f.definitions.getBlockFromStringID("default:grass").id();
			for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) chunk.setBlockRaw(x, y, 0, floor);
			const char *blocks[] = {"basalt", "chiseled_basalt", "nether_bricks", "quartz_ore", "quartz_pillar", "chiseled_quartz", "cave_moss", "cave_lichen", "crystal_lamp"};
			for (int i = 0; i < 9; ++i) chunk.setBlockRaw(11, 3 + i, 1, f.definitions.getBlockFromStringID(std::string("default:") + blocks[i]).id());
			const char *plants[] = {"potato_eyes", "tomato_seeds", "barley_seeds", "cave_fungi", "cave_mycena", "cave_crystal", "wild_hive", "beehive"};
			for (int i = 0; i < 8; ++i) {
				std::string id = std::string("default:") + plants[i]; if (!f.definitions.hasBlock(id)) continue;
				chunk.setBlockRaw(8, 4 + i, 1, f.definitions.getBlockFromStringID(id).id());
				if (i < 3) {
					chunk.setBlockRaw(8, 4 + i, 0, f.definitions.getBlockFromStringID("default:farmland").id());
					chunk.setBlockState(8, 4 + i, 0, 7); chunk.setBlockState(8, 4 + i, 1, 7);
				}
			}
			chunk.setBlockRaw(7,10,1,f.definitions.getBlockFromStringID("default:rose").id());
			chunk.setBlockRaw(7,11,1,f.definitions.getBlockFromStringID("default:dandelion").id());
			chunk.setInitialized(true); chunk.setModified(true); chunk.initializeTickingBlocks(); world.initializeChunkLighting(chunk);
			world.spawnMob("default:ocelot", 6.5, 5.5, 129);
			world.spawnMob("default:bat", 8.5, 8.5, 131);
			f.player->setPosition(3.5, 8.0, 129); f.player->setRotation(0, 5); GameTime::setTicks(6000);
			f.player->inventory().setStack(0, 0, "default:fish_cod_raw", 1);
			f.player->inventory().setStack(1, 0, "default:tomato", 1);
			f.player->inventory().setStack(2, 0, "default:bone_meal", 99);
			std::filesystem::create_directories("/tmp/oldworld-expansion-qa"); std::filesystem::current_path("/tmp/oldworld-expansion-qa");
			f.worlds.save("gallery"); TS_ASSERT(std::filesystem::exists("saves/gallery/map.sqlite"));
		}
};

#endif
