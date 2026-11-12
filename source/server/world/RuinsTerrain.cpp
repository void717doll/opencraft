#include <algorithm>
#include "common/core/Registry.hpp"
#include "common/world/Exploration.hpp"
#include "common/world/Heightmap.hpp"
#include "server/world/RuinsTerrain.hpp"
#include "server/world/ServerWorld.hpp"

namespace {
std::optional<Vector3i> location(Heightmap &heightmap, Vector3i point) {
	int surface = 128;
	for (int dx = -4; dx <= 4; ++dx) for (int dy = -9; dy <= 4; ++dy) {
		int x = point.x + dx, y = point.y + dy;
		auto &column = heightmap.getOrCreateChunk((x & -16) / 16, (y & -16) / 16);
		surface = std::min(surface, int(column.landHeightAt(x & 15, y & 15)));
	}
	if (surface < 12) return {};
	point.z = std::min(point.z, surface - 6);
	for (int dx = -5; dx <= 5; ++dx) for (int dy = -10; dy <= 5; ++dy) {
		int x = point.x + dx, y = point.y + dy;
		auto &column = heightmap.getOrCreateChunk((x & -16) / 16, (y & -16) / 16);
		for (int z = point.z; z <= point.z + 5; ++z) {
			u8 block = column.blockAt(x & 15, y & 15, z);
			if (block >= 8 && block <= 11) return {};
		}
	}
	return point;
}
}

std::optional<Vector3i> nearestUndergroundRuin(ServerWorld &world, const Vector3i &point) {
	std::vector<Vector3i> candidates;
	int cx = exploration::cellAt(point.x), cy = exploration::cellAt(point.y);
	for (int x = cx - 3; x <= cx + 3; ++x) for (int y = cy - 3; y <= cy + 3; ++y)
		if (auto candidate = exploration::ruin(world.seed(), x, y)) candidates.push_back(*candidate);
	auto distance = [&](Vector3i p) {
		double x = double(p.x) - point.x, y = double(p.y) - point.y;
		return x*x + y*y;
	};
	std::sort(candidates.begin(), candidates.end(), [&](Vector3i a, Vector3i b) { return distance(a) < distance(b); });
	for (const auto &candidate : candidates) {
		auto result = location(world.heightmap(), candidate);
		if (!result) continue;
		int x = result->x + 2, y = result->y + 2, z = result->z + 1;
		if (const auto *chunk = world.getChunkAtBlockPos(x,y,z); chunk && chunk->isInitialized()) {
			auto *data = chunk->getBlockData(x & 15, y & 15, z & 31);
			if (!data || !data->meta.hasInt("underground_ruin") || data->meta.get<int>("underground_ruin") != 1) continue;
		}
		return result;
	}
	return {};
}

void decorateRuinsTerrain(ServerChunk &chunk, Heightmap &heightmap, s32 seed) {
	if (chunk.isInitialized() || chunk.z() < 0 || chunk.z() > 1) return;
	auto &registry = Registry::getInstance();
	for (const char *id : {"default:chest", "default:cobblestone", "default:cobblestone_mossy"})
		if (!registry.hasBlock(id)) return;
	auto candidate = exploration::ruin(seed, exploration::cellAt(chunk.x()*16), exploration::cellAt(chunk.y()*16));
	if (!candidate) return;
	if (candidate->x + 4 < chunk.x()*16 || candidate->x - 4 >= chunk.x()*16 + 16
	 || candidate->y + 4 < chunk.y()*16 || candidate->y - 9 >= chunk.y()*16 + 16) return;
	auto center = location(heightmap, *candidate);
	if (!center) return;
	u16 stone = registry.getBlockFromStringID("default:cobblestone").id();
	u16 moss = registry.getBlockFromStringID("default:cobblestone_mossy").id();
	u16 chest = registry.getBlockFromStringID("default:chest").id();
	for (int dy = -9; dy <= 4; ++dy) for (int dx = -4; dx <= 4; ++dx) for (int dz = 0; dz <= 4; ++dz) {
		bool corridor = dy < -4;
		if (corridor && (std::abs(dx) > 1 || dz == 0 || dz > 2)) continue;
		Vector3i world = *center + Vector3i{dx, dy, dz};
		int x = world.x - chunk.x()*16, y = world.y - chunk.y()*16, z = world.z - chunk.z()*CHUNK_HEIGHT;
		if (x < 0 || x >= 16 || y < 0 || y >= 16 || z < 0 || z >= CHUNK_HEIGHT || chunk.getBlockData(x,y,z)) continue;
		bool wall = dx == -4 || dx == 4 || dy == -4 || dy == 4;
		bool doorway = dy == -4 && std::abs(dx) <= 1 && dz >= 1 && dz <= 2;
		u16 id = 0;
		if (!corridor && (dz == 0 || dz == 4 || (wall && !doorway)))
			id = (dx*dx + dy*dy + dz + seed) % 3 ? stone : moss;
		if (dx == 2 && dy == 2 && dz == 1) id = chest;
		if (dx == -2 && dy == 2 && dz == 1 && registry.hasBlock("default:torch"))
			id = registry.getBlockFromStringID("default:torch").id();
		chunk.setData(x,y,z,0);
		chunk.setBlockRaw(x,y,z,id);
		if (id == chest) {
			auto *data = chunk.addBlockData(x,y,z,9,3);
			data->meta.setInt("underground_ruin",1);
			for (auto loot : {std::make_pair("default:coal",4), {"default:iron_ingot",2}, {"default:bread",1}, {"default:gold_ingot",1}})
				if (registry.hasItem(loot.first)) data->inventory.addStack(loot.first,u16(loot.second));
			chunk.setModified(true);
		}
	}
}
