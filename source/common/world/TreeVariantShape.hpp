#ifndef TREEVARIANTSHAPE_HPP_
#define TREEVARIANTSHAPE_HPP_

#include <string>
#include "common/world/BetaTreeShape.hpp"

namespace retro {
inline bool hasVariantTreeShape(const std::string &id) {
	return id == "default:jungle" || id == "default:acacia" || id == "default:dark_oak";
}

inline std::vector<TreeCell> variantTreeShape(Random &random, const std::string &id, int height) {
	std::vector<TreeCell> cells;
	auto crown = [&](int cx, int cy, int z, int radius) {
		for (int y = -radius; y <= radius; ++y) for (int x = -radius; x <= radius; ++x)
			if (std::abs(x) != radius || std::abs(y) != radius)
				cells.push_back({{cx + x, cy + y, z}, false});
	};
	if (id == "default:acacia") {
		int dx = random.nextInt(2) ? 1 : -1, dy = random.nextInt(2) ? 1 : -1;
		crown(dx * 2, 0, height, 2); crown(dx * 2, 0, height + 1, 1);
		crown(0, dy * 2, height - 1, 2);
		for (int z = 0; z < height; ++z) cells.push_back({{z >= height - 2 ? dx : 0, 0, z}, true});
		cells.push_back({{dx * 2, 0, height - 1}, true});
		cells.push_back({{0, dy, height - 2}, true});
		cells.push_back({{0, dy * 2, height - 1}, true});
	}
	else if (id == "default:dark_oak") {
		crown(0, 0, height - 1, 3); crown(1, 1, height, 3); crown(0, 0, height + 1, 2);
		for (int z = 0; z < height; ++z) for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x)
			cells.push_back({{x, y, z}, true});
	}
	else if (id == "default:jungle") {
		crown(0, 0, height - 2, 2); crown(0, 0, height - 1, 3); crown(0, 0, height, 2);
		for (int z = 0; z < height; ++z) cells.push_back({{0, 0, z}, true});
	}
	return cells;
}
}
#endif
