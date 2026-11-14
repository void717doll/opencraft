#ifndef EXPANSIONTERRAINTESTS_HPP_
#define EXPANSIONTERRAINTESTS_HPP_

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>
#include <cxxtest/TestSuite.h>
#include "common/core/Registry.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/world/ExpansionTerrain.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerWorld.hpp"

class ExpansionTerrainTests : public CxxTest::TestSuite {
	static u16 id(const char *name) { return Registry::getInstance().getBlockFromStringID(name).id(); }

	static void definitions(Registry &registry) {
		Registry::setInstance(registry);
		auto &air = registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0);
		air.isCollidable(false); air.isOpaque(false);
		for (const char *name : {"stone", "netherrack", "bedrock", "lava", "glowstone", "soul_sand", "grass", "dirt",
			"basalt", "quartz_ore", "cave_moss", "cave_lichen", "cave_fungi", "cave_mycena", "cave_glowworm", "cave_crystal",
			"wild_potato", "wild_tomato", "wild_barley", "wild_hive", "copper_ore", "nether_bricks"}) {
			auto &block = registry.registerBlock<ServerBlock>(std::string("default:") + name);
			std::string value(name);
			if (value == "cave_fungi" || value == "cave_mycena" || value == "cave_glowworm" || value == "cave_crystal"
				|| value == "wild_potato" || value == "wild_tomato" || value == "wild_barley") {
				block.getState(0).isCollidable(false); block.getState(0).isOpaque(false);
			}
			unsigned light = value == "cave_mycena" ? 3 : value == "cave_glowworm" ? 4 : value == "cave_crystal" ? 5 : 0;
			if (light) { block.getState(0).isLightSource(true); block.addGroup("group:om_light_level", light); }
		}
	}

	static std::uint64_t hash(const ServerChunk &chunk) {
		std::uint64_t result = 14695981039346656037ULL;
		for (int z = 0; z < 32; ++z) for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x)
			result = (result ^ chunk.getFullBlock(x, y, z)) * 1099511628211ULL;
		return result;
	}

	static void prepareColumn(HeightmapChunk &base, unsigned kind) {
		base.blocks().fill(0);
		for (int x = 0; x < 16; ++x) for (int y = 0; y < 16; ++y) for (int z = 0; z < 128; ++z) {
			unsigned block = 0;
			if (kind == 1) block = z == 0 || z == 127 ? 7 : z == 8 ? 11 : z == 12 ? 88 : z == 120 ? 89 : 87;
			else if (kind == 2) block = z == 64 ? 2 : z < 64 ? 1 : 0;
			else block = z == 0 ? 7 : z == 64 || (z < 56 && z % 8 >= 6) ? 1 : 0;
			base.blocks()[(x * 16 + y) * 128 + z] = block;
		}
		base.refreshHeight();
	}

	static void fillChunk(ServerChunk &chunk, const HeightmapChunk &base, bool protectedBlocks) {
		for (int z = 0; z < 32; ++z) for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) {
			int worldZ = chunk.z() * 32 + z;
			u16 block = 0;
			switch (base.blockAt(x, y, worldZ)) {
				case 1: block = id("default:stone"); break;
				case 2: block = id("default:grass"); break;
				case 7: block = id("default:bedrock"); break;
				case 11: block = id("default:lava"); break;
				case 87: block = id("default:netherrack"); break;
				case 88: block = id("default:soul_sand"); break;
				case 89: block = id("default:glowstone"); break;
			}
			if (protectedBlocks && (x + y + worldZ) % 17 == 0)
				block = id(block ? "default:copper_ore" : "default:nether_bricks");
			chunk.setBlockRaw(x, y, z, block);
		}
	}

public:
	void testNetherVeinsPreserveMaterialsAndChunkOrderAcrossNegativeBorders() {
		Registry registry; definitions(registry);
		PlayerList players; Dimension dimension(1, "test:nether", "nether");
		ServerWorld forward(players, dimension, 42), backward(players, dimension, 42), changed(players, dimension, 43);
		std::vector<Vector3i> positions;
		std::vector<std::uint64_t> hashes;
		unsigned basalt = 0, quartz = 0, border = 0, different = 0;
		for (int cy = -2; cy < 2; ++cy) for (int cx = -2; cx < 2; ++cx) {
			auto &base = forward.heightmap().getOrCreateChunk(cx, cy); prepareColumn(base, 1);
			for (int cz = 0; cz < 4; ++cz) {
				auto &chunk = forward.getOrCreateChunk(cx, cy, cz); fillChunk(chunk, base, true);
				std::array<u16, 8192> before{};
				for (int z = 0; z < 32; ++z) for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x)
					before[(z * 16 + y) * 16 + x] = chunk.getBlock(x, y, z);
				decorateExpansionTerrain(chunk, forward.heightmap(), 1, 42);
				for (int z = 0; z < 32; ++z) for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) {
					u16 old = before[(z * 16 + y) * 16 + x], block = chunk.getBlock(x, y, z);
					if (old != id("default:netherrack")) TS_ASSERT_EQUALS(block, old);
					if (block == id("default:basalt")) ++basalt;
					if (block == id("default:quartz_ore")) ++quartz;
					if (block != old && (x == 0 || x == 15 || y == 0 || y == 15 || z == 0 || z == 31)) ++border;
				}
				positions.push_back({cx, cy, cz}); hashes.push_back(hash(chunk));
			}
		}
		TS_ASSERT(basalt > 0); TS_ASSERT(quartz > 0); TS_ASSERT(border > 0);
		TS_ASSERT(basalt + quartz < positions.size() * 8192 / 25);
		for (std::size_t index = positions.size(); index-- > 0;) {
			const auto &p = positions[index];
			auto &base = backward.heightmap().getOrCreateChunk(p.x, p.y); prepareColumn(base, 1);
			auto &chunk = backward.getOrCreateChunk(p.x, p.y, p.z); fillChunk(chunk, base, true);
			decorateExpansionTerrain(chunk, backward.heightmap(), 1, 42);
			TS_ASSERT_EQUALS(hash(chunk), hashes[index]);
			auto &otherBase = changed.heightmap().getOrCreateChunk(p.x, p.y); prepareColumn(otherBase, 1);
			auto &other = changed.getOrCreateChunk(p.x, p.y, p.z); fillChunk(other, otherBase, true);
			decorateExpansionTerrain(other, changed.heightmap(), 1, 43);
			if (hash(other) != hashes[index]) ++different;
		}
		TS_ASSERT(different > positions.size() / 2);
		for (unsigned index = 0; index < positions.size(); ++index) {
			const auto &p = positions[index];
			TS_ASSERT_EQUALS(hash(*static_cast<ServerChunk *>(forward.getChunk(p.x, p.y, p.z))), hashes[index]);
		}
	}

	void testCaveDetailsKeepGeometryOresSupportsAndDimLighting() {
		Registry registry; definitions(registry);
		PlayerList players; Dimension dimension(0, "test:surface", "surface");
		ServerWorld world(players, dimension, 42);
		std::array<unsigned, 6> found{};
		const u16 details[] = {id("default:cave_moss"), id("default:cave_lichen"), id("default:cave_fungi"),
			id("default:cave_mycena"), id("default:cave_glowworm"), id("default:cave_crystal")};
		for (int cy = -2; cy <= 2; ++cy) for (int cx = -2; cx <= 2; ++cx) {
			auto &base = world.heightmap().getOrCreateChunk(cx, cy); prepareColumn(base, 0);
			auto immutable = base.blocks();
			for (int cz = 1; cz >= 0; --cz) {
				auto &chunk = world.getOrCreateChunk(cx, cy, cz); fillChunk(chunk, base, true);
				decorateExpansionTerrain(chunk, world.heightmap(), 0, 42);
				TS_ASSERT(base.blocks() == immutable);
				for (int z = 0; z < 32; ++z) for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) {
					int worldZ = cz * 32 + z; u16 block = chunk.getBlock(x, y, z);
					if ((x + y + worldZ) % 17 == 0) TS_ASSERT_EQUALS(block, id(base.blockAt(x, y, worldZ) ? "default:copper_ore" : "default:nether_bricks"));
					for (unsigned i = 0; i < found.size(); ++i) if (block == details[i]) {
						++found[i];
						TS_ASSERT(worldZ >= 8 && worldZ <= 54);
						if (i < 2) {
							TS_ASSERT_EQUALS(base.blockAt(x, y, worldZ), 1);
							TS_ASSERT_EQUALS(base.blockAt(x, y, worldZ + 1), 0);
						}
						else {
							TS_ASSERT_EQUALS(base.blockAt(x, y, worldZ), 0);
							TS_ASSERT_EQUALS(base.blockAt(x, y, worldZ - 1), 1);
							TS_ASSERT(!registry.getBlock(block).getState(0).isCollidable());
							TS_ASSERT_EQUALS((cx * 16 + x) % 3, 0);
							TS_ASSERT_EQUALS((cy * 16 + y) % 3, 0);
						}
						if (i >= 3) TS_ASSERT_EQUALS(chunk.lightmap().getTorchlight(x, y, z), i);
					}
				}
			}
		}
		for (unsigned count : found) TS_ASSERT(count > 0);
	}

	void testBasaltStaysBesideLavaAtNineThroughTwelveWithoutCarving() {
		Registry registry; definitions(registry);
		PlayerList players; Dimension dimension(0, "test:surface", "surface");
		ServerWorld world(players, dimension, 42);
		for (int cy = -2; cy <= 1; ++cy) for (int cx = -2; cx <= 1; ++cx) {
			auto &base = world.heightmap().getOrCreateChunk(cx, cy); prepareColumn(base, 2);
			if (cx == 0 && cy == 0) {
				for (int x = 0; x < 3; ++x) for (int y = 0; y < 3; ++y) base.blocks()[(x * 16 + y) * 128 + 9] = 11;
				base.blocks()[(0 * 16 + 0) * 128 + 40] = 11;
			}
		}
		std::array<unsigned, 4> levels{}; unsigned crossed = 0;
		for (int cy = -1; cy <= 0; ++cy) for (int cx = -1; cx <= 0; ++cx) for (int cz = 0; cz < 2; ++cz) {
			auto &base = world.heightmap().getOrCreateChunk(cx, cy);
			auto immutable = base.blocks();
			auto &chunk = world.getOrCreateChunk(cx, cy, cz); fillChunk(chunk, base, true);
			Chunk::DataArray before;
			std::memcpy(before, chunk.data(), sizeof(before));
			decorateExpansionTerrain(chunk, world.heightmap(), 0, 42);
			TS_ASSERT(base.blocks() == immutable);
			for (int z = 0; z < 32; ++z) for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) {
				u16 block = chunk.getBlock(x, y, z);
				if (block != id("default:basalt")) { TS_ASSERT_EQUALS(chunk.getFullBlock(x, y, z), before[z][y][x]); continue; }
				int wz = cz * 32 + z, wx = cx * 16 + x, wy = cy * 16 + y;
				TS_ASSERT(wz >= 9 && wz <= 12);
				TS_ASSERT(std::abs(wx - std::clamp(wx, 0, 2)) + std::abs(wy - std::clamp(wy, 0, 2)) + std::abs(wz - 9) <= 3);
				TS_ASSERT_EQUALS(before[z][y][x], id("default:stone"));
				if (wz >= 9 && wz <= 12) ++levels[wz - 9];
				if (wx < 0 || wy < 0) ++crossed;
			}
		}
		for (auto count : levels) TS_ASSERT(count > 0);
		TS_ASSERT(crossed > 0);
	}

	void testSurfaceAcquisitionRespectsOccupancySoilAndForestBiomes() {
		Registry registry; definitions(registry);
		PlayerList players; Dimension dimension(0, "test:surface", "surface");
		ServerWorld world(players, dimension, 42);
		unsigned crops = 0;
		for (int cy = -8; cy < 8; cy += 2) for (int cx = -8; cx < 8; cx += 2) {
			auto &base = world.heightmap().getOrCreateChunk(cx, cy); prepareColumn(base, 2);
			auto &chunk = world.getOrCreateChunk(cx, cy, 2); fillChunk(chunk, base, true);
			decorateExpansionTerrain(chunk, world.heightmap(), 0, 42);
			for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x) {
				u16 block = chunk.getBlock(x, y, 1);
				TS_ASSERT_DIFFERS(block, id("default:wild_hive"));
				if ((x + y + 65) % 17 == 0) TS_ASSERT_EQUALS(block, id("default:nether_bricks"));
				if (block == id("default:wild_potato") || block == id("default:wild_tomato") || block == id("default:wild_barley")) {
					++crops; TS_ASSERT_EQUALS(chunk.getBlock(x, y, 0), id("default:grass"));
				}
			}
		}
		TS_ASSERT(crops > 0);
	}
};

#endif // EXPANSIONTERRAINTESTS_HPP_
