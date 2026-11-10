#ifndef NAVIGATIONMAP_HPP_
#define NAVIGATIONMAP_HPP_

#include <array>
#include <cmath>
#include <fstream>
#include <filesystem.hpp>
#include "common/world/WorldFiles.hpp"
#include "common/core/IntTypes.hpp"

namespace navigation {
constexpr int mapSize = 128;
constexpr int mapScale = 8;
struct Map {
	s32 x = 0, y = 0;
	u16 dimension = 0;
	std::array<u8, mapSize * mapSize> colors{};
};
inline ghc::filesystem::path mapPath(const std::string &world, u16 id) {
	return worldFiles::directory(world) / "maps" / ("map_" + std::to_string(id) + ".dat");
}
inline bool load(const std::string &world, u16 id, Map &map) {
	std::ifstream file(mapPath(world, id).string(), std::ios::binary);
	char magic[4]{};
	file.read(magic, 4);
	if (std::string(magic, 4) != "OMAP") return false;
	// fixed little-endian header keeps map files portable between package targets.
	auto integer = [&]() { u32 value = 0; for (int i = 0; i < 4; ++i) { int b = file.get(); if (b < 0) { file.setstate(std::ios::failbit); return u32(0); } value |= u32(b) << (i * 8); } return value; };
	map.x = s32(integer()); map.y = s32(integer()); map.dimension = u16(integer());
	file.read(reinterpret_cast<char *>(map.colors.data()), map.colors.size());
	return bool(file);
}
inline bool save(const std::string &world, u16 id, const Map &map) {
	if (world.empty() || !id) return false;
	auto path = mapPath(world, id);
	std::error_code error;
	ghc::filesystem::create_directories(path.parent_path(), error);
	if (error) return false;
	auto temporary = path; temporary += ".tmp";
	std::ofstream file(temporary.string(), std::ios::binary | std::ios::trunc);
	file.write("OMAP", 4);
	auto integer = [&](u32 value) { for (int i = 0; i < 4; ++i) file.put(char((value >> (i * 8)) & 255)); };
	integer(u32(map.x)); integer(u32(map.y)); integer(map.dimension);
	file.write(reinterpret_cast<const char *>(map.colors.data()), map.colors.size());
	file.flush();
	if (!file) return false;
	file.close();
	ghc::filesystem::rename(temporary, path, error);
	return !error;
}
inline u16 create(const std::string &world, double x, double y, u16 dimension) {
	if (world.empty()) return 0;
	for (unsigned id = 1; id <= 65535; ++id) {
		std::error_code error;
		if (!ghc::filesystem::exists(mapPath(world, u16(id)), error)) {
			Map map;
			map.x = s32(std::floor(x / 1024.0) * 1024 + 512);
			map.y = s32(std::floor(y / 1024.0) * 1024 + 512);
			map.dimension = dimension;
			return save(world, u16(id), map) ? u16(id) : 0;
		}
	}
	return 0;
}
}
#endif
