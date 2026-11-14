/*
 * =====================================================================================
 *
 *  OpenMiner
 *
 *  Copyright (C) 2018-2020 Unarelith, Quentin Bazin <openminer@unarelith.net>
 *  Copyright (C) 2019-2020 the OpenMiner contributors (see CONTRIBUTORS.md)
 *
 *  This file is part of OpenMiner.
 *
 *  OpenMiner is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  OpenMiner is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with OpenMiner; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * =====================================================================================
 */
#include "common/core/EngineConfig.hpp"
#include "common/core/Registry.hpp"
#include "common/world/World.hpp"
#include "common/world/Dimension.hpp"
#include "server/world/ServerChunk.hpp"
#include "server/world/ExpansionTerrain.hpp"
#include "server/world/AmethystTerrain.hpp"
#include "server/world/DungeonTerrain.hpp"
#include "server/world/TerrainGenerator.hpp"
#include "server/world/MineshaftTerrain.hpp"
#include "server/world/OceanTerrain.hpp"
#include "server/world/OreTerrain.hpp"
#include "server/world/RuinsTerrain.hpp"
#include "common/world/BetaTreeShape.hpp"
#include "common/world/TreeVariantShape.hpp"

#include <array>

TerrainGenerator::TerrainGenerator(Heightmap &heightmap, const Dimension &dimension, s32 seed)
	: m_biomeSampler(dimension, seed), m_heightmap(heightmap), m_dimensionID(dimension.id())
{
	setSeed(seed);
}

void TerrainGenerator::generate(ServerChunk &chunk) {
	for (unsigned step = 0; !generateStep(chunk,step); ++step) {}
}

bool TerrainGenerator::generateStep(ServerChunk &chunk, unsigned step) {
	if (step < 16) { fastNoiseGeneration(chunk,int(step)); return false; }
	switch (step) {
		case 16: if (m_dimensionID == 0) decorateOreTerrain(chunk,m_seed); break;
		case 17: if (m_generationRevision == 1) decorateExpansionTerrain(chunk,m_heightmap,m_dimensionID,m_seed); break;
		case 18: if (m_generationRevision == 1 && m_dimensionID == 0) decorateDungeonTerrain(chunk,m_heightmap,m_seed); break;
		case 19: if (m_dimensionID == 0) decorateOceanTerrain(chunk,m_heightmap,m_seed); break;
		case 20: if (m_dimensionID == 1) decorateNetherRuins(chunk,m_heightmap,m_seed); break;
		case 21: if (m_dimensionID == 0) decorateMineshaftTerrain(chunk,m_heightmap,m_seed,m_mineshafts); break;
		case 22: if (m_dimensionID == 0) decorateRuinsTerrain(chunk,m_heightmap,m_seed); break;
		case 23: if (m_dimensionID == 0) decorateCacheTerrain(chunk,m_heightmap,m_seed); break;
		case 24: if (m_dimensionID == 0) decorateAmethyst(chunk,m_heightmap,m_seed); return true;
	}
	return false;
}

void TerrainGenerator::fastNoiseGeneration(ServerChunk &chunk, int row) {
	HeightmapChunk &heightmap = m_heightmap.getOrCreateChunk(chunk.x(), chunk.y());
	if (m_dimensionID == 1) heightmap.populateNether();

	auto &rand = chunk.random();
	std::seed_seq seed{u32(m_seed), u32(m_dimensionID), u32(chunk.x()), u32(chunk.y()), u32(chunk.z())};
	std::array<u32, 1> chunkSeed;
	seed.generate(chunkSeed.begin(), chunkSeed.end());
	if (row <= 0) rand.seed(chunkSeed[0]);

	for(int y = row < 0 ? 0 : row; y < (row < 0 ? CHUNK_DEPTH : row+1); ++y) {
		for(s8 x = 0 ; x < CHUNK_WIDTH ; x++) {
			u16 biomeIndex = m_dimensionID == 0 ? m_biomeSampler.getDecorationBiomeIndex(heightmap.biomeAt(x, y), x + chunk.x() * CHUNK_WIDTH, y + chunk.y() * CHUNK_DEPTH)
				: m_biomeSampler.getBiomeIndexAt(x + chunk.x() * CHUNK_WIDTH, y + chunk.y() * CHUNK_DEPTH);
			const Biome &biome = Registry::getInstance().getBiome(biomeIndex);

			// Land height
			double h = heightmap.landHeightAt(x, y);

			for (s8 z = 0; z < CHUNK_HEIGHT; ++z) {
				int worldZ = z + chunk.z() * CHUNK_HEIGHT;
				bool isGeneratingTopBlock = worldZ == h && worldZ >= SEALEVEL;
				if (m_dimensionID == 0 || m_dimensionID == 1) {
					u8 kind = heightmap.blockAt(x, y, worldZ);
					u16 block = 0;
					const char *id = nullptr;
					switch (kind) {
						case 1: block = biome.getDeepBlockID(); break;
						case 2: block = biome.getTopBlockID(); break;
						case 3: block = biome.getGroundBlockID(); break;
						case 7: id = "default:bedrock"; break;
						case 8: case 9: block = biome.getLiquidBlockID(); break;
						case 10: case 11: id = "default:lava"; break;
						case 12: id = "default:sand"; break;
						case 13: id = "default:gravel"; break;
						case 24: id = "default:sandstone"; break;
						case 79: id = "default:ice"; break;
						case 87: id = "default:netherrack"; break;
						case 88: id = "default:soul_sand"; break;
						case 89: id = "default:glowstone"; break;
					}
					if (id) {
						auto &registry = Registry::getInstance();
						block = registry.hasBlock(id) ? registry.getBlockFromStringID(id).id() : biome.getDeepBlockID();
					}
					// preserve decorations previously written by neighboring chunks.
					if (block || !chunk.getBlock(x, y, z)) chunk.setBlockRaw(x, y, z, block);
				}
				else if (heightmap.densityAt(x, y, worldZ) <= 0) {
					if (worldZ < SEALEVEL) chunk.setBlockRaw(x, y, z, biome.getLiquidBlockID());
				}
				else {
					if (isGeneratingTopBlock) chunk.setBlockRaw(x, y, z, biome.getTopBlockID());
					else if (worldZ <= SEALEVEL - 1 && h < SEALEVEL && worldZ > h - 3)
						chunk.setBlockRaw(x, y, z, biome.getBeachBlockID());
					else if (worldZ > h - 3) chunk.setBlockRaw(x, y, z, biome.getGroundBlockID());
					else chunk.setBlockRaw(x, y, z, biome.getDeepBlockID());
				}
				if (m_dimensionID != 0 && chunk.getBlock(x, y, z) == biome.getDeepBlockID()) generateOres(chunk, x, y, z, biome, rand);
				if (isGeneratingTopBlock && chunk.getBlock(x, y, z) == biome.getTopBlockID()) {
					bool placedTree = tryPlaceTree(chunk, x, y, z + 1, biome, rand);
					if (!placedTree) tryPlaceFlora(chunk, x, y, z + 1, biome, rand);
					if (m_dimensionID == 0 && heightmap.temperatureAt(x, y) < 0.5 && !chunk.getBlock(x, y, z + 1)) {
						auto &registry = Registry::getInstance();
						if (registry.hasBlock("default:snow")) chunk.setBlockRaw(x, y, z + 1, registry.getBlockFromStringID("default:snow").id());
					}
				}
			}
		}
	}
}

inline bool TerrainGenerator::tryPlaceTree(ServerChunk &chunk, int x, int y, int z, const Biome &biome, Random_t &rand) const {
	const auto &existing = Registry::getInstance().getBlock(chunk.getBlock(x, y, z)).stringID();
	if (existing == "default:cherry_wood" || existing == "default:cherry_leaves") return true;
	if (chunk.getBlock(x, y, z - 1) == biome.getTopBlockID()) {
		for (const PlacementEntry::Tree &treePlacement : biome.getTrees()) {
			if (!rand.get<bool>(treePlacement.probability))
				continue;

			auto &definitions = Registry::getInstance();
			const Tree &selected = definitions.getTree(treePlacement.treeID);
			bool spruce = selected.stringID() == "default:spruce" || biome.stringID() == "default:taiga";
			bool birch = selected.stringID() == "default:birch" || (biome.stringID() == "default:forest" && rand.get(0, 4) == 0);
			if ((spruce || birch) && definitions.hasTree(spruce ? "default:spruce" : "default:birch")) {
				const auto &tree = definitions.getTreeFromStringID(spruce ? "default:spruce" : "default:birch");
				retro::Random random(rand.get(0, 0x7fffffff));
				auto cells = retro::betaTreeShape(random, spruce);
				for (const auto &cell : cells) {
					auto p = cell.position + Vector3i{x, y, z};
					u16 old = chunk.getBlock(p.x, p.y, p.z);
					if (old && definitions.getBlock(old).getState(0).drawType() != BlockDrawType::Leaves) return false;
				}
				for (const auto &cell : cells) {
					auto p = cell.position + Vector3i{x, y, z};
					chunk.setBlockRaw(p.x, p.y, p.z, cell.log ? tree.getLogBlockID() : tree.getLeavesBlockID());
				}
				return true;
			}
			const Tree &tree = definitions.getTree(treePlacement.treeID);

			int h = rand.get(tree.trunkMinHeight(), tree.trunkMaxHeight());
			if (retro::hasVariantTreeShape(tree.stringID())) {
				retro::Random random(rand.get(0, 0x7fffffff));
				auto cells = retro::variantTreeShape(random, tree.stringID(), h);
				const auto &terrain = m_heightmap.getOrCreateChunk(chunk.x(), chunk.y());
				for (const auto &cell : cells) {
					auto p = cell.position + Vector3i{x, y, z};
					// shortcut: new crowns stay inside their chunk; use neighboring-origin replay for edge-spanning trees.
					if (p.x < 0 || p.x >= CHUNK_WIDTH || p.y < 0 || p.y >= CHUNK_DEPTH || p.z < 0 || p.z >= CHUNK_HEIGHT) return false;
					// later columns have not written their hillside blocks yet.
					if (terrain.blockAt(p.x, p.y, p.z + chunk.z() * CHUNK_HEIGHT)) return false;
					u16 old = chunk.getBlock(p.x, p.y, p.z);
					if (old && definitions.getBlock(old).getState(0).drawType() != BlockDrawType::Leaves) return false;
					if (cell.log && !cell.position.z && terrain.blockAt(p.x, p.y, p.z - 1 + chunk.z() * CHUNK_HEIGHT) != 2) return false;
				}
				for (const auto &cell : cells) {
					auto p = cell.position + Vector3i{x, y, z};
					chunk.setBlockRaw(p.x, p.y, p.z, cell.log ? tree.getLogBlockID() : tree.getLeavesBlockID());
				}
				return true;
			}

			// Trunk
			for (int i = 0; i < h; i++) {
				const auto &existing = Registry::getInstance().getBlock(chunk.getBlock(x, y, z + i)).stringID();
				if (existing != "default:cherry_wood" && existing != "default:cherry_leaves")
					chunk.setBlockRaw(x, y, z + i, tree.getLogBlockID());
			}

			// Leaves
			if (tree.hasLeaves()) {
				for (int iz = -3; iz <= 3; iz++) {
					for (int iy = -3; iy <= 3; iy++) {
						for (int ix = -3; ix <= 3; ix++) {
							u16 block = chunk.getBlock(x + ix, y + iy, z + h + iz);
							if (ix * ix + iy * iy + iz * iz < 8 + rand.get(0, 1) && !block) {
								chunk.setBlockRaw(x + ix, y + iy, z + h + iz, tree.getLeavesBlockID());

								// FIXME: This is a temporary fix for the second part of #41
								chunk.lightmap().setSunlight(x + ix, y + iy, z + h + iz, 0);
							}
						}
					}
				}
			}

			return true;
		}
	}

	return false;
}

inline bool TerrainGenerator::tryPlaceFlora(ServerChunk &chunk, int x, int y, int z, const Biome &biome, Random_t &rand) const {
	for (const PlacementEntry::Flora &flora : biome.getFlora()) {
		if (chunk.getBlock(x, y, z - 1) != flora.spawnsOnBlockID)
			continue;

		if (!rand.get<bool>(flora.probability))
			continue;

		chunk.setBlockRaw(x, y, z, flora.blockID);

		return true;
	}

	return false;
}

inline void TerrainGenerator::generateOres(ServerChunk &chunk, int x, int y, int z, const Biome &biome, Random_t &rand) const {
	// TODO: Like trees, ores should be able to seamlessly cross chunk boundaries.
	// This could be achieved either by setting up a generation pipeline with stages,
	// processing neighboring chunks' ores every time, or generating them with noise.
	for (const PlacementEntry::Ore &ore : biome.getOres()) {
		if (!rand.get<bool>(ore.probability))
			continue;

		if (ore.genType == PlacementEntry::Ore::Gen::RandomWalk)
			randomWalkOrePlace(chunk, x, y, z, rand, ore.blockID, biome.getDeepBlockID(), ore.size);
		else if (ore.genType == PlacementEntry::Ore::Gen::FloodFill)
			oreFloodFill(chunk, x, y, z, biome.getDeepBlockID(), ore.blockID, ore.size, rand);

		break;
	}
}

inline void TerrainGenerator::randomWalkOrePlace(ServerChunk &chunk, int x, int y, int z, Random_t &rand, u16 oreBlock, u16 deepBlock, int size) const {
	if (size <= 0 || chunk.getBlock(x, y, z) != deepBlock)
		return;
	chunk.setBlockRaw(x, y, z, oreBlock);
	int direction = rand.get(1, 6);
	switch(direction) {
		case 1:
			randomWalkOrePlace(chunk, x + 1, y, z, rand, oreBlock, deepBlock, size - 1);
			break;
		case 2:
			randomWalkOrePlace(chunk, x - 1, y, z, rand, oreBlock, deepBlock, size - 1);
			break;
		case 3:
			randomWalkOrePlace(chunk, x, y + 1, z, rand, oreBlock, deepBlock, size - 1);
			break;
		case 4:
			randomWalkOrePlace(chunk, x, y - 1, z, rand, oreBlock, deepBlock, size - 1);
			break;
		case 5:
			randomWalkOrePlace(chunk, x, y, z + 1, rand, oreBlock, deepBlock, size - 1);
			break;
		case 6:
			randomWalkOrePlace(chunk, x, y, z - 1, rand, oreBlock, deepBlock, size - 1);
			break;
	}
}

void TerrainGenerator::oreFloodFill(ServerChunk &chunk, int x, int y, int z, u16 toReplace, u16 replaceWith, int depth, Random_t &rand) const {
	if (depth < 0) return;
	if (chunk.getBlock(x, y, z) == replaceWith) return;
	if (chunk.getBlock(x, y, z) == toReplace)
		chunk.setBlockRaw(x, y, z, replaceWith);

	oreFloodFill(chunk, x + 1, y, z, toReplace, replaceWith, depth - 1, rand);
	oreFloodFill(chunk, x - 1, y, z, toReplace, replaceWith, depth - 1, rand);
	oreFloodFill(chunk, x, y + 1, z, toReplace, replaceWith, depth - 1, rand);
	oreFloodFill(chunk, x, y - 1, z, toReplace, replaceWith, depth - 1, rand);
	oreFloodFill(chunk, x, y, z + 1, toReplace, replaceWith, depth - 1, rand);
	oreFloodFill(chunk, x, y, z - 1, toReplace, replaceWith, depth - 1, rand);

	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x + 1, y + 1, z + 1, toReplace, replaceWith, depth - 1, rand);
	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x + 1, y + 1, z - 1, toReplace, replaceWith, depth - 1, rand);
	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x + 1, y - 1, z + 1, toReplace, replaceWith, depth - 1, rand);
	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x + 1, y - 1, z - 1, toReplace, replaceWith, depth - 1, rand);
	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x - 1, y + 1, z + 1, toReplace, replaceWith, depth - 1, rand);
	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x - 1, y + 1, z - 1, toReplace, replaceWith, depth - 1, rand);
	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x - 1, y - 1, z + 1, toReplace, replaceWith, depth - 1, rand);
	if (rand.get<bool>(1.f / 15.f))
		oreFloodFill(chunk, x - 1, y - 1, z - 1, toReplace, replaceWith, depth - 1, rand);
}
