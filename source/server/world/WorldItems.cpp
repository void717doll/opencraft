// Luanti item movement adapted from lunarchy/builtin/game/item_entity.lua.
// SPDX-License-Identifier: LGPL-2.1-or-later

#include <unordered_map>

#include "common/core/Registry.hpp"
#include "common/world/Dimension.hpp"
#include "common/scene/component/MobComponent.hpp"
#include "common/scene/component/PhysicalComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/world/WorldCollision.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerWorld.hpp"

void ServerWorld::updateItems() {
	auto &registry = m_scene.registry();
	std::vector<entt::entity> removed;
	std::unordered_map<u32, std::unordered_map<Vector3i, std::vector<entt::entity>>> groups;
	registry.view<ItemStack, PositionComponent, DoubleBox>(entt::exclude<MobComponent>).each(
		[&](auto entity, auto &stack, auto &position, auto &hitbox) {
			const Chunk *chunk = getChunkAtBlockPos(int(std::floor(position.x)), int(std::floor(position.y)), int(std::floor(position.z)));
			if (!chunk || !chunk->isInitialized()) return;
			auto &body = registry.get_or_emplace<PhysicalComponent>(entity);
			if (!stack.amount() || ++body.age >= 18000) { removed.push_back(entity); return; }
			if (body.pickupDelay) --body.pickupDelay;
			if (body.age == 1) hitbox.reset(-0.125, -0.125, -0.125, 0.25, 0.25, 0.25);
			Vector3i current{int(std::floor(position.x)), int(std::floor(position.y)), int(std::floor(position.z))};
			const BlockState *inside = getBlockState(current.x, current.y, current.z);
			DoubleBox currentBox = hitbox;
			currentBox.move(position.x, position.y, position.z);
			bool stuck = inside && inside->block().id() && inside->isCollidable()
				&& collision::intersectsBlock(*inside, getData(current.x, current.y, current.z), current.x, current.y, current.z, currentBox);
			if (body.forceDirection.isZero() && stuck) {
				for (const Vector3i &direction : {Vector3i{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}}) {
					Vector3i neighbor = current + direction;
					const Chunk *other = getChunkAtBlockPos(neighbor.x, neighbor.y, neighbor.z);
					const BlockState *state = getBlockState(neighbor.x, neighbor.y, neighbor.z);
					if (other && other->isInitialized() && state && (!state->block().id() || !state->isCollidable())) {
						body.forceOrigin = current;
						body.forceDirection = direction;
						break;
					}
				}
			}
			if (!body.forceDirection.isZero()) {
				position.x += body.forceDirection.x * 0.15;
				position.y += body.forceDirection.y * 0.15;
				position.z += body.forceDirection.z * 0.15;
				position.isUpdated = true;
				DoubleBox oldCell{double(body.forceOrigin.x), double(body.forceOrigin.y), double(body.forceOrigin.z), 1, 1, 1};
				DoubleBox moved = hitbox;
				moved.move(position.x, position.y, position.z);
				if (moved.intersects(oldCell)) return;
				body.forceDirection = {0, 0, 0};
				body.velocity = {0, 0, 0};
			}
			DoubleBox box = hitbox;
			box.move(position.x, position.y, position.z);
			auto result = collision::step(*this, box, Vector3d(body.velocity), {0, 0, -9.81}, 0.05, 0, false);
			position.x += result.movement.x;
			position.y += result.movement.y;
			position.z += result.movement.z;
			position.isUpdated |= !result.movement.isZero();
			body.velocity = Vector3f(result.velocity);
			if (result.grounded) {
				double friction = result.slippery ? std::min(4.0 / (result.slippery + 4) * 0.05, 1.0) : 1;
				body.velocity.x *= float(1 - friction);
				body.velocity.y *= float(1 - friction);
			}
			Vector3i cell{int(std::floor(position.x)), int(std::floor(position.y)), int(std::floor(position.z))};
			if (stack.amount() >= stack.item().maxStackSize()) return;
			auto &cells = groups[(u32(stack.item().id()) << 16) | stack.damage()];
			bool merged = false;
			for (int z = -1; !merged && z <= 1; ++z)
				for (int y = -1; !merged && y <= 1; ++y)
					for (int x = -1; !merged && x <= 1; ++x) {
						auto it = cells.find(cell + Vector3i{x, y, z});
						if (it == cells.end()) continue;
						for (auto other : it->second) {
							auto &target = registry.get<ItemStack>(other);
							if (unsigned(target.amount()) + stack.amount() > target.item().maxStackSize()) continue;
							const auto &otherPosition = registry.get<PositionComponent>(other);
							Vector3d delta{otherPosition.x - position.x, otherPosition.y - position.y, otherPosition.z - position.z};
							if (delta.length() > 0.6 || !target.sameItem(stack)) continue;
							target.setAmount(target.amount() + stack.amount());
							auto &targetBody = registry.get<PhysicalComponent>(other);
							targetBody.age = 0;
							targetBody.pickupDelay = std::max(targetBody.pickupDelay, body.pickupDelay);
							removed.push_back(entity);
							merged = true;
							break;
						}
					}
			if (!merged) cells[cell].push_back(entity);
		});
	for (auto entity : removed) {
		if (m_server) m_server->sendEntityDespawn(entity, m_dimension.id());
		registry.destroy(entity);
	}
}
