#ifndef REDSTONEPOWER_HPP_
#define REDSTONEPOWER_HPP_

#include <array>
#include <optional>
#include "common/world/World.hpp"
#include "common/world/TorchSupport.hpp"

namespace redstone {
inline constexpr std::array<Vector3i, 6> sides{{{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}}};
inline constexpr std::array<Vector3i, 4> horizontal{{{-1,0,0},{1,0,0},{0,-1,0},{0,1,0}}};

inline const BlockState *stateAt(const World &world, const Vector3i &point) {
	const Chunk *chunk = world.getChunkAtBlockPos(point.x, point.y, point.z);
	return chunk && chunk->isInitialized() ? world.getBlockState(point.x, point.y, point.z) : nullptr;
}
inline bool solid(const BlockState *state) {
	return state && state->block().id() && state->isOpaque() && state->isCollidable()
		&& state->boundingBox().sizeX == 1 && state->boundingBox().sizeY == 1 && state->boundingBox().sizeZ == 1;
}
inline unsigned source(const BlockState *state) {
	return state ? state->block().getGroupValue("group:om_power_source") : 0;
}
inline unsigned rotation(const World &world, const Vector3i &point, const BlockState &state) {
	return state.block().isRotatable() ? state.block().param().getParam(BlockParam::Rotation,
		world.getData(point.x, point.y, point.z)) : 0;
}
inline Vector3i forward(unsigned rotation) {
	static constexpr std::array<Vector3i, 4> directions{{{1,0,0},{0,1,0},{-1,0,0},{0,-1,0}}};
	return directions[rotation & 3];
}
inline Vector3i support(const World &world, const Vector3i &point, const BlockState &state) {
	unsigned kind = source(&state);
	if (kind == 1 || kind == 2 || kind == 5)
		return point + torchSupportOffset(u16(rotation(world, point, state)));
	return point + Vector3i{0,0,-1};
}

template<typename GetState>
std::optional<Vector3i> wireNeighbor(const Vector3i &point, const Vector3i &side, GetState get) {
	Vector3i next = point + side;
	const auto *neighbor = get(next);
	if (source(neighbor) == 7) return next;
	if (solid(neighbor)) {
		if (!solid(get(point + Vector3i{0,0,1})) && source(get(next + Vector3i{0,0,1})) == 7)
			return next + Vector3i{0,0,1};
	} else if (neighbor && source(get(next + Vector3i{0,0,-1})) == 7) return next + Vector3i{0,0,-1};
	return {};
}

template<typename GetState>
std::array<bool,4> wireConnections(const Vector3i &point, GetState get) {
	std::array<bool,4> connected{};
	for (unsigned i = 0; i < horizontal.size(); ++i)
		connected[i] = source(get(point + horizontal[i])) != 0 || wireNeighbor(point, horizontal[i], get).has_value();
	return connected;
}

inline unsigned output(const World &world, const Vector3i &point, const Vector3i &target, bool strong, bool includeWire = true) {
	const auto *state = stateAt(world, point);
	if (!state) return 0;
	unsigned kind = source(state);
	if (kind == 1) {
		if (state->id() != 0 || target == support(world, point, *state)) return 0;
		return !strong || target == point + Vector3i{0,0,1} ? 15 : 0;
	}
	if (kind == 6)
		return (state->id() & 4) && target == point + forward(rotation(world, point, *state)) ? 15 : 0;
	if (kind == 7) {
		if (!includeWire || !state->id()) return 0;
		if (target == point + Vector3i{0,0,-1}) return std::min<unsigned>(15, state->id());
		if (strong || target.z != point.z) return 0;
		auto connected = wireConnections(point, [&](Vector3i p) { return stateAt(world, p); });
		bool x = connected[0] || connected[1], y = connected[2] || connected[3];
		if ((!x && !y) || (target.x != point.x && !y) || (target.y != point.y && !x)) return std::min<unsigned>(15, state->id());
		return 0;
	}
	bool enabled = (kind == 2 || kind == 3 || kind == 4 || kind == 5) && state->id() == 1;
	if (state->block().getGroupValue("group:om_rail_type") == 3) enabled = (state->id() & 8) != 0;
	return enabled && (!strong || target == support(world, point, *state)) ? 15 : 0;
}

inline unsigned strongPower(const World &world, const Vector3i &point, bool includeWire = true) {
	unsigned strength = 0;
	for (auto side : sides) {
		strength = std::max(strength, output(world, point + side, point, true, includeWire));
		if (strength == 15) break;
	}
	return strength;
}
inline unsigned inputFrom(const World &world, const Vector3i &point, const Vector3i &neighbor, bool includeWire = true) {
	unsigned strength = output(world, neighbor, point, false, includeWire);
	if (strength < 15 && solid(stateAt(world, neighbor))) strength = std::max(strength, strongPower(world, neighbor, includeWire));
	return strength;
}
inline unsigned power(const World &world, const Vector3i &point, bool includeWire = true) {
	unsigned strength = 0;
	for (auto side : sides) {
		strength = std::max(strength, inputFrom(world, point, point + side, includeWire));
		if (strength == 15) break;
	}
	return strength;
}
}
#endif
