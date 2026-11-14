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
#include "common/world/Heightmap.hpp"
#include "common/world/RetroCaves.hpp"
#include "common/world/ModernRavines.hpp"
#include "common/world/World.hpp"

void HeightmapChunk::generate() {
	if (m_heightmap.isNether()) {
		m_density = m_heightmap.netherTerrain().density(m_x, m_y);
		m_blocks = m_heightmap.netherTerrain().surface(m_x, m_y, m_density);
	}
	else {
		m_climate = m_heightmap.terrain().climate().sample(m_x * 16, m_y * 16);
		m_density = m_heightmap.terrain().density(m_x, m_y, m_climate);
		m_blocks = m_heightmap.terrain().surface(m_x, m_y, m_climate, m_density);
	}
	retro::Caves::carve(m_heightmap.terrain().seed(), m_x, m_y, m_blocks, m_heightmap.isNether());
	if (!m_heightmap.isNether()) modern::Ravines::carve(m_heightmap.terrain().seed(), m_x, m_y, m_blocks);
	refreshHeight();
}

void HeightmapChunk::refreshHeight() {
	for (int y = 0; y < CHUNK_DEPTH; ++y) {
		for (int x = 0; x < CHUNK_WIDTH; ++x) {
			int height = 127;
			while (height > 0 && (blockAt(x, y, height) == 0 || blockAt(x, y, height) == 9)) --height;
			m_map[x + y * CHUNK_WIDTH] = height;
		}
	}
}

void HeightmapChunk::populateNether() {
	if (m_populated || !m_heightmap.isNether()) return;
	// source anchors and their seven-block reach cover this column and its positive neighbors.
	for (int y = m_y - 1; y <= m_y; ++y) {
		for (int x = m_x - 1; x <= m_x; ++x) {
			for (const auto &position : m_heightmap.netherGlowstone(x, y)) {
				int localX = position.x - m_x * 16, localY = position.y - m_y * 16;
				if (localX >= 0 && localX < 16 && localY >= 0 && localY < 16)
					m_blocks[(localX * 16 + localY) * 128 + position.z] = 89;
			}
		}
	}
	m_populated = true;
}

s32 HeightmapChunk::landHeightAt(s8 x, s8 y) const {
	return m_map[x + y * CHUNK_WIDTH];
}

void HeightmapChunk::setLandHeight(s8 x, s8 y, s32 height) {
	m_map[x + y * CHUNK_WIDTH] = height;
}

Heightmap::Heightmap(s32 seed, bool nether) : m_terrain(seed), m_netherTerrain(seed), m_nether(nether) {}

HeightmapChunk &Heightmap::getOrCreateChunk(s32 chunkX, s32 chunkY) {
	HeightmapChunk *chunk = nullptr;

	auto it = m_chunks.find({chunkX, chunkY});
	if (it == m_chunks.end()) {
		m_chunks.emplace(Vector2i{chunkX, chunkY}, HeightmapChunk{*this, chunkX, chunkY});

		chunk = &m_chunks.at({chunkX, chunkY});
		chunk->generate();
	}
	else
		chunk = &it->second;

	return *chunk;
}

const std::vector<Vector3i> &Heightmap::netherGlowstone(s32 chunkX, s32 chunkY) {
	auto found = m_netherGlowstone.find({chunkX, chunkY});
	if (found != m_netherGlowstone.end()) return found->second;
	std::array<retro::Terrain::Blocks, 4> columns;
	for (int y = 0; y < 2; ++y) {
		for (int x = 0; x < 2; ++x) {
			columns[x + y * 2] = getOrCreateChunk(chunkX + x, chunkY + y).blocks();
			// each population pass reads base terrain so chunk loading order cannot alter clusters.
			for (auto &block : columns[x + y * 2]) if (block == 89) block = 0;
		}
	}
	std::vector<Vector3i> positions;
	auto read = [&](int x, int y, int z) {
		if (z < 0) return 7;
		if (x < 0 || x >= 32 || y < 0 || y >= 32 || z >= 128) return 0;
		return int(columns[x / 16 + (y / 16) * 2][((x % 16) * 16 + y % 16) * 128 + z]);
	};
	auto write = [&](int x, int y, int z) {
		columns[x / 16 + (y / 16) * 2][((x % 16) * 16 + y % 16) * 128 + z] = 89;
		positions.push_back({chunkX * 16 + x, chunkY * 16 + y, z});
	};
	std::uint64_t seed = std::uint64_t(m_terrain.seed())
		+ std::uint64_t(std::int64_t(chunkX)) * 341873128712ULL
		+ std::uint64_t(std::int64_t(chunkY)) * 132897987541ULL;
	retro::Random random(std::int64_t(seed & ((1ULL << 48) - 1)));
	int attempts = random.nextInt(10) + 1;
	attempts = random.nextInt(attempts);
	for (int pass = 0; pass < 2; ++pass) {
		for (int attempt = 0; attempt < (pass == 0 ? attempts : 10); ++attempt) {
			int x = random.nextInt(16) + 8;
			int z = pass == 0 ? random.nextInt(120) + 4 : random.nextInt(128);
			int y = random.nextInt(16) + 8;
			retro::glowstoneCluster(random, x, y, z, read, write);
		}
	}
	return m_netherGlowstone.emplace(Vector2i{chunkX, chunkY}, std::move(positions)).first->second;
}

int Heightmap::getHighestBlockAt(s32 blockX, s32 blockY) {
	s32 chunkX = (blockX & -CHUNK_WIDTH) / CHUNK_WIDTH;
	s32 chunkY = (blockY & -CHUNK_DEPTH) / CHUNK_DEPTH;

	s32 blockZ = getOrCreateChunk(chunkX, chunkY).landHeightAt(
		s8(math::pmod(blockX, CHUNK_WIDTH)),
		s8(math::pmod(blockY, CHUNK_DEPTH))
	);

	return blockZ;
}

int Heightmap::getHighestChunkAt(s32 blockX, s32 blockY) {
	return (getHighestBlockAt(blockX, blockY) & -CHUNK_HEIGHT) / CHUNK_HEIGHT;
}

void Heightmap::retainChunks(const std::unordered_set<Vector2i> &columns) {
	for (auto it=m_chunks.begin(); it!=m_chunks.end();) {
		if (!columns.count(it->first)) it=m_chunks.erase(it); else ++it;
	}
	for (auto it=m_netherGlowstone.begin(); it!=m_netherGlowstone.end();) {
		if (!columns.count(it->first)) it=m_netherGlowstone.erase(it); else ++it;
	}
}

void Heightmap::setSeed(s32 seed) {
	m_chunks.clear();
	m_netherGlowstone.clear();
	m_terrain = retro::Terrain(seed);
	m_netherTerrain = retro::NetherTerrain(seed);
}
