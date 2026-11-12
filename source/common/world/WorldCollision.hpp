#ifndef WORLDCOLLISION_HPP_
#define WORLDCOLLISION_HPP_

#include <cmath>

#include "common/core/BlockGeometry.hpp"
#include "common/world/World.hpp"
#include "common/world/LunarchyCollision.hpp"
#include "common/world/StairGeometry.hpp"

namespace collision {

inline DoubleBox blockBox(const BlockState &state, u16 data, int x, int y, int z) {
	DoubleBox box = state.block().hasGroup("group:fence") ? DoubleBox{0, 0, 0, 1, 1, 1.5} : DoubleBox(state.boundingBox());
	if (state.block().isRotatable()) {
		BlockGeometry::initOrientation();
		u16 orientation = state.block().param().getParam(BlockParam::Rotation, data);
		const glm::mat3 &rotation = BlockGeometry::orientMatrices[orientation & 31];
		glm::vec3 a = rotation * glm::vec3(box.x - 0.5, box.y - 0.5, box.z - 0.5);
		glm::vec3 b = rotation * glm::vec3(box.x + box.sizeX - 0.5,
			box.y + box.sizeY - 0.5, box.z + box.sizeZ - 0.5);
		box.reset(std::min(a.x, b.x) + 0.5, std::min(a.y, b.y) + 0.5,
			std::min(a.z, b.z) + 0.5, std::abs(a.x - b.x), std::abs(a.y - b.y), std::abs(a.z - b.z));
	}
	box.move(x, y, z);
	return box;
}

inline bool intersectsBlock(const BlockState &state, u16 data, int x, int y, int z, const DoubleBox &body) {
	if (state.block().hasGroup("group:om_piston_head")) {
		BlockState shaft = state;
		shaft.boundingBox(FloatBox{0.375f,0.375f,-0.25f,0.25f,0.25f,1.f});
		return blockBox(state,data,x,y,z).intersects(body) || blockBox(shaft,data,x,y,z).intersects(body);
	}
	if (!state.block().hasGroup("group:om_stairs")) return blockBox(state, data, x, y, z).intersects(body);
	u16 rotation = state.block().param().getParam(BlockParam::Rotation, data);
	for (const auto &part : retro::stairBoxes(rotation)) {
		DoubleBox box = part;
		box.move(x, y, z);
		if (box.intersects(body)) return true;
	}
	return false;
}

inline double clipAxis(const DoubleBox &box, const DoubleBox &obstacle, int axis, double movement) {
	Vector3d velocity{0, 0, 0};
	component(velocity, axis) = movement;
	double time = 1;
	if (axisAlignedCollision(obstacle, box, velocity, time) < 0)
		return movement;
	return movement * std::max(time, 0.0);
}

inline double rayDistance(const Vector3d &origin, const Vector3d &direction, const DoubleBox &box, double limit) {
	double low[3] = {box.x, box.y, box.z};
	double high[3] = {box.x + box.sizeX, box.y + box.sizeY, box.z + box.sizeZ};
	double start[3] = {origin.x, origin.y, origin.z};
	double ray[3] = {direction.x, direction.y, direction.z};
	double enter = 0, leave = limit;
	for (int axis = 0; axis < 3; ++axis) {
		if (std::abs(ray[axis]) < 1e-9) {
			if (start[axis] < low[axis] || start[axis] > high[axis]) return -1;
			continue;
		}
		double a = (low[axis] - start[axis]) / ray[axis];
		double b = (high[axis] - start[axis]) / ray[axis];
		if (a > b) std::swap(a, b);
		enter = std::max(enter, a);
		leave = std::min(leave, b);
		if (enter > leave) return -1;
	}
	return enter;
}

inline std::vector<Obstacle> collectObstacles(const World &world, const DoubleBox &box,
	const Vector3d &movement, double stepHeight, bool &anyLoaded, bool collideWithObjects = true) {
	std::vector<Obstacle> obstacles;
	anyLoaded = false;
	int minX = int(std::floor(box.x + std::min(0.0, movement.x))) - 1;
	int minY = int(std::floor(box.y + std::min(0.0, movement.y))) - 1;
	int minZ = int(std::floor(box.z + std::min(0.0, movement.z))) - 1;
	int maxX = int(std::floor(box.x + box.sizeX + std::max(0.0, movement.x))) + 1;
	int maxY = int(std::floor(box.y + box.sizeY + std::max(0.0, movement.y))) + 1;
	int maxZ = int(std::floor(box.z + box.sizeZ + std::max(0.0, movement.z) + stepHeight)) + 1;
	for (int z = minZ; z <= maxZ; ++z) {
		for (int y = minY; y <= maxY; ++y) {
			for (int x = minX; x <= maxX; ++x) {
				const Chunk *chunk = world.getChunkAtBlockPos(x, y, z);
				const BlockState *state = chunk && chunk->isInitialized() ? world.getBlockState(x, y, z) : nullptr;
				if (!state) {
					obstacles.push_back({DoubleBox{double(x), double(y), double(z), 1, 1, 1}, 0, 0, true});
					continue;
				}
				anyLoaded = true;
				if (!state->block().id() || !state->isCollidable())
					continue;
				int bouncy = state->block().getGroupValue("group:bouncy");
				if (bouncy >= 32768)
					bouncy -= 65536;
				u16 data = world.getData(x, y, z);
				u16 slippery = state->block().getGroupValue("group:slippery");
				if (state->block().hasGroup("group:om_stairs")) {
					u16 rotation = state->block().param().getParam(BlockParam::Rotation, data);
					for (const auto &part : retro::stairBoxes(rotation)) {
						DoubleBox stair = part;
						stair.move(x, y, z);
						obstacles.push_back({stair, bouncy, slippery, false, false, {x, y, z}});
					}
				}
				else {
					obstacles.push_back({blockBox(*state, data, x, y, z), bouncy, slippery, false, false, {x, y, z}});
					if (state->block().hasGroup("group:om_piston_head")) {
						BlockState shaft = *state;
						shaft.boundingBox(FloatBox{0.375f,0.375f,-0.25f,0.25f,0.25f,1.f});
						obstacles.push_back({blockBox(shaft,data,x,y,z),bouncy,slippery,false,false,{x,y,z}});
					}
				}
			}
		}
	}
	if (collideWithObjects && anyLoaded) {
		DoubleBox broadphase{box.x + std::min(0.0, movement.x) - 1.5,
			box.y + std::min(0.0, movement.y) - 1.5,
			box.z + std::min(0.0, movement.z) - 1.5,
			box.sizeX + std::abs(movement.x) + 3,
			box.sizeY + std::abs(movement.y) + 3,
			box.sizeZ + std::abs(movement.z) + 3 + stepHeight};
		std::vector<DoubleBox> objectBoxes;
		world.appendCollisionBoxes(box, broadphase, objectBoxes);
		for (const auto &objectBox : objectBoxes)
			obstacles.push_back({objectBox, 0, 0, false, false, {0, 0, 0}, true});
	}
	return obstacles;
}

inline Result step(const World &world, const DoubleBox &box, Vector3d velocity,
	Vector3d acceleration, double seconds, double stepHeight = 0, bool collideWithObjects = true) {
	seconds = std::clamp(seconds, 0.0, 2.5);
	Vector3d movement = limitVelocity(velocity + acceleration * (seconds * 0.5)) * seconds;
	bool anyLoaded;
	auto obstacles = collectObstacles(world, box, movement, stepHeight, anyLoaded, collideWithObjects);
	if (!anyLoaded) {
		Result result;
		result.unloaded = true;
		return result;
	}
	return sweep(std::move(obstacles), box, velocity, acceleration, seconds, stepHeight);
}

inline Vector3d move(const World &world, DoubleBox box, Vector3d movement, bool collideWithObjects = true) {
	bool anyLoaded;
	auto obstacles = collectObstacles(world, box, movement, 0, anyLoaded, collideWithObjects);
	if (!anyLoaded)
		return {0, 0, 0};
	// the displacement API clips requested movement without material rebound.
	Vector3d allowed = sweep(std::move(obstacles), box, movement, {0, 0, 0}, 1, 0, false, false).movement;
	for (int axis = 0; axis < 3; ++axis) {
		if (std::abs(component(allowed, axis) - component(movement, axis)) < 1e-10)
			component(allowed, axis) = component(movement, axis);
	}
	return allowed;
}

}

#endif // WORLDCOLLISION_HPP_
