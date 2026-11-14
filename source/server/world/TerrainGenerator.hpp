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
#ifndef TERRAINGENERATOR_HPP_
#define TERRAINGENERATOR_HPP_

#include <random.hpp>

#include <sol/sol.hpp>

#include "common/core/IntTypes.hpp"
#include "common/world/Heightmap.hpp"
#include "common/world/ModernMineshaft.hpp"
#include "server/world/TerrainBiomeSampler.hpp"

using Random_t = effolkronium::random_local;

class Biome;
class Dimension;
class ServerChunk;

class TerrainGenerator {
	public:
		TerrainGenerator(Heightmap &heightmap, const Dimension &dimension, s32 seed);

		void generate(ServerChunk &chunk);
		bool generateStep(ServerChunk &chunk, unsigned step);

		void setSeed(s32 seed) { m_seed = seed; m_biomeSampler.setSeed(seed); }
		u16 generationRevision() const { return m_generationRevision; }
		void setGenerationRevision(u16 revision) { m_generationRevision = revision; }

	private:
		void fastNoiseGeneration(ServerChunk &chunk, int row = -1);

		bool tryPlaceTree(ServerChunk &chunk, int x, int y, int z, const Biome &biome, Random_t &rand) const;
		bool tryPlaceFlora(ServerChunk &chunk, int x, int y, int z, const Biome &biome, Random_t &rand) const;

		void generateOres(ServerChunk &chunk, int x, int y, int z, const Biome &biome, Random_t &rand) const;

		void randomWalkOrePlace(ServerChunk &chunk, int x, int y, int z, Random_t &rand, u16 oreBlock, u16 deepBlock, int size) const;
		void oreFloodFill(ServerChunk &chunk, int x, int y, int z, u16 toReplace, u16 replaceWith, int depth, Random_t &rand) const;

		TerrainBiomeSampler m_biomeSampler;

		Heightmap &m_heightmap;
		modern::MineshaftCache m_mineshafts;

		s32 m_seed = 0;
		u16 m_dimensionID = 0;
		u16 m_generationRevision = 1;
};

#endif // TERRAINGENERATOR_HPP_
