#include <map>

#include "common/core/Registry.hpp"
#include "common/world/Heightmap.hpp"
#include "common/world/RetroDungeon.hpp"
#include "server/world/DungeonTerrain.hpp"
#include "server/world/ServerChunk.hpp"

void decorateDungeonTerrain(ServerChunk &chunk, Heightmap &heightmap, s32 seed) {
	if (chunk.isInitialized() || chunk.hasBeenModified() || chunk.z() < 0 || chunk.z() > 3) return;
	auto &definitions = Registry::getInstance();
	for (const char *id : {"default:chest", "default:mob_spawner", "default:cobblestone", "default:cobblestone_mossy"})
		if (!definitions.hasBlock(id)) return;
	const u16 cobble = definitions.getBlockFromStringID("default:cobblestone").id();
	const u16 mossy = definitions.getBlockFromStringID("default:cobblestone_mossy").id();
	const u16 chest = definitions.getBlockFromStringID("default:chest").id();
	const u16 spawner = definitions.getBlockFromStringID("default:mob_spawner").id();
	std::vector<retro::DungeonRoom> rooms;
	retro::Random worldRandom(seed);
	std::uint64_t multiplierX = std::uint64_t(worldRandom.nextLong() / 2 * 2 + 1);
	std::uint64_t multiplierY = std::uint64_t(worldRandom.nextLong() / 2 * 2 + 1);
	auto base = [&](int x, int y, int z) {
		int cx = (x & -16) / 16, cy = (y & -16) / 16;
		return heightmap.getOrCreateChunk(cx, cy).blockAt(x & 15, y & 15, z);
	};
	// independently replay all source columns whose rooms can intersect this chunk.
	for (int cx = chunk.x() - 1; cx <= chunk.x(); ++cx) for (int cy = chunk.y() - 1; cy <= chunk.y(); ++cy) {
		std::uint64_t populationSeed = (std::uint64_t(std::int64_t(cx)) * multiplierX + std::uint64_t(std::int64_t(cy)) * multiplierY) ^ std::uint64_t(std::int64_t(seed));
		retro::Random random(std::int64_t(populationSeed & ((1ULL << 48) - 1)));
		for (int attempt = 0; attempt < 8; ++attempt) {
			int x = cx * 16 + random.nextInt(16) + 8;
			int z = random.nextInt(128);
			int y = cy * 16 + random.nextInt(16) + 8;
			auto room = retro::makeDungeon(random, {x, y, z}, base);
			if (room) rooms.push_back(std::move(*room));
		}
	}
	struct Cell { u8 block; const retro::DungeonRoom *room; const retro::DungeonChest *chest; };
	std::map<int, Cell> cells;
	for (const auto &room : rooms) for (const auto &cell : room.cells) {
		int x = cell.position.x - chunk.x() * CHUNK_WIDTH, y = cell.position.y - chunk.y() * CHUNK_DEPTH, z = cell.position.z - chunk.z() * CHUNK_HEIGHT;
		if (x < 0 || x >= CHUNK_WIDTH || y < 0 || y >= CHUNK_DEPTH || z < 0 || z >= CHUNK_HEIGHT) continue;
		const retro::DungeonChest *contents = nullptr;
		if (cell.block == 54) for (const auto &candidate : room.chests) if (candidate.position == cell.position) contents = &candidate;
		cells[(z * CHUNK_DEPTH + y) * CHUNK_WIDTH + x] = {cell.block, &room, contents};
	}
	for (const auto &entry : cells) {
		int x = entry.first % CHUNK_WIDTH, y = (entry.first / CHUNK_WIDTH) % CHUNK_DEPTH, z = entry.first / (CHUNK_WIDTH * CHUNK_DEPTH);
		if (chunk.getBlockData(x, y, z)) continue;
		const auto &cell = entry.second;
		u16 id = cell.block == 52 ? spawner : cell.block == 54 ? chest : cell.block == 48 ? mossy : cell.block == 4 ? cobble : 0;
		chunk.setBlockRaw(x, y, z, id); chunk.setData(x, y, z, 0);
		if (cell.block == 54 && cell.chest) {
			auto *data = chunk.addBlockData(x, y, z, 9, 3);
			for (unsigned slot = 0; slot < 27; ++slot) {
				const auto &loot = cell.chest->slots[slot];
				if (loot.count && definitions.hasItem(loot.id)) data->inventory.setStack(slot % 9, slot / 9, loot.id, loot.count);
			}
			if (definitions.hasItem("default:treasure_note")
			 && (u32(seed) ^ u32(chunk.x()*16+x)*31u ^ u32(chunk.y()*16+y)*17u ^ u32(chunk.z()*32+z)) % 6 == 0)
				data->inventory.addStack("default:treasure_note", 1);
			chunk.setModified(true);
		}
		if (cell.block == 52) {
			auto *data = chunk.addBlockData(x, y, z);
			data->meta.setInt("spawner_kind", cell.room->spawner);
			data->meta.setInt("spawner_delay", 20);
			data->meta.setInt("spawner_random", int((u32(seed) ^ u32(cell.room->center.x) * 341873u ^ u32(cell.room->center.y) * 132897u ^ u32(cell.room->center.z)) & 0x7fffffff));
			chunk.setModified(true);
		}
	}
}
