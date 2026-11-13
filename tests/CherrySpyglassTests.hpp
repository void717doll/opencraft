#ifndef CHERRYSPYGLASSTESTS_HPP_
#define CHERRYSPYGLASSTESTS_HPP_

#include <array>
#include <cxxtest/TestSuite.h>
#include "client/world/SpyglassState.hpp"
#include "common/core/Registry.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/world/ExpansionTerrain.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerWorld.hpp"

class CherrySpyglassTests : public CxxTest::TestSuite {
	static u16 id(const char *name) { return Registry::getInstance().getBlockFromStringID(name).id(); }
	static void definitions(Registry &registry) {
		Registry::setInstance(registry);
		auto &air = registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0); air.isCollidable(false); air.isOpaque(false);
		registry.registerBlock<ServerBlock>("default:stone");
		registry.registerBlock<ServerBlock>("default:grass").addGroup("group:om_material_dirt", 1);
		registry.registerBlock<ServerBlock>("default:dirt").addGroup("group:om_material_dirt", 1);
		registry.registerBlock<ServerBlock>("default:cherry_wood");
		auto &leaves = registry.registerBlock<ServerBlock>("default:cherry_leaves").getState(0);
		leaves.drawType(BlockDrawType::Leaves); leaves.isOpaque(false);
		auto &sapling = registry.registerBlock<ServerBlock>("default:cherry_sapling"); sapling.addGroup("group:om_sapling", 1);
		sapling.getState(0).isCollidable(false); sapling.getState(0).isOpaque(false);
		auto &tree = registry.registerTree("default:cherry");
		tree.setLogBlockID(id("default:cherry_wood")); tree.setLeavesBlockID(id("default:cherry_leaves")); tree.setTrunkHeight(4,6);
		for (const char *item : {BLOCK_AIR, "default:cherry_leaves", "default:cherry_sapling", "default:shears"})
			registry.registerItem<Item>({}, item, item);
	}
	static std::uint64_t hash(const ServerChunk &chunk) {
		std::uint64_t value = 14695981039346656037ULL;
		for (int z = 0; z < CHUNK_HEIGHT; ++z) for (int y = 0; y < CHUNK_DEPTH; ++y) for (int x = 0; x < CHUNK_WIDTH; ++x)
			value = (value ^ chunk.getBlock(x,y,z)) * 1099511628211ULL;
		return value;
	}
public:
	void testSpyglassRequiresNewPressAfterEveryContextChange() {
		SpyglassState state;
		TS_ASSERT_EQUALS(state.fieldOfView(85), 85);
		state.begin(2, 0); state.update(true, true, 2, 0); TS_ASSERT_EQUALS(state.fieldOfView(85), 8);
		state.reset(); TS_ASSERT_EQUALS(state.fieldOfView(85), 85);
		state.begin(2, 0); state.update(true, true, 3, 0); TS_ASSERT(!state.active());
		state.update(true, true, 2, 0); TS_ASSERT(!state.active());
		state.begin(2, 0); state.update(true, false, 2, 0); TS_ASSERT(!state.active());
		state.begin(2, 0); state.update(false, true, 2, 0); TS_ASSERT(!state.active());
		state.begin(2, 0); state.update(true, true, 2, 1); TS_ASSERT(!state.active());
		TS_ASSERT_EQUALS(state.fieldOfView(110), 110);
	}

	void testSaplingGrowthChecksLightObstructionsAndLoadedSpaceBeforeWriting() {
		Registry registry; definitions(registry); PlayerList players;
		Dimension dimension(0, "test:surface", "surface"); ServerWorld world(players, dimension, 42);
		auto &chunk = world.getOrCreateChunk(0,0,0); chunk.setInitialized(true);
		chunk.setBlockRaw(8,8,0,id("default:grass")); chunk.setBlockRaw(8,8,1,id("default:cherry_sapling"));
		chunk.lightmap().setLightDataRaw(8,8,2,0);
		TS_ASSERT(!world.growTree(8,8,1,"default:cherry"));
		chunk.lightmap().setLightDataRaw(8,8,2,15);
		chunk.setBlockRaw(8,8,3,id("default:stone")); auto before = hash(chunk);
		TS_ASSERT(!world.growTree(8,8,1,"default:cherry")); TS_ASSERT_EQUALS(hash(chunk), before);
		chunk.setBlockRaw(8,8,3,0);
		TS_ASSERT(world.growTree(8,8,1,"default:cherry"));
		TS_ASSERT_EQUALS(world.getBlock(8,8,1), id("default:cherry_wood"));
		unsigned leaves = 0;
		for (int z = 1; z < 12; ++z) for (int y = 5; y < 12; ++y) for (int x = 5; x < 12; ++x)
			if (world.getBlock(x,y,z) == id("default:cherry_leaves")) ++leaves;
		TS_ASSERT(leaves > 30); TS_ASSERT(!world.growTree(8,8,1,"default:cherry"));
		chunk.setBlockRaw(0,4,0,id("default:grass")); chunk.setBlockRaw(0,4,1,id("default:cherry_sapling")); chunk.lightmap().setLightDataRaw(0,4,2,15);
		before = hash(chunk); TS_ASSERT(!world.growTree(0,4,1,"default:cherry")); TS_ASSERT_EQUALS(hash(chunk), before);
	}

	void testCherryLeavesRenewSaplingsAndShearsRecoverLeaves() {
		Registry registry; definitions(registry);
		const auto &leaves = registry.getBlockFromStringID("default:cherry_leaves").getState(0);
		const auto &shears = registry.getItemFromStringID("default:shears");
		const auto cut = leaves.getItemDrops(&shears);
		TS_ASSERT_EQUALS(cut.size(), 1u);
		if (!cut.empty()) TS_ASSERT_EQUALS(cut[0].stringID(), "default:cherry_leaves");
		unsigned saplings = 0;
		for (int attempt = 0; attempt < 1000; ++attempt) {
			for (const auto &drop : leaves.getItemDrops()) {
				TS_ASSERT_EQUALS(drop.stringID(), "default:cherry_sapling");
				TS_ASSERT_EQUALS(drop.amount(), 1); ++saplings;
			}
		}
		TS_ASSERT(saplings > 0 && saplings < 150);
	}

	void testCherryGrovesReplayAcrossChunkOrderWithoutChangingBaseTerrain() {
		Registry registry; definitions(registry); PlayerList players;
		Dimension dimension(0, "test:surface", "surface"); ServerWorld forward(players, dimension, 42), backward(players, dimension, 42);
		const Vector2i groves[] = {{2984,-3549},{-1826,-1191},{-3298,814},{-3426,996},{-284,2286},{852,2479},{2274,2583},{-852,3793}};
		retro::Climate climate(42); Vector2i chosen; bool found = false;
		for (auto grove : groves) {
			auto biome = climate.sample(grove.x,grove.y+3,1,1).biomes[0];
			if (biome == retro::Biome::Forest || biome == retro::Biome::Rainforest) { chosen = grove; found = true; break; }
		}
		TS_ASSERT(found); if (!found) return;
		int centerX = int(std::floor(chosen.x / 16.0)), centerY = int(std::floor(chosen.y / 16.0));
		std::vector<Vector3i> positions;
		for (int y = centerY - 1; y <= centerY + 1; ++y) for (int x = centerX - 1; x <= centerX + 1; ++x) {
			for (auto *world : {&forward,&backward}) {
				auto &base = world->heightmap().getOrCreateChunk(x,y); base.blocks().fill(0);
				for (int bx = 0; bx < 16; ++bx) for (int by = 0; by < 16; ++by) {
					for (int z = 0; z <= 94; ++z) base.blocks()[(bx * 16 + by) * 128 + z] = z == 94 ? 2 : 1;
				}
				base.refreshHeight();
				for (int cz : {2,3}) {
					auto &chunk = world->getOrCreateChunk(x,y,cz);
					for (int z = 0; z < CHUNK_HEIGHT; ++z) for (int by = 0; by < 16; ++by) for (int bx = 0; bx < 16; ++bx)
						chunk.setBlockRaw(bx,by,z,z + cz * 32 == 94 ? id("default:grass") : z + cz * 32 < 94 ? id("default:stone") : 0);
				}
			}
			positions.push_back({x,y,2}); positions.push_back({x,y,3});
		}
		std::vector<std::uint64_t> expected; unsigned logs = 0, leaves = 0;
		for (const auto &p : positions) {
			auto &base = forward.heightmap().getOrCreateChunk(p.x,p.y); auto original = base.blocks();
			auto &chunk = forward.getOrCreateChunk(p.x,p.y,p.z); decorateExpansionTerrain(chunk,forward.heightmap(),0,42); expected.push_back(hash(chunk));
			TS_ASSERT(base.blocks() == original);
			for (int z = 0; z < 32; ++z) for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) {
				if (chunk.getBlock(x,y,z) == id("default:cherry_wood")) ++logs;
				if (chunk.getBlock(x,y,z) == id("default:cherry_leaves")) ++leaves;
				if (z + p.z * 32 <= 94) TS_ASSERT_EQUALS(chunk.getBlock(x,y,z), id(z + p.z * 32 == 94 ? "default:grass" : "default:stone"));
			}
		}
		TS_ASSERT(logs >= 4 && leaves > 30);
		for (std::size_t i = positions.size(); i-- > 0;) {
			const auto &p = positions[i]; auto &chunk = backward.getOrCreateChunk(p.x,p.y,p.z);
			decorateExpansionTerrain(chunk,backward.heightmap(),0,42); TS_ASSERT_EQUALS(hash(chunk), expected[i]);
		}
	}
};

#endif // CHERRYSPYGLASSTESTS_HPP_
