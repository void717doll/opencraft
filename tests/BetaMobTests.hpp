#ifndef BETAMOBTESTS_HPP_
#define BETAMOBTESTS_HPP_

#include <cxxtest/TestSuite.h>
#include <limits>
#include "common/core/Registry.hpp"
#include "common/scene/component/MobComponent.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/scene/component/RotationComponent.hpp"
#include "common/world/RetroMobSpawn.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerWorld.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/WorldController.hpp"
#include "server/lua/ScriptEngine.hpp"

class BetaMobTests : public CxxTest::TestSuite {
	static void registerModel(Registry &definitions, const std::string &name, BetaMobKind kind, DoubleBox box, bool hostile) {
		auto entity = definitions.registerEntity(name);
		auto &models = definitions.entityRegistry();
		auto &mob = models.emplace<MobComponent>(entity);
		mob.hostile = hostile;
		mob.moveSpeed = 0;
		mob.health = 100;
		models.emplace<BetaMobComponent>(entity).kind = kind;
		models.emplace<DoubleBox>(entity, box);
		models.emplace<RotationComponent>(entity);
		models.emplace<DrawableDef>(entity).setMobModel(u8(kind));
	}

	static void prepareFloor(ServerWorld &world) {
		auto &chunk = world.getOrCreateChunk(0, 0, 0);
		for (int x = 0; x < 16; ++x)
			for (int y = 0; y < 16; ++y) chunk.setBlockRaw(x, y, 0, 1);
		chunk.setInitialized(true);
	}

	public:
		void testAttackSendsActualDamageAndHitPosition() {
			Registry definitions; Registry::setInstance(definitions);
			definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
			definitions.registerBlock<ServerBlock>("test:stone");
			definitions.registerItem<Item>({}, BLOCK_AIR, "air");
			definitions.registerDimension("test:world", "world");
			registerModel(definitions, "default:zombie", BetaMobKind::Zombie, {-0.3, -0.3, 0, 0.6, 0.6, 1.8}, true);
			PlayerList players; ScriptEngine scripts; WorldController worlds(definitions); worlds.init(players, 42);
			Server server; server.init(); auto session = Network::LocalSession::connect();
			ClientInfo client{1, session}; auto *player = players.connectPlayer("tester", client);
			TS_ASSERT(player); if (!player) return;
			player->setPosition(8, 8, 1); player->setRotation(0, 0);
			ServerCommandHandler commands(scripts, server, worlds, players, definitions);
			auto &world = worlds.getWorld(0); world.setServer(&commands); prepareFloor(world);
			TS_ASSERT(world.spawnMob("default:zombie", 10, 8, 1));
			auto target = world.findMobTarget(*player, 3); TS_ASSERT(target != entt::null);
			if (target == entt::null) return;
			world.scene().registry().get<MobComponent>(target).health = 1;
			TS_ASSERT(world.attackMob(*player));
			Network::Packet packet; Network::Command command;
			TS_ASSERT(session->receiveOnClient(packet));
			entt::entity id; u16 damage; double x, y, z;
			packet >> command >> id >> damage >> x >> y >> z;
			TS_ASSERT(packet); TS_ASSERT(command == Network::Command::EntityHurt);
			TS_ASSERT(id == target); TS_ASSERT_EQUALS(damage, 1);
			TS_ASSERT_EQUALS(x, 10); TS_ASSERT_EQUALS(y, 8); TS_ASSERT_DELTA(z, 2.8, 0.0001);
			TS_ASSERT_EQUALS(player->score(),5);
			TS_ASSERT(session->receiveOnClient(packet)); s32 experiencePoints=0; packet >> command >> experiencePoints;
			TS_ASSERT(command==Network::Command::PlayerScore); TS_ASSERT_EQUALS(experiencePoints,5);
			TS_ASSERT(world.attackMob(*player));
			TS_ASSERT(!session->receiveOnClient(packet));
			player->setHealth(0); TS_ASSERT(!world.attackMob(*player));
		}

		void testCowMilkingAndSheepShearingThroughTargetRay() {
			Registry definitions;
			Registry::setInstance(definitions);
			definitions.registerBlock<ServerBlock>("air").getState(0).isCollidable(false);
			definitions.registerBlock<ServerBlock>("test:stone");
			definitions.registerItem<Item>({}, BLOCK_AIR, "air");
			definitions.registerItem<Item>({}, "default:bucket_empty", "bucket").setMaxStackSize(16);
			definitions.registerItem<Item>({}, "default:bucket_milk", "milk").setMaxStackSize(1);
			definitions.registerItem<Item>({}, "default:shears", "shears").setMaxStackSize(1);
			definitions.registerItem<Item>({}, "default:wool", "wool");
			definitions.registerEntity("default:item_drop");
			registerModel(definitions, "default:cow", BetaMobKind::Cow, {-0.45, -0.45, 0, 0.9, 0.9, 1.3}, false);
			registerModel(definitions, "default:sheep", BetaMobKind::Sheep, {-0.45, -0.45, 0, 0.9, 0.9, 1.3}, false);
			PlayerList players;
			Dimension dimension{0, "test:world", "test"};
			ServerWorld world(players, dimension, 42);
			prepareFloor(world);
			auto &player = players.addPlayer("tester", true);
			player.setPosition(8, 8, 1);
			player.setRotation(0, -20);
			player.setHeldItemSlot(0);
			player.inventory().setStack(0, 0, "default:bucket_empty", 2);
			TS_ASSERT(world.spawnMob("default:cow", 10, 8, 1));
			auto cow = world.findMobTarget(player, 3);
			TS_ASSERT(cow != entt::null);
			world.getChunkAtBlockPos(9, 8, 2)->setBlockRaw(9, 8, 2, 1);
			TS_ASSERT(!world.interactMob(player));
			TS_ASSERT_EQUALS(player.heldItemStack().amount(), 2);
			world.getChunkAtBlockPos(9, 8, 2)->setBlockRaw(9, 8, 2, 0);
			TS_ASSERT(world.interactMob(player));
			TS_ASSERT_EQUALS(player.heldItemStack().item().stringID(), "default:bucket_empty");
			TS_ASSERT_EQUALS(player.heldItemStack().amount(), 1);
			TS_ASSERT_EQUALS(player.inventory().getStack(1, 0).item().stringID(), "default:bucket_milk");
			TS_ASSERT_EQUALS(player.inventory().getStack(1, 0).amount(), 1);

			player.setPosition(8, 11, 1);
			player.inventory().setStack(0, 0, "default:shears", 1);
			TS_ASSERT(world.spawnMob("default:sheep", 10, 11, 1));
			auto sheep = world.findMobTarget(player, 3);
			TS_ASSERT(sheep != entt::null);
			if (sheep == entt::null) return;
			auto &registry = world.scene().registry();
			registry.get<MobComponent>(sheep).randomState = 123;
			TS_ASSERT(world.interactMob(player));
			TS_ASSERT(registry.get<BetaMobComponent>(sheep).sheared);
			TS_ASSERT(registry.get<DrawableDef>(sheep).mobAppearance() & 16);
			TS_ASSERT(!world.interactMob(player));
			TS_ASSERT_EQUALS(player.heldItemStack().item().stringID(), "default:shears");
			TS_ASSERT_EQUALS(player.heldItemStack().amount(), 1);
			unsigned wool = 0;
			for (u16 y = 0; y < player.inventory().height(); ++y)
				for (u16 x = 0; x < player.inventory().width(); ++x) {
					const auto &stack = player.inventory().getStack(x, y);
					if (stack.amount() && stack.item().stringID() == "default:wool") wool += stack.amount();
				}
			TS_ASSERT_EQUALS(wool, 0);
			registry.view<ItemStack>().each([&](const auto &stack) { if (stack.item().stringID() == "default:wool") wool += stack.amount(); });
			TS_ASSERT_EQUALS(wool, 3);
		}

		void testUnchangedMobHeadingDoesNotResendItsRotation() {
			Registry definitions; Registry::setInstance(definitions);
			definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
			definitions.registerBlock<ServerBlock>("test:stone");
			registerModel(definitions,"default:cow",BetaMobKind::Cow,{-0.45,-0.45,0,0.9,0.9,1.3},false);
			PlayerList players; Dimension dimension{0,"test:world","world"}; ServerWorld world(players,dimension,42);
			prepareFloor(world);
			TS_ASSERT(world.spawnMob("default:cow",8,8,1));
			world.updateMobs();
			world.scene().registry().view<RotationComponent>().each([](auto &rotation) { rotation.isUpdated=false; });
			world.updateMobs();
			world.scene().registry().view<RotationComponent>().each([&](const auto &rotation) { TS_ASSERT(!rotation.isUpdated); });
		}

		void testSkeletonProjectileAndCreeperBlastSimulation() {
			Registry definitions;
			Registry::setInstance(definitions);
			definitions.registerBlock<ServerBlock>("air").getState(0).isCollidable(false);
			definitions.registerBlock<ServerBlock>("test:stone");
			registerModel(definitions, "default:skeleton", BetaMobKind::Skeleton, {-0.3, -0.3, 0, 0.6, 0.6, 1.8}, true);
			registerModel(definitions, "default:creeper", BetaMobKind::Creeper, {-0.3, -0.3, 0, 0.6, 0.6, 1.8}, true);
			registerModel(definitions, "default:cow", BetaMobKind::Cow, {-0.45, -0.45, 0, 0.9, 0.9, 1.3}, false);
			PlayerList players;
			auto &player = players.addPlayer("tester", true);
			ClientInfo client{1, {}};
			player.setClient(&client);
			player.setReady(true);
			player.setPosition(8, 8, 1);
			Dimension dimension{0, "test:world", "test"};
			ServerWorld world(players, dimension, 42);
			prepareFloor(world);
			TS_ASSERT(world.spawnMob("default:skeleton", 3, 8, 1));
			world.updateMobs();
			auto &registry = world.scene().registry();
			entt::entity arrow = entt::null;
			unsigned count = 0;
			registry.view<BetaMobComponent>().each([&](auto entity, auto &traits) {
				if (traits.kind == BetaMobKind::Arrow) { arrow = entity; ++count; }
			});
			TS_ASSERT_EQUALS(count, 1);
			if (arrow == entt::null) return;
			double initialX = registry.get<PositionComponent>(arrow).x;
			for (int tick = 0; tick < 3; ++tick) world.updateMobs();
			TS_ASSERT(registry.valid(arrow));
			if (!registry.valid(arrow)) return;
			TS_ASSERT(registry.get<PositionComponent>(arrow).x > initialX + 1);
			TS_ASSERT_EQUALS(registry.get<MobComponent>(arrow).age, 3);
			registry.clear();

			TS_ASSERT(world.spawnMob("default:creeper", 10, 8, 1));
			TS_ASSERT(world.spawnMob("default:cow", 12, 8, 1));
			entt::entity creeper = entt::null, cow = entt::null;
			registry.view<BetaMobComponent>().each([&](auto entity, auto &traits) {
				if (traits.kind == BetaMobKind::Creeper) creeper = entity;
				if (traits.kind == BetaMobKind::Cow) cow = entity;
			});
			if (creeper == entt::null || cow == entt::null) return;
			for (int tick = 0; tick < 29; ++tick) world.updateMobs();
			TS_ASSERT(registry.valid(creeper));
			if (!registry.valid(creeper)) return;
			TS_ASSERT_EQUALS(registry.get<BetaMobComponent>(creeper).fuse, 29);
			TS_ASSERT_EQUALS(registry.get<MobComponent>(cow).health, 100);
			world.updateMobs();
			TS_ASSERT(!registry.valid(creeper));
			TS_ASSERT(registry.valid(cow));
			if (registry.valid(cow)) TS_ASSERT(registry.get<MobComponent>(cow).health < 100);
		}

		void testDeathDropsPreserveInventoryAmounts() {
			Registry definitions;
			Registry::setInstance(definitions);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air");
			definitions.registerItem<Item>({}, "test:stone", "stone");
			definitions.registerEntity("default:item_drop");
			PlayerList players;
			auto &player = players.addPlayer("tester", true);
			player.inventory().setStack(0, 0, "test:stone", 17);
			player.inventory().setStack(2, 3, "test:stone", 23);
			Dimension dimension{0, "test:world", "world"};
			ServerWorld world(players, dimension, 42);
			world.dropPlayerItems(player);
			TS_ASSERT_EQUALS(player.inventory().getStack(0, 0).amount(), 0);
			TS_ASSERT_EQUALS(player.inventory().getStack(2, 3).amount(), 0);
			unsigned amount = 0;
			world.scene().registry().view<ItemStack>().each([&](auto, const auto &stack) { amount += stack.amount(); });
			TS_ASSERT_EQUALS(amount, 40);
			world.dropPlayerItems(player);
			TS_ASSERT_EQUALS(world.scene().registry().view<ItemStack>().size(), 2);
		}

		void testTraitsPacketPreservesAllFieldsAndFollowingRecord() {
			MobComponent original;
			original.health = 7;
			original.dropItem = "default:bone";
			BetaMobComponent traits;
			traits.kind = BetaMobKind::Wolf;
			traits.size = 4;
			traits.fuse = 29;
			traits.anger = 800;
			traits.eggTimer = 11999;
			traits.jumpTimer = 17;
			traits.attackCharge = -40;
			traits.grounded = true;
			traits.sheared = true;
			traits.tamed = true;
			traits.sitting = true;
			traits.color = 15;
			traits.saddled = true;
			traits.owner = "owner";
			traits.target = "target";
			traits.velocity = {0.25f, -0.5f, 1.25f};
			traits.waypoint = {-16, 32, 48};
			sf::Packet packet;
			packet << original << traits << u32(0x12345678);
			MobComponent restoredMob;
			BetaMobComponent restored;
			u32 marker = 0;
			packet >> restoredMob >> restored >> marker;
			TS_ASSERT(packet);
			TS_ASSERT(packet.endOfPacket());
			TS_ASSERT_EQUALS(restoredMob.health, 7);
			TS_ASSERT_EQUALS(restoredMob.dropItem, "default:bone");
			TS_ASSERT_EQUALS(u8(restored.kind), u8(BetaMobKind::Wolf));
			TS_ASSERT_EQUALS(restored.size, 4);
			TS_ASSERT_EQUALS(restored.fuse, 29);
			TS_ASSERT_EQUALS(restored.anger, 800);
			TS_ASSERT_EQUALS(restored.eggTimer, 11999);
			TS_ASSERT_EQUALS(restored.jumpTimer, 17);
			TS_ASSERT_EQUALS(restored.attackCharge, -40);
			TS_ASSERT(restored.grounded && restored.sheared && restored.tamed && restored.sitting && restored.saddled);
			TS_ASSERT_EQUALS(restored.color, 15);
			TS_ASSERT_EQUALS(restored.owner, "owner");
			TS_ASSERT_EQUALS(restored.target, "target");
			TS_ASSERT_EQUALS(restored.velocity.x, 0.25f);
			TS_ASSERT_EQUALS(restored.velocity.y, -0.5f);
			TS_ASSERT_EQUALS(restored.velocity.z, 1.25f);
			TS_ASSERT_EQUALS(restored.waypoint.x, -16);
			TS_ASSERT_EQUALS(restored.waypoint.y, 32);
			TS_ASSERT_EQUALS(restored.waypoint.z, 48);
			TS_ASSERT_EQUALS(marker, 0x12345678u);
		}

		void testSlimeChunkMatchesJavaChunkSeedOracle() {
			const int coordinates[] = {std::numeric_limits<int>::min(), -1000000, -101, -10, -9, -6,
				-1, 0, 1, 2, 10, 17, 99, 10000, 1000000, std::numeric_limits<int>::max()};
			const std::uint64_t expected[] = {0x620a4a73e529a43dULL, 0x806b3efeebd6eacfULL,
				0xe8912caa1c1b3b84ULL, 0x8f019290e7ed139aULL};
			std::size_t sample = 0;
			// oracle uses the literal chunk.java expression with java.util.Random.
			for (std::int64_t seed : {0LL, 42LL, -99LL, 9876543210123456LL}) {
				std::uint64_t hash = 14695981039346656037ULL;
				for (int x : coordinates)
					for (int y : coordinates) hash = (hash ^ (retro::slimeChunk(seed, x, y) ? 1ULL : 0ULL)) * 1099511628211ULL;
				TS_ASSERT_EQUALS(hash, expected[sample++]);
			}
			TS_ASSERT(retro::slimeChunk(42, -9, 0));
			TS_ASSERT(retro::slimeChunk(42, -6, 0));
			TS_ASSERT(retro::slimeChunk(42, 2, 0));
			TS_ASSERT(!retro::slimeChunk(42, 0, 0));
		}

		void testNetherCavitySearchUsesLoadedFloorRatherThanRoof() {
			Registry definitions;
			Registry::setInstance(definitions);
			definitions.registerBlock<ServerBlock>("air").getState(0).isCollidable(false);
			definitions.registerBlock<ServerBlock>("test:stone");
			auto &lava = definitions.registerBlock<ServerBlock>("test:lava").getState(0);
			lava.isCollidable(false);
			lava.drawType(BlockDrawType::Liquid);
			PlayerList players;
			Dimension dimension{1, "test:nether", "test"};
			ServerWorld world(players, dimension, 42);
			auto &cavity = world.getOrCreateChunk(0, 0, 49 / CHUNK_HEIGHT);
			auto &roof = world.getOrCreateChunk(0, 0, 127 / CHUNK_HEIGHT);
			for (int x = 0; x < 16; ++x)
				for (int y = 0; y < 16; ++y) {
					cavity.setBlockRaw(x, y, 49 % CHUNK_HEIGHT, 1);
					roof.setBlockRaw(x, y, 127 % CHUNK_HEIGHT, 1);
				}
			cavity.setInitialized(true);
			roof.setInitialized(true);
			DoubleBox pig{-0.45, -0.45, 0, 0.9, 0.9, 0.9};
			DoubleBox ghast{-2, -2, 0, 4, 4, 4};
			TS_ASSERT_EQUALS(retro::mobCavityFloor(world, pig, 8.5, 8.5, 52), 50);
			TS_ASSERT_EQUALS(retro::mobCavityFloor(world, ghast, 8.5, 8.5, 52), 50);
			TS_ASSERT_EQUALS(retro::mobCavityFloor(world, pig, 8.5, 8.5, 127), -1);
			TS_ASSERT_EQUALS(retro::mobCavityFloor(world, ghast, 0.5, 8.5, 52), -1);
			cavity.setBlockRaw(8, 8, 50 % CHUNK_HEIGHT, 2);
			TS_ASSERT_EQUALS(retro::mobCavityFloor(world, pig, 8.5, 8.5, 52), -1);
			cavity.setBlockRaw(8, 8, 50 % CHUNK_HEIGHT, 0);
			cavity.setInitialized(false);
			TS_ASSERT_EQUALS(retro::mobCavityFloor(world, pig, 8.5, 8.5, 52), -1);
		}
};

#endif // BETAMOBTESTS_HPP_
