#ifndef BETAPLAYERMOVEMENT_HPP_
#define BETAPLAYERMOVEMENT_HPP_

#include "common/world/WorldCollision.hpp"
#include "common/world/LiquidState.hpp"

namespace retro {

struct MovementEnvironment {
	bool water = false;
	bool lava = false;
	bool ladder = false;
	bool rope = false;
	bool web = false;
	float slipperiness = 0.6f;
};

inline MovementEnvironment movementEnvironment(const World &world, const DoubleBox &box) {
	MovementEnvironment result;
	DoubleBox liquidBox = box;
	liquidBox.z += 0.4;
	liquidBox.sizeZ -= 0.8;
	for (int z = int(std::floor(liquidBox.z)); z < int(std::ceil(liquidBox.z + liquidBox.sizeZ)); ++z)
		for (int y = int(std::floor(box.y)); y < int(std::ceil(box.y + box.sizeY)); ++y)
			for (int x = int(std::floor(box.x)); x < int(std::ceil(box.x + box.sizeX)); ++x) {
				const BlockState *state = world.getBlockState(x, y, z);
				if (!state) continue;
				int kind = liquid::kind(state->block());
				if (!kind) continue;
				const BlockState *above = world.getBlockState(x, y, z + 1);
				bool covered = above && liquid::kind(above->block()) == kind;
				DoubleBox cell{double(x), double(y), double(z), 1, 1,
					liquid::height(state->block(), world.getData(x, y, z), covered)};
				DoubleBox contact = liquidBox;
				if (kind == 2) { contact.x += 0.1; contact.y += 0.1; contact.sizeX -= 0.2; contact.sizeY -= 0.2; }
				if (cell.intersects(contact)) {
					result.water |= kind == 1;
					result.lava |= kind == 2;
				}
			}
	for (int z = int(std::floor(box.z)); z < int(std::ceil(box.z + box.sizeZ)); ++z)
		for (int y = int(std::floor(box.y)); y < int(std::ceil(box.y + box.sizeY)); ++y)
			for (int x = int(std::floor(box.x)); x < int(std::ceil(box.x + box.sizeX)); ++x) {
				const auto *state = world.getBlockState(x, y, z);
				if (state && state->block().hasGroup("group:om_web")) result.web = true;
			}
	int x = int(std::floor(box.x + box.sizeX / 2));
	int y = int(std::floor(box.y + box.sizeY / 2));
	int z = int(std::floor(box.z));
	const BlockState *feet = world.getBlockState(x, y, z);
	result.ladder = feet && feet->block().getGroupValue("group:climbable");
	result.rope = feet && feet->block().hasGroup("group:om_rope");
	const BlockState *below = world.getBlockState(x, y, z - 1);
	if (below && below->block().stringID() == "default:ice") result.slipperiness = 0.98f;
	return result;
}

// motion uses blocks per tick, as in EntityLiving and MovementInputFromOptions.
inline void acceleratePlayer(Vector3d &motion, float forward, float strafe, double yaw,
	bool jump, bool sneak, bool grounded, const MovementEnvironment &environment) {
	if (jump) {
		if (environment.water || environment.lava) motion.z += double(0.04f);
		else if (environment.rope) motion.z = 0.2;
		else if (grounded) motion.z = double(0.42f);
	}
	if (sneak) { forward = float(forward * 0.3); strafe = float(strafe * 0.3); }
	forward *= 0.98f;
	strafe *= 0.98f;
	float friction = grounded ? environment.slipperiness * 0.91f : 0.91f;
	float acceleration = grounded ? 0.1f * (0.16277136f / (friction * friction * friction)) : 0.02f;
	if (environment.water || environment.lava) acceleration = 0.02f;
	float length = std::sqrt(forward * forward + strafe * strafe);
	if (length >= 0.01f) {
		float scale = acceleration / std::max(1.f, length);
		forward *= scale;
		strafe *= scale;
		motion.x += forward * std::cos(yaw) - strafe * std::sin(yaw);
		motion.y += forward * std::sin(yaw) + strafe * std::cos(yaw);
	}
	if (environment.ladder && !environment.water && !environment.lava) {
		motion.x = std::clamp(motion.x, -double(0.15f), double(0.15f));
		motion.y = std::clamp(motion.y, -double(0.15f), double(0.15f));
		motion.z = std::max(motion.z, -0.15);
		if (sneak && motion.z < 0) motion.z = 0;
	}
}

inline void dragPlayer(Vector3d &motion, bool grounded, bool horizontalCollision,
	const MovementEnvironment &environment) {
	if (environment.water || environment.lava) {
		motion *= environment.water ? double(0.8f) : 0.5;
		motion.z -= 0.02;
	} else {
		if (horizontalCollision && environment.ladder) motion.z = 0.2;
		motion.z = (motion.z - 0.08) * double(0.98f);
		float friction = grounded ? environment.slipperiness * 0.91f : 0.91f;
		motion.x *= friction;
		motion.y *= friction;
	}
}

inline double clipPlayerAxis(const DoubleBox &box, const DoubleBox &obstacle, int axis, double offset) {
	double low[] = {box.x, box.y, box.z}, size[] = {box.sizeX, box.sizeY, box.sizeZ};
	double other[] = {obstacle.x, obstacle.y, obstacle.z};
	double extent[] = {obstacle.sizeX, obstacle.sizeY, obstacle.sizeZ};
	for (int i = 0; i < 3; ++i)
		if (i != axis && (low[i] + size[i] <= other[i] || low[i] >= other[i] + extent[i])) return offset;
	if (offset > 0 && low[axis] + size[axis] <= other[axis])
		offset = std::min(offset, other[axis] - low[axis] - size[axis]);
	if (offset < 0 && low[axis] >= other[axis] + extent[axis])
		offset = std::max(offset, other[axis] + extent[axis] - low[axis]);
	return offset;
}

inline Vector3d movePlayerBox(const std::vector<collision::Obstacle> &obstacles, DoubleBox box, Vector3d motion) {
	// beta clips vertical movement, then source x, then source z (oldworld y).
	for (int axis : {2, 0, 1}) {
		for (const auto &obstacle : obstacles)
			collision::component(motion, axis) = clipPlayerAxis(box, obstacle.box, axis, collision::component(motion, axis));
		Vector3d offset{0, 0, 0};
		collision::component(offset, axis) = collision::component(motion, axis);
		box.move(offset);
	}
	return motion;
}

inline Vector3d movePlayer(const World &world, const DoubleBox &box, Vector3d motion, bool grounded, bool sneak) {
	bool loaded;
	auto obstacles = collision::collectObstacles(world, box, motion, 0.5, loaded, false);
	if (!loaded) return {0, 0, 0};
	if (grounded && sneak) {
		auto supported = [&](double x, double y) {
			DoubleBox lowered = box;
			lowered.move(x, y, -1);
			for (const auto &obstacle : obstacles) if (lowered.intersects(obstacle.box)) return true;
			return false;
		};
		auto reduce = [](double value) { return std::abs(value) <= 0.05 ? 0 : value - std::copysign(0.05, value); };
		while (motion.x != 0 && !supported(motion.x, 0)) motion.x = reduce(motion.x);
		while (motion.y != 0 && !supported(0, motion.y)) motion.y = reduce(motion.y);
		while (motion.x != 0 && motion.y != 0 && !supported(motion.x, motion.y)) {
			motion.x = reduce(motion.x); motion.y = reduce(motion.y);
		}
	}
	Vector3d allowed = movePlayerBox(obstacles, box, motion);
	bool landed = motion.z < 0 && allowed.z != motion.z;
	if ((grounded || landed) && (motion.x != allowed.x || motion.y != allowed.y)) {
		Vector3d stepped = movePlayerBox(obstacles, box, {motion.x, motion.y, 0.5});
		DoubleBox raised = box;
		raised.move(stepped);
		Vector3d down = movePlayerBox(obstacles, raised, {0, 0, -0.5});
		stepped.z += down.z;
		if (stepped.x * stepped.x + stepped.y * stepped.y > allowed.x * allowed.x + allowed.y * allowed.y)
			allowed = stepped;
	}
	return allowed;
}

}

#endif // BETAPLAYERMOVEMENT_HPP_
