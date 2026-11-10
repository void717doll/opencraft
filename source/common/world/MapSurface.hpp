#ifndef MAPSURFACE_HPP_
#define MAPSURFACE_HPP_

#include <optional>
#include "common/world/WorldBounds.hpp"
#include "common/world/World.hpp"

namespace navigation {
inline u8 mapColor(const std::string &id) {
	if (id.find("water") != std::string::npos) return 4;
	if (id.find("lava") != std::string::npos) return 9;
	if (id.find("snow") != std::string::npos || id.find("ice") != std::string::npos) return 6;
	if (id.find("sand") != std::string::npos) return 3;
	if (id.find("leaves") != std::string::npos) return 8;
	if (id.find("grass") != std::string::npos || id.find("moss") != std::string::npos) return 1;
	if (id.find("dirt") != std::string::npos || id.find("farmland") != std::string::npos) return 7;
	if (id.find("wood") != std::string::npos || id.find("plank") != std::string::npos || id.find("log") != std::string::npos) return 5;
	if (id.find("netherrack") != std::string::npos) return 10;
	return 2;
}
inline std::optional<u8> sampleSurface(const World &world, int x, int y) {
	for (int z = worldBounds::height - 1; z >= worldBounds::minHeight; --z) {
		const auto *state = world.getBlockState(x, y, z);
		if (!state || !state->block().id()) continue;
		if (!state->isCollidable() && state->drawType() != BlockDrawType::Liquid && state->drawType() != BlockDrawType::Leaves) continue;
		if (z < worldBounds::height - 1) {
			const auto *above = world.getChunkAtBlockPos(x, y, z + 1);
			if (!above || !above->isInitialized()) return {};
			int lx = ((x % CHUNK_WIDTH) + CHUNK_WIDTH) % CHUNK_WIDTH;
			int ly = ((y % CHUNK_DEPTH) + CHUNK_DEPTH) % CHUNK_DEPTH;
			int lz = (z + 1) % CHUNK_HEIGHT;
			if (above->lightmap().getSunlight(lx, ly, lz) < 15) return {};
		}
		return mapColor(state->block().stringID());
	}
	return {};
}
}
#endif
