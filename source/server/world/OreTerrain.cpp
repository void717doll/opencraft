#include "common/core/Registry.hpp"
#include "common/world/RetroOre.hpp"
#include "server/world/ServerChunk.hpp"
#include "server/world/OreTerrain.hpp"

void decorateOreTerrain(ServerChunk &chunk, s32 seed) {
	if (chunk.isInitialized() || chunk.z() < 0 || chunk.z() > 4) return;
	auto &definitions = Registry::getInstance();
	if (!definitions.hasBlock("default:stone")) return;
	u16 stone = definitions.getBlockFromStringID("default:stone").id();
	struct Ore { const char *name; int attempts, height, size; bool triangular = false; int minimum = 0; };
	constexpr Ore ores[] = {
		{"dirt", 20, 128, 32}, {"gravel", 10, 128, 32}, {"coal_ore", 20, 128, 16},
		{"iron_ore", 12, 64, 8}, {"copper_ore", 18, 72, 8, false, 8},
		{"gold_ore", 2, 32, 8}, {"redstone_ore", 8, 16, 7},
		{"diamond_ore", 1, 16, 7}, {"lapis_ore", 1, 16, 6, true}
	};
	retro::Random worldRandom(seed);
	std::uint64_t multiplierX = std::uint64_t(worldRandom.nextLong() / 2 * 2 + 1);
	std::uint64_t multiplierY = std::uint64_t(worldRandom.nextLong() / 2 * 2 + 1);
	// replay neighboring source columns and write only this chunk, regardless of load order.
	for (int cx = chunk.x() - 1; cx <= chunk.x(); ++cx) for (int cy = chunk.y() - 1; cy <= chunk.y(); ++cy) {
		std::uint64_t populationSeed = (std::uint64_t(std::int64_t(cx)) * multiplierX
			+ std::uint64_t(std::int64_t(cy)) * multiplierY) ^ std::uint64_t(std::int64_t(seed));
		retro::Random random(std::int64_t(populationSeed & ((1ULL << 48) - 1)));
		for (const auto &ore : ores) {
			std::string name = std::string("default:") + ore.name;
			u16 id = definitions.hasBlock(name) ? definitions.getBlockFromStringID(name).id() : 0;
			for (int attempt = 0; attempt < ore.attempts; ++attempt) {
				int x = cx * 16 + random.nextInt(16);
				int z = ore.minimum + random.nextInt(ore.height);
				if (ore.triangular) z += random.nextInt(ore.height);
				int y = cy * 16 + random.nextInt(16);
				retro::oreVein(random, x, y, z, ore.size, [&](int wx, int wy, int wz) {
					int bx = wx - chunk.x() * CHUNK_WIDTH, by = wy - chunk.y() * CHUNK_DEPTH, bz = wz - chunk.z() * CHUNK_HEIGHT;
					if (id && bx >= 0 && bx < CHUNK_WIDTH && by >= 0 && by < CHUNK_DEPTH && bz >= 0 && bz < CHUNK_HEIGHT
					 && chunk.getBlock(bx, by, bz) == stone) chunk.setBlockRaw(bx, by, bz, id);
				}, chunk.z() * CHUNK_HEIGHT, (chunk.z() + 1) * CHUNK_HEIGHT - 1);
			}
		}
	}
}
