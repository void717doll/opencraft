#include <array>
#include "common/core/Registry.hpp"
#include "common/world/Heightmap.hpp"
#include "common/world/ModernMineshaft.hpp"
#include "server/world/AmethystTerrain.hpp"
#include "server/world/ServerChunk.hpp"

void decorateAmethyst(ServerChunk &chunk, Heightmap &heightmap, s32 seed) {
	if (heightmap.isNether() || chunk.z() < 0 || chunk.z() > 1) return;
	auto &registry = Registry::getInstance();
	if (!registry.hasBlock("default:amethyst_cluster") || !registry.hasBlock("default:stone")) return;
	u16 stone = registry.getBlockFromStringID("default:stone").id();
	u16 amethyst = registry.getBlockFromStringID("default:amethyst_block").id();
	u16 budding = registry.getBlockFromStringID("default:budding_amethyst").id();
	static constexpr const char *stages[] = {
		"default:small_amethyst_bud", "default:medium_amethyst_bud", "default:large_amethyst_bud", "default:amethyst_cluster",
	};
	static const std::array<Vector3i, 6> directions{{{0,0,1},{0,0,-1},{0,1,0},{0,-1,0},{1,0,0},{-1,0,0}}};
	static constexpr u16 rotations[] = {0,4,8,12,20,16};
	auto random = modern::Mineshaft::originRandom(seed ^ 0x414d4554, chunk.x(), chunk.y());
	const auto &base = heightmap.getOrCreateChunk(chunk.x(), chunk.y());
	int originZ = chunk.z() * CHUNK_HEIGHT;
	for (int z = 1; z < CHUNK_HEIGHT - 1; ++z) {
		int worldZ = originZ + z;
		if (worldZ < 8 || worldZ > 48) continue;
		for (int y = 1; y < CHUNK_DEPTH - 1; ++y) for (int x = 1; x < CHUNK_WIDTH - 1; ++x) {
			if (random.nextInt(2048) || chunk.getBlock(x,y,z) || base.blockAt(x,y,worldZ)
			 || base.landHeightAt(s8(x),s8(y)) - worldZ < 12) continue;
			unsigned start = unsigned(random.nextInt(6));
			for (unsigned i = 0; i < 6; ++i) {
				unsigned side = (start + i) % 6;
				Vector3i anchor = Vector3i{x,y,z} - directions[side];
				if (chunk.getBlock(anchor.x,anchor.y,anchor.z) != stone) continue;
				// surface patches decorate existing cave walls without carving new terrain.
				chunk.setBlockRaw(anchor.x,anchor.y,anchor.z,random.nextInt(4) ? amethyst : budding);
				auto &cluster = registry.getBlockFromStringID(stages[random.nextInt(4)]);
				chunk.setBlockRaw(x,y,z,cluster.id());
				chunk.setData(x,y,z,cluster.param().setParam(BlockParam::Rotation,0,rotations[side]));
				break;
			}
		}
	}
}
