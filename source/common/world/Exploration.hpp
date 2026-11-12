#ifndef EXPLORATION_HPP_
#define EXPLORATION_HPP_

#include <cmath>
#include <fstream>
#include <optional>
#include <string>
#include "common/core/Vector3.hpp"
#include "common/world/RetroRandom.hpp"
#include "common/world/WorldFiles.hpp"

namespace exploration {

inline int cellAt(int value) { return int(std::floor(double(value) / 256)); }

inline std::optional<Vector3i> ruin(s32 seed, int cellX, int cellY) {
	std::uint64_t value = std::uint64_t(std::int64_t(seed))
		^ std::uint64_t(std::int64_t(cellX)) * 341873128712ULL
		^ std::uint64_t(std::int64_t(cellY)) * 132897987541ULL ^ 0x5255494eULL;
	retro::Random random(std::int64_t(value & ((1ULL << 48) - 1)));
	if (random.nextInt(3)) return {};
	return Vector3i{cellX*256 + 32 + random.nextInt(192), cellY*256 + 32 + random.nextInt(192), 16 + random.nextInt(16)};
}

inline std::optional<Vector3i> cache(s32 seed, int cellX, int cellY) {
	std::uint64_t value = std::uint64_t(std::int64_t(seed))
		^ std::uint64_t(std::int64_t(cellX)) * 341873128712ULL
		^ std::uint64_t(std::int64_t(cellY)) * 132897987541ULL ^ 0x4341434845ULL;
	retro::Random random(std::int64_t(value & ((1ULL << 48) - 1)));
	if (random.nextInt(3)) return {};
	return Vector3i{cellX*256 + 32 + random.nextInt(192), cellY*256 + 32 + random.nextInt(192), 0};
}

inline ghc::filesystem::path notePath(const std::string &world, u16 id) {
	return worldFiles::directory(world) / "notes" / ("note_" + std::to_string(id) + ".txt");
}

inline u16 saveNote(const std::string &world, const Vector3i &target, bool cache = false) {
	if (world.empty()) return 0;
	std::error_code error;
	ghc::filesystem::create_directories(worldFiles::directory(world) / "notes", error);
	if (error) return 0;
	for (unsigned id = 1; id <= 65535; ++id) {
		auto path = notePath(world, u16(id));
		if (ghc::filesystem::exists(path)) continue;
		std::ofstream file(path.string());
		file << target.x << ' ' << target.y << ' ' << target.z << (cache ? " cache\n" : " ruin\n");
		file.close();
		return file ? u16(id) : 0;
	}
	return 0;
}

inline std::optional<Vector3i> loadNote(const std::string &world, u16 id) {
	std::ifstream file(notePath(world, id).string());
	Vector3i target;
	if (!(file >> target.x >> target.y >> target.z)) return {};
	std::string kind; file >> kind;
	if (std::abs(double(target.x)) > 1000000000 || std::abs(double(target.y)) > 1000000000
	 || target.z < 6 || target.z > (kind == "cache" ? 124 : 64)) return {};
	return target;
}

inline bool noteIsCache(const std::string &world, u16 id) {
	std::ifstream file(notePath(world,id).string());
	int x,y,z; std::string kind;
	return (file >> x >> y >> z >> kind) && kind == "cache";
}

inline const char *rareName(unsigned kind, u32 roll) {
	static const char *zombies[] = {"Ashen Jack", "Hollow Tom", "Old Cinder", "Gravelhand"};
	static const char *skeletons[] = {"Stonewatch", "The Pale Archer", "Old Flint", "Dustbow"};
	static const char *spiders[] = {"Mossback", "The Cellar Weaver", "Slatefang", "Old Silk"};
	if (kind == 2) return zombies[roll % 4];
	if (kind == 6) return skeletons[roll % 4];
	return spiders[roll % 4];
}
}
#endif
