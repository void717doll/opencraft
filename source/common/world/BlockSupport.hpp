#ifndef BLOCKSUPPORT_HPP_
#define BLOCKSUPPORT_HPP_

#include "common/world/WorldBounds.hpp"
#include "common/world/Chest.hpp"
#include "common/world/LiquidState.hpp"
#include "common/world/TorchSupport.hpp"
#include "common/world/WorldCollision.hpp"

inline const BlockState *loadedSupport(const World &world, const Vector3i &pos) {
	const Chunk *chunk = world.getChunkAtBlockPos(pos.x, pos.y, pos.z);
	return chunk && chunk->isInitialized() ? world.getBlockState(pos.x, pos.y, pos.z) : nullptr;
}

inline bool resolveSlabPlacement(const World &world, Vector3i &pos, const Block &block) {
	if (!block.hasGroup("group:om_slab")) return false;
	const auto *target = loadedSupport(world, pos);
	if (!target) return false;
	if (target->block().id() == block.id() && target->id() == 0) return true;
	if (target->block().id() && target->drawType() != BlockDrawType::Liquid && target->drawType() != BlockDrawType::XShape)
		return false;
	const auto *below = loadedSupport(world, pos + Vector3i{0, 0, -1});
	if (!below || below->block().id() != block.id() || below->id() != 0) return false;
	--pos.z;
	return true;
}

inline bool hasSolidTop(const World &world, const Vector3i &pos) {
	const auto *state = loadedSupport(world, pos);
	if (!state || !state->block().id() || !state->isCollidable()) return false;
	const auto box = collision::blockBox(*state, world.getData(pos.x, pos.y, pos.z), 0, 0, 0);
	// modernmcp permits full opaque blocks and the full top face of upper slabs.
	return box.x == 0 && box.y == 0 && box.sizeX == 1 && box.sizeY == 1 && box.z + box.sizeZ == 1
		&& (state->isOpaque() || box.z > 0);
}

inline bool blockHasSupport(const World &world, const Vector3i &pos, const BlockState &state, u16 data) {
	const auto &block = state.block();
	if (!loadedSupport(world, pos) || !block.placementConstraints().check(world, pos)) return false;
	if (block.hasGroup("group:om_sign")) {
		u16 rotation = block.param().getParam(BlockParam::Rotation, data);
		if (rotation > 3) return false;
		if (state.id() == 0) return hasSolidTop(world, pos + Vector3i{0, 0, -1});
		static constexpr Vector3i anchors[] = {{0,-1,0}, {1,0,0}, {0,1,0}, {-1,0,0}};
		const auto *anchor = loadedSupport(world, pos + anchors[rotation]);
		if (!anchor || !anchor->isCollidable()) return false;
		const auto &box = anchor->boundingBox();
		return box.x == 0 && box.y == 0 && box.z == 0 && box.sizeX == 1 && box.sizeY == 1 && box.sizeZ == 1;
	}
	if (block.hasGroup("group:om_painting")) {
		u16 rotation = block.param().getParam(BlockParam::Rotation, data);
		if (rotation != 8 && rotation != 12 && rotation != 16 && rotation != 20) return false;
		BlockGeometry::initOrientation();
		glm::vec3 direction = BlockGeometry::orientMatrices[rotation] * glm::vec3{0,0,-1};
		Vector3i anchor = pos + Vector3i{int(direction.x), int(direction.y), int(direction.z)};
		const auto *support = loadedSupport(world, anchor);
		if (!support || !support->isCollidable()) return false;
		const auto &box = support->boundingBox();
		return box.x == 0 && box.y == 0 && box.z == 0 && box.sizeX == 1 && box.sizeY == 1 && box.sizeZ == 1;
	}
	if (block.hasGroup("group:om_chest")) return chest::canPlace(world, pos);
	if (block.hasGroup("group:fence")) {
		const auto *floor = loadedSupport(world, pos + Vector3i{0, 0, -1});
		return floor && floor->block().id() && floor->isCollidable();
	}
	if (u16 species = block.getGroupValue("group:om_coral_plant")) {
		const auto *floor = loadedSupport(world, pos + Vector3i{0, 0, -1});
		if (!floor || floor->block().getGroupValue("group:om_coral_species") != species) return false;
		const auto *target = loadedSupport(world, pos);
		if (block.getGroupValue("group:om_waterlogged") && (!target || liquid::kind(target->block()) != 1)) return false;
	}
	if (block.hasGroup("group:om_cave_decoration") && !canPlaceCaveDecoration(world, pos)) return false;
	u16 rotation = block.isRotatable() ? block.param().getParam(BlockParam::Rotation, data) : 0;
	u16 attached = block.getGroupValue("group:attached_node");
	if (attached == 5 || block.hasGroup("group:om_torch") || block.hasGroup("group:om_wall_switch"))
		return canPlaceTorch(world, pos, rotation) && (block.getGroupValue("group:om_wall_switch") != 2 || rotation != 0);
	if (block.hasGroup("group:om_rail")) {
		if (!hasSolidTop(world, pos + Vector3i{0, 0, -1})) return false;
		unsigned shape = block.getGroupValue("group:om_rail_type") ? state.id() & 7 : state.id();
		if (shape >= 2 && shape <= 5) {
			static constexpr Vector3i highEnds[] = {{1,0,0}, {-1,0,0}, {0,-1,0}, {0,1,0}};
			return hasSolidTop(world, pos + highEnds[shape - 2]);
		}
		return true;
	}
	u16 source = block.getGroupValue("group:om_power_source");
	if (source == 6 || source == 7) {
		Vector3i below = pos + Vector3i{0, 0, -1};
		const auto *floor = loadedSupport(world, below);
		return hasSolidTop(world, below) || (source == 7 && floor && floor->block().stringID() == "default:glowstone");
	}
	if (!attached) return true;
	Vector3i offset{0, 0, attached == 4 ? 1 : -1};
	if ((attached == 1 || attached == 2) && block.isRotatable()) {
		BlockGeometry::initOrientation();
		glm::vec3 direction = BlockGeometry::orientMatrices[rotation & 31] * glm::vec3(0, 0, -1);
		offset = {int(std::round(direction.x)), int(std::round(direction.y)), int(std::round(direction.z))};
	}
	const auto *support = loadedSupport(world, pos + offset);
	return support && (support->isCollidable() || (attached == 4
		&& block.hasGroup("group:om_hanging_chain") && support->block().hasGroup("group:om_hanging_chain")));
}

#endif
