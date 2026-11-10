#ifndef BETAPISTON_HPP_
#define BETAPISTON_HPP_

#include <cmath>
#include "common/core/BlockGeometry.hpp"
#include "common/world/Player.hpp"

namespace retro {
inline Vector3i pistonDirection(unsigned rotation) {
	BlockGeometry::initOrientation();
	glm::vec3 direction = BlockGeometry::orientMatrices[rotation & 31] * glm::vec3(0, 0, 1);
	return {int(std::round(direction.x)), int(std::round(direction.y)), int(std::round(direction.z))};
}

inline u16 pistonPlacementRotation(const Player &player, const Vector3i &point) {
	double x = player.x() - point.x - 0.5, y = player.y() - point.y - 0.5;
	double eye = player.z() + 1.625 - point.z;
	Vector3i direction;
	if (std::abs(x) < 2 && std::abs(y) < 2 && eye > 2) direction = {0, 0, 1};
	else if (std::abs(x) < 2 && std::abs(y) < 2 && eye < 0) direction = {0, 0, -1};
	else if (std::abs(x) > std::abs(y)) direction = {x > 0 ? 1 : -1, 0, 0};
	else direction = {0, y > 0 ? 1 : -1, 0};
	for (u16 rotation = 0; rotation < 24; rotation += 4)
		if (pistonDirection(rotation) == direction) return rotation;
	return 0;
}
}
#endif
