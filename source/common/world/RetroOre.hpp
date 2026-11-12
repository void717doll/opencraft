#ifndef RETROORE_HPP_
#define RETROORE_HPP_

#include <array>
#include <algorithm>
#include <limits>
#include <cmath>
#include "common/world/RetroRandom.hpp"

namespace retro {

inline float oreSine(float angle, bool cosine = false) {
	static const auto table = [] {
		std::array<float, 65536> values{};
		for (unsigned i = 0; i < values.size(); ++i)
			values[i] = float(std::sin(double(i) * 3.141592653589793 * 2 / 65536));
		return values;
	}();
	return table[unsigned(int(angle * 10430.378f + (cosine ? 16384.f : 0.f))) & 65535];
}

struct OreBounds {
	int minX=std::numeric_limits<int>::min(), minY=std::numeric_limits<int>::min(), minZ=std::numeric_limits<int>::min();
	int maxX=std::numeric_limits<int>::max(), maxY=std::numeric_limits<int>::max(), maxZ=std::numeric_limits<int>::max();
};

// WorldGenMinable's ellipsoids, with minecraft y mapped to our z axis.
template<class Place>
void oreVein(Random &random, int x, int y, int z, int size, Place place, const OreBounds &bounds = {}) {
	float angle = random.nextFloat() * float(3.141592653589793);
	float sine = oreSine(angle);
	float cosine = oreSine(angle, true);
	double x0 = float(x + 8) + sine * float(size) / 8.f;
	double x1 = float(x + 8) - sine * float(size) / 8.f;
	double y0 = float(y + 8) + cosine * float(size) / 8.f;
	double y1 = float(y + 8) - cosine * float(size) / 8.f;
	double z0 = z + random.nextInt(3) + 2;
	double z1 = z + random.nextInt(3) + 2;
	for (int step = 0; step <= size; ++step) {
		double cx = x0 + (x1 - x0) * step / size;
		double cy = y0 + (y1 - y0) * step / size;
		double cz = z0 + (z1 - z0) * step / size;
		double width = random.nextDouble() * size / 16;
		double diameter = double(oreSine(float(step) * float(3.141592653589793) / float(size)) + 1.f) * width + 1;
		double radius = diameter / 2;
		int minX=std::max(bounds.minX,int(std::floor(cx-radius))), maxX=std::min(bounds.maxX,int(std::floor(cx+radius)));
		int minY=std::max(bounds.minY,int(std::floor(cy-radius))), maxY=std::min(bounds.maxY,int(std::floor(cy+radius)));
		int minZ=std::max(bounds.minZ,int(std::floor(cz-radius))), maxZ=std::min(bounds.maxZ,int(std::floor(cz+radius)));
		if(minX>maxX || minY>maxY || minZ>maxZ) continue;
		for (int bx=minX;bx<=maxX;++bx)
			for (int bz=minZ;bz<=maxZ;++bz)
				for (int by=minY;by<=maxY;++by) {
					double dx = (bx + 0.5 - cx) / radius, dy = (by + 0.5 - cy) / radius, dz = (bz + 0.5 - cz) / radius;
					if (dx * dx + dz * dz + dy * dy < 1) place(bx, by, bz);
				}
	}
}

}
#endif
