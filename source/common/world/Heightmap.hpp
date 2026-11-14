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
#ifndef HEIGHTMAP_HPP_
#define HEIGHTMAP_HPP_

#include <cstring>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "common/world/RetroTerrain.hpp"
#include "common/world/RetroNether.hpp"

#include "common/core/EngineConfig.hpp"
#include "common/core/IntTypes.hpp"
#include "common/core/Vector2.hpp"
#include "common/core/Vector3.hpp"

class Heightmap;

class HeightmapChunk {
	public:
		HeightmapChunk(Heightmap &heightmap, s32 x, s32 y)
			: m_heightmap(heightmap), m_x(x), m_y(y) {
				std::memset(m_map, 0, CHUNK_WIDTH * CHUNK_DEPTH * sizeof(s32));
		}

		void generate();
		void populateNether();
		void refreshHeight();
		std::uint8_t blockAt(int x, int y, int z) const { return z < 0 || z >= 128 ? 0 : m_blocks[(x * 16 + y) * 128 + z]; }
		retro::Biome biomeAt(int x, int y) const { return m_climate.biomes[x * 16 + y]; }
		double temperatureAt(int x, int y) const { return m_climate.temperature[x * 16 + y]; }
		retro::Terrain::Blocks &blocks() { return m_blocks; }
		const retro::Terrain::Blocks &blocks() const { return m_blocks; }
		double densityAt(int x, int y, int z) const { return retro::Terrain::sample(m_density, x, y, z); }

		s32 landHeightAt(s8 x, s8 y) const;
		void setLandHeight(s8 x, s8 y, s32 height);

	private:
		Heightmap &m_heightmap;

		s32 m_x = 0;
		s32 m_y = 0;

		s32 m_map[CHUNK_WIDTH * CHUNK_DEPTH];
		std::array<double, 425> m_density{};
		retro::ClimateMap m_climate;
		retro::Terrain::Blocks m_blocks{};
		bool m_populated = false;
};

class Heightmap {
	public:
		Heightmap(s32 seed, bool nether = false);

		HeightmapChunk &getOrCreateChunk(s32 chunkX, s32 chunkY);
		const std::vector<Vector3i> &netherGlowstone(s32 chunkX, s32 chunkY);

		int getHighestBlockAt(s32 blockX, s32 blockY);
		int getHighestChunkAt(s32 blockX, s32 blockY);

		void setSeed(s32 seed);
		void retainChunks(const std::unordered_set<Vector2i> &columns);

		const retro::Terrain &terrain() const { return m_terrain; }
		const retro::NetherTerrain &netherTerrain() const { return m_netherTerrain; }
		bool isNether() const { return m_nether; }

	private:
		std::unordered_map<Vector2i, HeightmapChunk> m_chunks;
		std::unordered_map<Vector2i, std::vector<Vector3i>> m_netherGlowstone;
		retro::Terrain m_terrain;
		retro::NetherTerrain m_netherTerrain;
		bool m_nether;
};

#endif // HEIGHTMAP_HPP_
