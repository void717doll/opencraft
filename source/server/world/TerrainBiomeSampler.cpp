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
#include "common/core/Registry.hpp"
#include "common/world/Biome.hpp"
#include "server/world/ServerWorld.hpp"
#include "server/world/TerrainBiomeSampler.hpp"

TerrainBiomeSampler::TerrainBiomeSampler(const Dimension &dimension, s32 seed) : m_dimension(dimension), m_climate(seed) {
	for (u8 i = 0; i < biomeParamCount; i++) {
		m_paramNoises.emplace_back();

		m_paramNoises.back().SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
		m_paramNoises.back().SetFractalType(FastNoiseLite::FractalType_FBm);
		m_paramNoises.back().SetFractalOctaves(5);
		m_paramNoises.back().SetFrequency(1 / 800.0f);
		m_paramNoises.back().SetSeed(s32(u32(seed) + i));
	}
}

void TerrainBiomeSampler::setSeed(s32 seed) {
	m_climate = retro::Climate(seed);
	for (std::size_t i = 0; i < m_paramNoises.size(); ++i)
		m_paramNoises[i].SetSeed(s32(u32(seed) + u32(i)));
}

u16 TerrainBiomeSampler::getBiomeIndexAt(s32 x, s32 y) {
	if (m_dimension.id() == 0) {
		auto climate = m_climate.sample(x, y, 1, 1);
		return getDecorationBiomeIndex(climate.biomes[0], x, y);
	}
	// TODO with a lot of biomes, perhaps we want an R-Tree or similar, instead of a long loop.
	// Should also finish solving for analytic blending, or find completely separate solution such as isotropically-modified genlayer
	// If we continue with temp/precip/etc params, need to write a weighted lloyd smoother so biomes becone fairly represented.
	// True temp/precip values can then be re-interpolated out from the Voronoi diagram using a neighborhood figure "kernel".

	u16 decidedBiomeIndex = 0;
	double decidedBiomeDeviation = INFINITY;

	for (auto &biomeID : m_dimension.biomes()) {
		const Biome &biome = Registry::getInstance().getBiomeFromStringID(biomeID);

		double deviation = 0;
		for (int i = 0; i < biomeParamCount; i++) {
			double dp = m_paramNoises[i].GetNoise<double>(x, y) - biome.getParams()[i];
			deviation += dp * dp;
		}

		if (deviation < decidedBiomeDeviation) {
			decidedBiomeDeviation = deviation;
			decidedBiomeIndex = biome.id();
		}
	}

	return decidedBiomeIndex;
}

u16 TerrainBiomeSampler::getBetaBiomeIndex(retro::Biome biome) const {
	std::string id = std::string("default:") + retro::biomeName(biome);
	for (const auto &allowed : m_dimension.biomes()) {
		if (allowed == id) return Registry::getInstance().getBiomeFromStringID(id).id();
	}
	return m_dimension.biomes().empty() ? 0 : Registry::getInstance().getBiomeFromStringID(m_dimension.biomes().front()).id();
}

u16 TerrainBiomeSampler::getDecorationBiomeIndex(retro::Biome biome, s32 x, s32 y) const {
	const char *variant = nullptr;
	float region = 0;
	if (biome == retro::Biome::Forest || biome == retro::Biome::SeasonalForest)
		region = m_paramNoises[0].GetNoise<float>(float(x), float(y));
	if (biome == retro::Biome::Rainforest) variant = "default:jungle";
	else if (biome == retro::Biome::Savanna) variant = "default:acacia_savanna";
	else if (biome == retro::Biome::Forest) {
		if (region < -0.12f) variant = "default:birch_forest";
		else if (region > 0.18f) variant = "default:dark_forest";
	}
	else if (biome == retro::Biome::SeasonalForest)
		variant = region < 0 ? "default:cherry_grove" : "default:dark_forest";
	if (variant) for (const auto &allowed : m_dimension.biomes())
		if (allowed == variant) return Registry::getInstance().getBiomeFromStringID(variant).id();
	return getBetaBiomeIndex(biome);
}
