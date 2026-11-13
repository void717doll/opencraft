#include "common/world/WorldBounds.hpp"
#include "common/world/BetaTreeShape.hpp"
#include "common/world/TreeVariantShape.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "common/core/Registry.hpp"
#include "common/world/Heightmap.hpp"
#include "common/world/SimpleTreeShape.hpp"
#include "server/world/ExpansionTerrain.hpp"
#include "server/world/LushCaves.hpp"
#include "server/world/ServerChunk.hpp"
#include "server/world/ServerWorld.hpp"

namespace {

std::uint64_t mix(std::uint64_t value) {
	value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
	value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
	return value ^ (value >> 31);
}

std::uint64_t featureHash(s32 seed, int x, int y, int z, std::uint64_t salt) {
	std::uint64_t value = mix(std::uint32_t(seed) ^ salt);
	value = mix(value ^ std::uint64_t(std::uint32_t(x)) * 0x9e3779b185ebca87ULL);
	value = mix(value ^ std::uint64_t(std::uint32_t(y)) * 0xc2b2ae3d27d4eb4fULL);
	return mix(value ^ std::uint64_t(std::uint32_t(z)) * 0x165667b19e3779f9ULL);
}

int cellAt(int coordinate, int size) {
	int result = coordinate / size;
	return coordinate % size < 0 ? result - 1 : result;
}

u16 blockID(const char *name) {
	auto &registry = Registry::getInstance();
	return registry.hasBlock(name) ? registry.getBlockFromStringID(name).id() : 0;
}

int baseBlock(Heightmap &heightmap, int x, int y, int z) {
	int cx = cellAt(x, CHUNK_WIDTH), cy = cellAt(y, CHUNK_DEPTH);
	return heightmap.getOrCreateChunk(cx, cy).blockAt(x - cx * CHUNK_WIDTH, y - cy * CHUNK_DEPTH, z);
}

void placeDecoration(ServerChunk &chunk, int x, int y, int z, u16 id) {
	if (!id) return;
	chunk.setBlockRaw(x, y, z, id);
	const auto &state = Registry::getInstance().getBlock(id).getState(0);
	if (u8 emission = Chunk::lightEmission(state))
		chunk.lightmap().addTorchlight(x, y, z, emission);
}

void netherVeins(ServerChunk &chunk, const HeightmapChunk &base, s32 seed, u16 netherrack,
		u16 replacement, std::uint64_t salt, bool basalt) {
	if (!replacement || !netherrack) return;
	int originX = chunk.x() * CHUNK_WIDTH, originY = chunk.y() * CHUNK_DEPTH;
	int originZ = chunk.z() * CHUNK_HEIGHT;
	// replay neighboring source cells, but only write this chunk's share of a vein.
	for (int cz = cellAt(originZ, 16) - 1; cz <= cellAt(originZ + CHUNK_HEIGHT - 1, 16) + 1; ++cz) {
		for (int cy = chunk.y() - 1; cy <= chunk.y() + 1; ++cy) {
			for (int cx = chunk.x() - 1; cx <= chunk.x() + 1; ++cx) {
				auto hash = featureHash(seed, cx, cy, cz, salt);
				if (basalt ? hash % 10 != 0 : hash % 4 == 0) continue;
				double centerX = cx * 16 + int((hash >> 8) & 15) + 0.5;
				double centerY = cy * 16 + int((hash >> 16) & 15) + 0.5;
				double centerZ = cz * 16 + int((hash >> 24) & 15) + 0.5;
				if (centerZ < (basalt ? 8 : 16) || centerZ > (basalt ? 119 : 112)) continue;
				double radius = basalt ? 2.5 + double((hash >> 32) & 3) * 0.5
					: 1.3 + double((hash >> 32) & 3) * 0.2;
				double height = radius * (basalt ? 0.65 : 0.8);
				int minX = std::max(0, int(std::floor(centerX - radius)) - originX);
				int maxX = std::min(CHUNK_WIDTH - 1, int(std::floor(centerX + radius)) - originX);
				int minY = std::max(0, int(std::floor(centerY - radius)) - originY);
				int maxY = std::min(CHUNK_DEPTH - 1, int(std::floor(centerY + radius)) - originY);
				int minZ = std::max(0, int(std::floor(centerZ - height)) - originZ);
				int maxZ = std::min(CHUNK_HEIGHT - 1, int(std::floor(centerZ + height)) - originZ);
				for (int z = minZ; z <= maxZ; ++z) {
					for (int y = minY; y <= maxY; ++y) {
						for (int x = minX; x <= maxX; ++x) {
							double dx = (originX + x + 0.5 - centerX) / radius;
							double dy = (originY + y + 0.5 - centerY) / radius;
							double dz = (originZ + z + 0.5 - centerZ) / height;
							if (dx * dx + dy * dy + dz * dz > 1) continue;
							if (base.blockAt(x, y, originZ + z) == 87 && chunk.getBlock(x, y, z) == netherrack)
								chunk.setBlockRaw(x, y, z, replacement);
						}
					}
				}
			}
		}
	}
}

void decorateLavaBasalt(ServerChunk &chunk, Heightmap &heightmap, const HeightmapChunk &base, s32 seed) {
	if (chunk.z() != 0) return;
	u16 stone = blockID("default:stone"), basalt = blockID("default:basalt");
	if (!stone || !basalt) return;
	std::array<bool, 4 * CHUNK_WIDTH * CHUNK_DEPTH> nearLava{};
	int originX = chunk.x() * CHUNK_WIDTH, originY = chunk.y() * CHUNK_DEPTH;
	for (int z = 6; z <= 15; ++z) {
		for (int y = -3; y < CHUNK_DEPTH + 3; ++y) {
			for (int x = -3; x < CHUNK_WIDTH + 3; ++x) {
				int kind = baseBlock(heightmap, originX + x, originY + y, z);
				if (kind != 10 && kind != 11) continue;
				for (int bz = std::max(9, z - 3); bz <= std::min(12, z + 3); ++bz) {
					for (int by = std::max(0, y - 3); by <= std::min(CHUNK_DEPTH - 1, y + 3); ++by) {
						for (int bx = std::max(0, x - 3); bx <= std::min(CHUNK_WIDTH - 1, x + 3); ++bx) {
							if (std::abs(bx - x) + std::abs(by - y) + std::abs(bz - z) <= 3)
								nearLava[((bz - 9) * CHUNK_DEPTH + by) * CHUNK_WIDTH + bx] = true;
						}
					}
				}
			}
		}
	}
	for (int z = 9; z <= 12; ++z) {
		for (int y = 0; y < CHUNK_DEPTH; ++y) {
			for (int x = 0; x < CHUNK_WIDTH; ++x) {
				if (!nearLava[((z - 9) * CHUNK_DEPTH + y) * CHUNK_WIDTH + x]) continue;
				if (base.blockAt(x, y, z) != 1 || chunk.getBlock(x, y, z) != stone) continue;
				if (featureHash(seed, originX + x, originY + y, z, 0x4c41564142415341ULL) % 3 != 0)
					chunk.setBlockRaw(x, y, z, basalt);
			}
		}
	}
}

void decorateCaves(ServerChunk &chunk, const HeightmapChunk &base, s32 seed) {
	u16 stone = blockID("default:stone"), moss = blockID("default:cave_moss"), lichen = blockID("default:cave_lichen");
	u16 fungi = blockID("default:cave_fungi"), mycena = blockID("default:cave_mycena");
	u16 worms = blockID("default:cave_glowworm"), crystal = blockID("default:cave_crystal");
	if (!stone) return;
	int originX = chunk.x() * CHUNK_WIDTH, originY = chunk.y() * CHUNK_DEPTH;
	int originZ = chunk.z() * CHUNK_HEIGHT;
	for (int z = 0; z < CHUNK_HEIGHT; ++z) {
		int worldZ = originZ + z;
		if (worldZ < 8 || worldZ > 54) continue;
		for (int y = 0; y < CHUNK_DEPTH; ++y) {
			for (int x = 0; x < CHUNK_WIDTH; ++x) {
				if (base.landHeightAt(x, y) - worldZ < 12) continue;
				int worldX = originX + x, worldY = originY + y;
				int kind = base.blockAt(x, y, worldZ);
				if (kind == 1 && chunk.getBlock(x, y, z) == stone) {
					auto patch = featureHash(seed, cellAt(worldX, 8), cellAt(worldY, 8), cellAt(worldZ, 8), 0x4341564550415443ULL);
					if (patch % 9 != 0) continue;
					if (featureHash(seed, worldX, worldY, worldZ, 0x4341564545444745ULL) % 5 == 0) continue;
					u16 covering = (patch >> 8) & 1 ? moss : lichen;
					if (covering && base.blockAt(x, y, worldZ + 1) == 0
					 && (z == CHUNK_HEIGHT - 1 || !chunk.getBlock(x, y, z + 1)))
						chunk.setBlockRaw(x, y, z, covering);
				}
				else if (kind == 0 && !chunk.getBlock(x, y, z)) {
					// immutable support checks also work when the vertical neighbor is not loaded.
					bool floor = base.blockAt(x, y, worldZ - 1) == 1;
					if (z > 0 && !Registry::getInstance().getBlock(chunk.getBlock(x, y, z - 1)).getState(0).isCollidable()) floor = false;
					// two clear columns between decorations, including across chunk borders.
					if (floor && worldX % 3 == 0 && worldY % 3 == 0) {
						if (crystal && worldZ <= 28 && featureHash(seed, worldX, worldY, worldZ, 0x4352595354414cULL) % 64 == 0)
							placeDecoration(chunk, x, y, z, crystal);
						else if (mycena && worldZ <= 44 && featureHash(seed, worldX, worldY, worldZ, 0x4d5943454e41ULL) % 24 == 0)
							placeDecoration(chunk, x, y, z, mycena);
						else if (fungi && featureHash(seed, worldX, worldY, worldZ, 0x46554e4749ULL) % 12 == 0)
							placeDecoration(chunk, x, y, z, fungi);
						else if (worms && worldZ <= 48 && featureHash(seed, worldX, worldY, worldZ, 0x474c4f57574f524dULL) % 32 == 0)
							placeDecoration(chunk, x, y, z, worms);
					}
				}
			}
		}
	}
}

bool cropBiome(retro::Biome biome, unsigned crop) {
	if (crop == 0)
		return biome == retro::Biome::Forest || biome == retro::Biome::SeasonalForest
			|| biome == retro::Biome::Taiga || biome == retro::Biome::Shrubland;
	if (crop == 1)
		return biome == retro::Biome::Plains || biome == retro::Biome::Savanna || biome == retro::Biome::SeasonalForest;
	return biome == retro::Biome::Plains || biome == retro::Biome::Shrubland || biome == retro::Biome::Savanna;
}

void decorateCherryTrees(ServerChunk &chunk, Heightmap &heightmap, s32 seed) {
	if ((chunk.z() + 1) * CHUNK_HEIGHT <= SEALEVEL) return;
	auto &registry = Registry::getInstance();
	if (!registry.hasTree("default:cherry")) return;
	const Tree &tree = registry.getTreeFromStringID("default:cherry");
	int originX = chunk.x() * CHUNK_WIDTH, originY = chunk.y() * CHUNK_DEPTH;
	int originZ = chunk.z() * CHUNK_HEIGHT;
	for (int cy = cellAt(originY, 64) - 1; cy <= cellAt(originY + CHUNK_DEPTH - 1, 64) + 1; ++cy) {
		for (int cx = cellAt(originX, 64) - 1; cx <= cellAt(originX + CHUNK_WIDTH - 1, 64) + 1; ++cx) {
			auto hash = featureHash(seed, cx, cy, 0, 0x434845525259ULL);
			if (hash % 4 != 0) continue;
			int centerX = cx * 64 + 16 + int((hash >> 8) & 31);
			int centerY = cy * 64 + 16 + int((hash >> 16) & 31);
			for (int member = 0; member < 3; ++member) {
				int x = centerX + (member - 1) * 6, y = centerY + (member == 1 ? 3 : -3);
				if (x + 2 < originX || x - 2 >= originX + CHUNK_WIDTH || y + 2 < originY || y - 2 >= originY + CHUNK_DEPTH) continue;
				int bx = cellAt(x, CHUNK_WIDTH), by = cellAt(y, CHUNK_DEPTH);
				const auto &base = heightmap.getOrCreateChunk(bx, by);
				auto biome = base.biomeAt(x - bx * CHUNK_WIDTH, y - by * CHUNK_DEPTH);
				if (biome != retro::Biome::Forest && biome != retro::Biome::Rainforest) continue;
				int z = base.landHeightAt(x - bx * CHUNK_WIDTH, y - by * CHUNK_DEPTH) + 1;
				int height = tree.trunkMinHeight() + int((hash >> (24 + member * 4)) % (tree.trunkMaxHeight() - tree.trunkMinHeight() + 1));
				if (z < SEALEVEL || z + height + 2 >= 128 || baseBlock(heightmap, x, y, z - 1) != 2) continue;
				bool clear = true;
				for (int dz = 0; clear && dz <= height + 2; ++dz)
					for (int dy = -2; clear && dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx)
						if (retro::simpleTreeBlock(dx, dy, dz, height) && baseBlock(heightmap, x + dx, y + dy, z + dz)) { clear = false; break; }
				if (!clear) continue;
				// replay each tree from immutable terrain, writing only this chunk's part.
				for (int dz = 0; dz <= height + 2; ++dz) for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx) {
					unsigned kind = retro::simpleTreeBlock(dx, dy, dz, height);
					int lx = x + dx - originX, ly = y + dy - originY, lz = z + dz - originZ;
					if (!kind || lx < 0 || lx >= CHUNK_WIDTH || ly < 0 || ly >= CHUNK_DEPTH || lz < 0 || lz >= CHUNK_HEIGHT) continue;
					const auto &existing = registry.getBlock(chunk.getBlock(lx, ly, lz));
					if (!existing.id() || existing.stringID() == "default:oak_wood" || existing.getState(0).drawType() == BlockDrawType::Leaves
					 || existing.getState(0).drawType() == BlockDrawType::XShape)
						chunk.setBlockRaw(lx, ly, lz, kind == 1 ? tree.getLogBlockID() : tree.getLeavesBlockID());
				}
			}
		}
	}
}

void decorateSurface(ServerChunk &chunk, const HeightmapChunk &base, s32 seed) {
	const u16 crops[] = {blockID("default:wild_potato"), blockID("default:wild_tomato"), blockID("default:wild_barley")};
	u16 hive = blockID("default:wild_hive"), grass = blockID("default:grass"), dirt = blockID("default:dirt");
	int originX = chunk.x() * CHUNK_WIDTH, originY = chunk.y() * CHUNK_DEPTH;
	int originZ = chunk.z() * CHUNK_HEIGHT;
	for (int y = 0; y < CHUNK_DEPTH; ++y) {
		for (int x = 0; x < CHUNK_WIDTH; ++x) {
			int surface = base.landHeightAt(x, y), z = surface + 1 - originZ;
			if (surface < 63 || surface > 110 || z < 0 || z >= CHUNK_HEIGHT) continue;
			if (base.blockAt(x, y, surface) != 2 || chunk.getBlock(x, y, z)) continue;
			if (z > 0 && chunk.getBlock(x, y, z - 1) != grass && chunk.getBlock(x, y, z - 1) != dirt) continue;
			int worldX = originX + x, worldY = originY + y;
			auto biome = base.biomeAt(x, y);
			int cellX = cellAt(worldX, 32), cellY = cellAt(worldY, 32);
			auto nest = featureHash(seed, cellX, cellY, 0, 0x57494c4448495645ULL);
			if (hive && nest % 2 == 0 && (biome == retro::Biome::Forest || biome == retro::Biome::SeasonalForest || biome == retro::Biome::Rainforest)
				&& worldX == cellX * 32 + int((nest >> 8) & 31) && worldY == cellY * 32 + int((nest >> 16) & 31)) {
				placeDecoration(chunk, x, y, z, hive);
				continue;
			}
			for (int cy = cellY - 1; cy <= cellY + 1; ++cy) {
				for (int cx = cellX - 1; cx <= cellX + 1; ++cx) {
					auto patch = featureHash(seed, cx, cy, 0, 0x57494c4443524f50ULL);
					unsigned crop = unsigned((patch >> 24) % 3);
					if (patch % 2 != 0 || !crops[crop] || !cropBiome(biome, crop)) continue;
					int dx = worldX - (cx * 32 + int((patch >> 8) & 31));
					int dy = worldY - (cy * 32 + int((patch >> 16) & 31));
					if (dx * dx + dy * dy > 16) continue;
					if (featureHash(seed, worldX, worldY, 0, 0x43524f5045444745ULL) % 4 != 0) continue;
					if (!chunk.getBlock(x, y, z)) placeDecoration(chunk, x, y, z, crops[crop]);
				}
			}
		}
	}
}

}

void decorateExpansionTerrain(ServerChunk &chunk, Heightmap &heightmap, u16 dimension, s32 seed) {
	if (chunk.z() < 0 || chunk.z() >= 4) return;
	auto &base = heightmap.getOrCreateChunk(chunk.x(), chunk.y());
	if (dimension == 1) {
		u16 netherrack = blockID("default:netherrack");
		netherVeins(chunk, base, seed, netherrack, blockID("default:basalt"), 0x424153414c54ULL, true);
		netherVeins(chunk, base, seed, netherrack, blockID("default:quartz_ore"), 0x51554152545aULL, false);
	}
	else if (dimension == 0) {
		decorateLavaBasalt(chunk, heightmap, base, seed);
		decorateLushCaves(chunk, heightmap, seed);
		decorateCaves(chunk, base, seed);
		decorateCherryTrees(chunk, heightmap, seed);
		decorateSurface(chunk, base, seed);
	}
}

bool ServerWorld::growTree(int x, int y, int z, const std::string &treeID) {
	auto &registry = Registry::getInstance();
	const BlockState *sapling = getBlockState(x, y, z), *ground = getBlockState(x, y, z - 1);
	if (!registry.hasTree(treeID) || !sapling || !sapling->block().getGroupValue("group:om_sapling")
	 || !ground || !ground->block().getGroupValue("group:om_material_dirt") || getLight(x, y, z + 1) < 9) return false;
	const Tree &tree = registry.getTreeFromStringID(treeID);
	if (!tree.trunkMinHeight() || tree.trunkMaxHeight() < tree.trunkMinHeight() || tree.trunkMaxHeight() > 16) return false;
	int height = tree.trunkMinHeight() + int(featureHash(m_seed, x, y, z, 0x5341504c494e47ULL) % (tree.trunkMaxHeight() - tree.trunkMinHeight() + 1));
	if (z < 1 || z + height + 2 >= worldBounds::height) return false;
	if (treeID == "default:birch" || treeID == "default:spruce" || retro::hasVariantTreeShape(treeID)) {
		retro::Random random(featureHash(m_seed, x, y, z, 0x4245544154524545ULL));
		auto cells = retro::hasVariantTreeShape(treeID) ? retro::variantTreeShape(random, treeID, height)
			: retro::betaTreeShape(random, treeID == "default:spruce");
		for (const auto &cell : cells) {
			auto p = cell.position + Vector3i{x, y, z};
			const auto *chunk = getChunkAtBlockPos(p.x, p.y, p.z);
			const auto *state = getBlockState(p.x, p.y, p.z);
			if (!worldBounds::contains(p.z) || !chunk || !chunk->isInitialized() || !state) return false;
			if (p != Vector3i{x, y, z} && state->block().id() && state->drawType() != BlockDrawType::Leaves) return false;
		}
		for (const auto &cell : cells) {
			auto p = cell.position + Vector3i{x, y, z};
			setBlock(p.x, p.y, p.z, cell.log ? tree.getLogBlockID() : tree.getLeavesBlockID());
		}
		return true;
	}
	std::vector<std::pair<Vector3i, u16>> blocks;
	for (int dz = 0; dz <= height + 2; ++dz) for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx) {
		unsigned kind = retro::simpleTreeBlock(dx, dy, dz, height);
		if (!kind || (kind == 2 && !tree.hasLeaves())) continue;
		const Chunk *chunk = getChunkAtBlockPos(x + dx, y + dy, z + dz);
		const BlockState *state = getBlockState(x + dx, y + dy, z + dz);
		if (!chunk || !chunk->isInitialized() || !state) return false;
		if (state->block().id() && state->drawType() != BlockDrawType::Leaves && (dx || dy || dz)) return false;
		blocks.push_back({{x + dx, y + dy, z + dz}, kind == 1 ? tree.getLogBlockID() : tree.getLeavesBlockID()});
	}
	for (const auto &entry : blocks) setBlock(entry.first.x, entry.first.y, entry.first.z, entry.second);
	return true;
}
