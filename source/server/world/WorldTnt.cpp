#include "common/core/Registry.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/NetworkComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/scene/component/TntComponent.hpp"
#include "common/scene/component/RotationComponent.hpp"
#include "common/world/Dimension.hpp"
#include "common/world/WorldCollision.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerPlayer.hpp"
#include "server/world/ServerWorld.hpp"

bool ServerWorld::primeTnt(const Vector3i &position, u16 fuse) {
	const BlockState *state = getBlockState(position.x, position.y, position.z);
	if (!state || state->block().stringID() != "default:tnt") return false;
	auto &registry = m_scene.registry();
	if (registry.view<TntComponent>().size() >= 32) return false;
	setBlock(position.x, position.y, position.z, 0);
	auto entity = registry.create();
	auto &effect = registry.emplace<TntComponent>(entity);
	effect.lifetime = fuse;
	effect.velocity = {0, 0, 4};
	effect.seed = u32(entity) ^ u32(m_seed);
	registry.emplace<PositionComponent>(entity, position.x + 0.5, position.y + 0.5, position.z + 0.5, m_dimension.id());
	registry.emplace<RotationComponent>(entity);
	registry.emplace<NetworkComponent>(entity, entity);
	auto &cube = registry.emplace<DrawableDef>(entity).addInventoryCube();
	cube.blockID = "default:tnt";
	cube.size = 0.98f;
	cube.origin = {0.49f, 0.49f, 0.49f};
	if (m_server) m_server->sendSound("tnt_ignite", {position.x + 0.5, position.y + 0.5, position.z + 0.5}, m_dimension.id(), 0.6f);
	return true;
}

void ServerWorld::updateTnt() {
	auto &registry = m_scene.registry();
	std::vector<entt::entity> expired;
	registry.view<TntComponent, PositionComponent>().each([&](auto entity, auto &effect, auto &position) {
		Vector3d start{position.x, position.y, position.z};
		const Chunk *chunk = getChunkAtBlockPos(int(std::floor(start.x)), int(std::floor(start.y)), int(std::floor(start.z)));
		if (!chunk || !chunk->isInitialized()) {
			return;
		}
		DoubleBox box{start.x - 0.49, start.y - 0.49, start.z - 0.49, 0.98, 0.98, 0.98};
		auto result = collision::step(*this, box, effect.velocity, {0, 0, -16}, 0.05, 0, false);
		position.x += result.movement.x; position.y += result.movement.y; position.z += result.movement.z;
		double bounce = -(effect.velocity.z - 0.8) * 0.5;
		effect.velocity = result.velocity * 0.98;
		if (result.grounded) {
			effect.velocity.x *= 0.7; effect.velocity.y *= 0.7;
			effect.velocity.z = bounce > 0.5 ? bounce : 0;
		}

		position.isUpdated = true;
		if (++effect.age >= effect.lifetime) expired.push_back(entity);
	});
	for (auto entity : expired) {
		const auto effect = registry.get<TntComponent>(entity);
		const auto position = registry.get<PositionComponent>(entity);
		Vector3d point{position.x, position.y, position.z};
		if (m_server) m_server->sendEntityDespawn(entity, m_dimension.id());
		registry.destroy(entity);
		explodeAt(point, 4, effect.seed, "tnt");
	}
}
