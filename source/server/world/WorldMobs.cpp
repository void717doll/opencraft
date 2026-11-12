#include <cmath>
#include <random>
#include <unordered_set>

#include <glm/gtc/quaternion.hpp>

#include "common/core/GameTime.hpp"
#include "common/core/Registry.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/MobComponent.hpp"
#include "common/scene/component/NetworkComponent.hpp"
#include "common/scene/component/PhysicalComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/scene/component/RotationComponent.hpp"
#include "common/world/Dimension.hpp"
#include "common/world/Exploration.hpp"
#include "common/scene/component/RareMobComponent.hpp"
#include "common/world/WorldCollision.hpp"
#include "common/world/BetaMining.hpp"
#include "common/world/BetaToolRules.hpp"
#include "common/world/LiquidState.hpp"
#include "common/world/RetroMobSpawn.hpp"
#include "server/core/PlayerList.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerWorld.hpp"

namespace {

using Kind = BetaMobKind;

u32 nextRandom(MobComponent &mob) {
	mob.randomState = mob.randomState * 1664525u + 1013904223u;
	return mob.randomState;
}

double unitRandom(MobComponent &mob) { return double(nextRandom(mob)) / 4294967296.0; }

bool projectile(Kind kind) { return betaMobIsProjectile(kind); }

bool holdingItem(ServerPlayer &player, const std::string &id) {
	int slot = player.heldItemSlot();
	if (slot < 0 || slot >= player.inventory().width()) return false;
	const ItemStack &held = player.heldItemStack();
	return held.amount() && held.stringID() == id;
}

bool liquidAt(const World &world, Vector3d point) {
	int x = int(std::floor(point.x)), y = int(std::floor(point.y)), z = int(std::floor(point.z));
	const BlockState *state = world.getBlockState(x, y, z);
	if (!state || state->drawType() != BlockDrawType::Liquid) return false;
	const BlockState *above = world.getBlockState(x, y, z + 1);
	bool covered = above && liquid::kind(above->block()) == liquid::kind(state->block());
	return point.z - z < liquid::height(state->block(), world.getData(x, y, z), covered);
}

bool waterAt(const World &world, Vector3d point) {
	const Chunk *chunk = world.getChunkAtBlockPos(int(std::floor(point.x)), int(std::floor(point.y)), int(std::floor(point.z)));
	if (!chunk || !chunk->isInitialized()) return false;
	const BlockState *state = world.getBlockState(int(std::floor(point.x)), int(std::floor(point.y)), int(std::floor(point.z)));
	return state && liquid::kind(state->block()) == 1 && liquidAt(world, point);
}

bool lavaInBox(const World &world, const DoubleBox &box) {
	for (int z = int(std::floor(box.z)); z <= int(std::floor(box.z + box.sizeZ - 0.001)); ++z)
		for (int y = int(std::floor(box.y)); y <= int(std::floor(box.y + box.sizeY - 0.001)); ++y)
			for (int x = int(std::floor(box.x)); x <= int(std::floor(box.x + box.sizeX - 0.001)); ++x) {
				const auto *state = world.getBlockState(x, y, z);
				if (!state || liquid::kind(state->block()) != 2) continue;
				const auto *above = world.getBlockState(x, y, z + 1);
				bool covered = above && liquid::kind(above->block()) == 2;
				if (box.z < z + liquid::height(state->block(), world.getData(x, y, z), covered)) return true;
			}
	return false;
}

bool lavaAhead(const World &world, DoubleBox box, Vector3d movement) {
	double length = std::hypot(movement.x, movement.y);
	if (length < 0.0001) return false;
	box.move(movement.x * (1 + 0.4 / length), movement.y * (1 + 0.4 / length), 0);
	if (lavaInBox(world, box)) return true;
	// check the landing surface too, but allow solid bridges over lava.
	for (int y = int(std::floor(box.y)); y <= int(std::floor(box.y + box.sizeY - 0.001)); ++y)
		for (int x = int(std::floor(box.x)); x <= int(std::floor(box.x + box.sizeX - 0.001)); ++x)
			for (int z = int(std::floor(box.z)); z >= int(std::floor(box.z)) - 3; --z) {
				const auto *state = world.getBlockState(x, y, z);
				if (!state) break;
				if (liquid::kind(state->block()) == 2) return true;
				if (state->isCollidable() || liquid::kind(state->block()) == 1) break;
			}
	return false;
}

void avoidLava(const World &world, const DoubleBox &box, Vector3d &movement, MobComponent &mob) {
	// modernmcp rejects lava paths, except when the mob is already in lava and needs to escape.
	if (lavaInBox(world, box) || !lavaAhead(world, box, movement)) return;
	double speed = std::hypot(movement.x, movement.y);
	double heading = std::atan2(movement.y, movement.x);
	for (double turn : {M_PI / 4, -M_PI / 4, M_PI / 2, -M_PI / 2}) {
		Vector3d candidate{std::cos(heading + turn) * speed, std::sin(heading + turn) * speed, movement.z};
		if (lavaAhead(world, box, candidate)) continue;
		auto step = collision::move(world, box, candidate);
		if (std::hypot(step.x, step.y) < speed * 0.5) continue;
		movement = candidate;
		mob.yaw = float(heading + turn);
		return;
	}
	movement.x = movement.y = 0;
}

bool segmentBox(Vector3d start, Vector3d delta, const DoubleBox &box, double &distance) {
	double hit = collision::rayDistance(start, delta, box, distance);
	if (hit < 0) return false;
	distance = hit;
	return true;
}

bool clearPath(const World &world, Vector3d from, Vector3d to) {
	Vector3d delta = to - from;
	int steps = std::max(1, int(std::ceil(delta.length() * 8)));
	for (int i = 1; i < steps; ++i) {
		Vector3d point = from + delta * (double(i) / steps);
		int x = int(std::floor(point.x)), y = int(std::floor(point.y)), z = int(std::floor(point.z));
		const BlockState *state = world.getBlockState(x, y, z);
		if (!state) return false;
		if (!state->block().id() || !state->isCollidable()) continue;
		DoubleBox box = collision::blockBox(*state, world.getData(x, y, z), x, y, z);
		if (point.x >= box.x && point.x <= box.x + box.sizeX && point.y >= box.y && point.y <= box.y + box.sizeY
		 && point.z >= box.z && point.z <= box.z + box.sizeZ) return false;
	}
	return true;
}

BetaMobComponent &traitsFor(entt::registry &registry, entt::entity entity) {
	auto &traits = registry.get_or_emplace<BetaMobComponent>(entity);
	if (traits.kind == Kind::Unknown) {
		const auto *id = registry.try_get<std::string>(entity);
		if (id) traits.kind = betaMobKind(*id);
		if (traits.kind == Kind::Slime) traits.size = 2;
	}
	return traits;
}

void setSlimeSize(entt::registry &registry, entt::entity entity, u8 size, bool resetHealth) {
	auto &mob = registry.get<MobComponent>(entity);
	auto &traits = traitsFor(registry, entity);
	traits.size = size;
	registry.get<DoubleBox>(entity).reset(-0.3 * size, -0.3 * size, 0, 0.6 * size, 0.6 * size, 0.6 * size);
	if (resetHealth) mob.health = u16(size * size);
	mob.attackStrength = size;
	if (auto *visual = registry.try_get<DrawableDef>(entity)) {
		visual->setMobModel(u8(Kind::Slime), float(size));
		visual->isUpdated = true;
	}
}

void syncAppearance(entt::registry &registry, entt::entity entity) {
	auto *visual = registry.try_get<DrawableDef>(entity);
	if (!visual) return;
	const auto &traits = registry.get<BetaMobComponent>(entity);
	u8 appearance = 0;
	if (traits.kind == Kind::Sheep) appearance = u8((traits.color & 15) | (traits.sheared ? 16 : 0));
	if (traits.kind == Kind::Wolf) {
		appearance = u8((traits.anger ? 1 : 0) | (traits.tamed ? 2 : 0) | (traits.sitting ? 4 : 0));
		appearance |= u8(std::min<u16>(20, registry.get<MobComponent>(entity).health) << 3);
	}
	if (traits.kind == Kind::Axolotl || traits.kind == Kind::Rabbit) appearance = traits.color;
	if (traits.kind == Kind::SnowGolem) appearance = traits.sheared ? 1 : 0;
	if (traits.kind == Kind::Pig) appearance = traits.saddled ? 1 : 0;
	if (traits.kind == Kind::Ghast) appearance = traits.attackCharge > 10 ? 1 : 0;
	if (traits.kind == Kind::Bat || traits.kind == Kind::Ocelot) appearance = traits.sitting ? 1 : 0;
	if (registry.all_of<RareMobComponent>(entity)) appearance |= 128;
	if (visual->mobAppearance() != appearance) {
		visual->setMobAppearance(appearance);
		visual->isUpdated = true;
	}
}

entt::entity dropItem(ServerWorld &world, Vector3d position, const std::string &id, u16 count, u16 damage = 0, bool limited = true) {
	if (!count || !Registry::getInstance().hasItem(id)) return entt::null;
	auto &definitions = Registry::getInstance();
	entt::entity model = definitions.getEntityFromStringID("default:item_drop");
	if (model == entt::null) return entt::null;
	auto &scene = world.scene();
	if (limited && scene.registry().view<ItemStack>().size() >= 128) return entt::null;
	entt::entity entity = scene.createEntityFromModel(definitions.entityRegistry(), model);
	auto &registry = scene.registry();
	registry.emplace<PositionComponent>(entity, position.x, position.y, position.z + 0.3, world.dimension().id());
	registry.emplace<NetworkComponent>(entity, entity);
	registry.emplace<std::string>(entity, "default:item_drop");
	registry.emplace<ItemStack>(entity, id, count, damage);
	if (auto *drawable = registry.try_get<DrawableDef>(entity)) drawable->getInventoryCubeDef().blockID = id;
	return entity;
}

struct Shot { Vector3d position, direction; Kind kind; entt::entity shooter; };
struct Blast { Vector3d position; float strength; u32 seed; std::string source; };

bool spawnShot(ServerWorld &world, const Shot &shot) {
	auto &registry = world.scene().registry();
	std::size_t count = 0;
	registry.view<BetaMobComponent>().each([&](auto, auto &traits) { if (projectile(traits.kind)) ++count; });
	if (count >= 32) return false;
	entt::entity entity = registry.create();
	auto &mob = registry.emplace<MobComponent>(entity);
	mob.randomState = u32(entity) ^ 0x9e3779b9u;
	mob.health = 1; mob.moveSpeed = 0; mob.attackStrength = shot.kind == Kind::Arrow ? 4 : 0;
	auto &traits = registry.emplace<BetaMobComponent>(entity);
	traits.kind = shot.kind;
	if (shot.shooter != entt::null) {
		traits.owner = "mob:" + std::to_string(u32(shot.shooter));
		traits.target = betaMobName(traitsFor(registry, shot.shooter).kind);
	}
	Vector3d direction = shot.direction;
	double length = direction.length();
	if (length < 1e-8) direction = {1, 0, 0};
	else direction /= length;
	traits.velocity = shot.kind == Kind::Fireball ? Vector3f{0, 0, 0} : Vector3f(direction * 0.6);
	traits.waypoint = Vector3f(direction * 0.1);
	registry.emplace<PositionComponent>(entity, shot.position.x, shot.position.y, shot.position.z, world.dimension().id());
	registry.emplace<NetworkComponent>(entity, entity);
	registry.emplace<std::string>(entity, "default:" + std::string(betaMobName(shot.kind)));
	registry.emplace<DoubleBox>(entity, -0.125, -0.125, -0.125, 0.25, 0.25, 0.25);
	registry.emplace<RotationComponent>(entity);
	registry.emplace<DrawableDef>(entity).setMobModel(u8(shot.kind));
	return true;
}

void explode(ServerWorld &world, const Blast &blast) {
	float reach = blast.strength * 2;
	for (const auto &entry : world.players()) {
		const auto &player = entry.second;
		if (!player.isReady() || !player.isOnline() || !player.health() || player.dimension() != world.dimension().id()) continue;
		Vector3d target{player.x(), player.y(), player.z() + 0.8};
		double distance = (target - blast.position).length() / reach;
		if (distance > 1 || !clearPath(world, blast.position, target)) continue;
		double impact = 1 - distance;
		u16 damage = u16((impact * impact + impact) * 4 * reach + 1);
		if (world.server()) world.server()->damagePlayer(player.clientID(), damage, "blast:" + blast.source);
	}
	world.scene().registry().view<MobComponent, PositionComponent>().each([&](auto entity, auto &mob, auto &position) {
		if (projectile(traitsFor(world.scene().registry(), entity).kind)) return;
		Vector3d target{position.x, position.y, position.z + 0.5};
		double distance = (target - blast.position).length() / reach;
		if (distance > 1 || !clearPath(world, blast.position, target)) return;
		u16 damage = u16(((1 - distance) * (1 - distance) + 1 - distance) * 4 * reach + 1);
		mob.health = damage >= mob.health ? 0 : mob.health - damage;
	});
	// beta's edge-ray blast propagation, bounded to already loaded terrain.
	MobComponent random; random.randomState = blast.seed;
	std::unordered_set<Vector3i> blocks;
	for (int x = 0; x < 16; ++x) for (int y = 0; y < 16; ++y) for (int z = 0; z < 16; ++z) {
		if (x && x != 15 && y && y != 15 && z && z != 15) continue;
		Vector3d direction{x / 15.0 * 2 - 1, y / 15.0 * 2 - 1, z / 15.0 * 2 - 1};
		direction /= direction.length();
		double energy = blast.strength * (0.7 + unitRandom(random) * 0.6);
		Vector3d point = blast.position;
		for (int step = 0; energy > 0 && step < 80; ++step) {
			int bx = int(std::floor(point.x)), by = int(std::floor(point.y)), bz = int(std::floor(point.z));
			const Chunk *chunk = world.getChunkAtBlockPos(bx, by, bz);
			if (!chunk || !chunk->isInitialized()) break;
			const BlockState *state = world.getBlockState(bx, by, bz);
			if (state && state->block().id()) {
				double resistance = std::max(0.f, state->hardness());
				const auto &id = state->block().stringID();
				if (id == "default:obsidian" || id == "default:bedrock" || state->hardness() < 0 || !std::isfinite(state->hardness())) resistance = 1200;
				else if (id == "default:stone" || id == "default:cobblestone") resistance = 6;
				else if (state->drawType() == BlockDrawType::Liquid) resistance = 500;
				energy -= (resistance + 0.3) * 0.3;
			}
			if (energy > 0) blocks.emplace(bx, by, bz);
			point += direction * 0.3;
			energy -= 0.225;
		}
	}
	for (const auto &point : blocks) {
		u16 id = world.getBlock(point.x, point.y, point.z);
		if (!id) continue;
		const BlockState *state = world.getBlockState(point.x, point.y, point.z);
		if (state && state->block().stringID() == "default:tnt" && world.primeTnt(point, u16(10 + nextRandom(random) % 20))) continue;
		if (state && unitRandom(random) < 0.3) {
			for (const ItemStack &stack : state->getItemDrops()) {
				if (Registry::getInstance().hasItem(stack.stringID()))
					world.dropWorldItem({point.x + 0.5, point.y + 0.5, point.z + 0.5}, stack.stringID(), stack.amount());
			}
		}
		world.setBlock(point.x, point.y, point.z, 0);
	}
}

bool consumeFor(ServerPlayer &player, const std::string &replacement) {
	if (player.isCreative()) return true;
	ItemStack held = player.heldItemStack();
	if (!held.amount()) return false;
	if (held.amount() == 1) player.inventory().setStack(player.heldItemSlot(), 0, replacement, replacement.empty() ? 0 : 1);
	else {
		if (!replacement.empty() && player.inventory().addStack(replacement, 1).amount()) return false;
		player.inventory().setStack(player.heldItemSlot(), 0, held.item().stringID(), held.amount() - 1);
	}
	return true;
}

void updateAxolotl(ServerWorld &world, MobComponent &mob, BetaMobComponent &traits,
		PositionComponent &position, const DoubleBox &localBox, RotationComponent &rotation) {
	Vector3d origin{position.x, position.y, position.z};
	Vector3d center = origin + Vector3d{0, 0, localBox.sizeZ * 0.5};
	bool swimming = waterAt(world, center);
	Vector3d difference = Vector3d(traits.waypoint) - origin;
	if (traits.jumpTimer) --traits.jumpTimer;
	if (!traits.jumpTimer || difference.length() < 0.4) {
		traits.jumpTimer = 20;
		if (swimming) {
			for (int attempt = 0; attempt < 12; ++attempt) {
				Vector3d candidate = origin + Vector3d{int(nextRandom(mob) % 9) - 4.0,
					int(nextRandom(mob) % 9) - 4.0, int(nextRandom(mob) % 5) - 2.0};
				if (waterAt(world, candidate + Vector3d{0,0,0.2}) && clearPath(world, center, candidate + Vector3d{0,0,0.2})) {
					traits.waypoint = Vector3f(candidate); break;
				}
			}
		}
		else {
			double nearest = 6 * 6 + 1;
			for (int dz = -1; dz <= 1; ++dz) for (int dy = -6; dy <= 6; ++dy) for (int dx = -6; dx <= 6; ++dx) {
				double distance = dx * dx + dy * dy + dz * dz;
				if (distance >= nearest) continue;
				Vector3d candidate{std::floor(origin.x) + dx + 0.5, std::floor(origin.y) + dy + 0.5, std::floor(origin.z) + dz};
				if (waterAt(world, candidate + Vector3d{0,0,0.2}) && clearPath(world, center, candidate + Vector3d{0,0,0.2})) {
					nearest = distance; traits.waypoint = Vector3f(candidate);
				}
			}
			if (nearest > 36) traits.waypoint = Vector3f(origin + Vector3d{unitRandom(mob) * 4 - 2, unitRandom(mob) * 4 - 2, 0});
		}
		difference = Vector3d(traits.waypoint) - origin;
	}
	if (difference.x * difference.x + difference.y * difference.y > 0.01)
		mob.yaw = float(std::atan2(difference.y, difference.x));
	Vector3d desired{0, 0, 0};
	if (swimming) {
		double length = difference.length();
		if (length > 0.1) desired = difference * (0.1 / length);
		if (!waterAt(world, center + desired)) desired = {0, 0, 0};
		mob.verticalSpeed = 0; traits.eggTimer = 6000;
	}
	else {
		mob.verticalSpeed = std::max(-0.5f, (mob.verticalSpeed - 0.08f) * 0.98f);
		desired = {std::cos(mob.yaw) * 0.075, std::sin(mob.yaw) * 0.075, mob.verticalSpeed};
		if (traits.eggTimer) --traits.eggTimer;
		else if (mob.age % 20 == 0) mob.health = mob.health <= 2 ? 0 : mob.health - 2;
	}
	DoubleBox box = localBox; box.move(position.x, position.y, position.z);
	if (!swimming) avoidLava(world, box, desired, mob);
	Vector3d movement = collision::move(world, box, desired);
	traits.grounded = desired.z < 0 && movement.z > desired.z;
	if (movement.z != desired.z) mob.verticalSpeed = 0;
	bool blocked = std::abs(movement.x - desired.x) > 0.001 || std::abs(movement.y - desired.y) > 0.001;
	if (blocked) {
		traits.jumpTimer = 0;
		if (traits.grounded) mob.verticalSpeed = 0.42f;
	}
	position.x += movement.x; position.y += movement.y; position.z += movement.z;
	position.isUpdated = position.isUpdated || !movement.isZero();
	rotation.quat = glm::angleAxis(mob.yaw + float(M_PI / 2), glm::vec3(0,0,1)); rotation.isUpdated = true;
}

bool opaqueCeiling(const World &world, int x, int y, int z) {
	const BlockState *state = world.getBlockState(x, y, z);
	if (!state || !state->block().id() || !state->isCollidable() || state->drawType() != BlockDrawType::Solid) return false;
	DoubleBox box = collision::blockBox(*state, world.getData(x, y, z), x, y, z);
	return box.sizeX == 1 && box.sizeY == 1 && box.sizeZ == 1;
}

void updateBat(ServerWorld &world, MobComponent &mob, BetaMobComponent &traits,
		PositionComponent &position, const DoubleBox &localBox, RotationComponent &rotation) {
	Vector3d origin{position.x, position.y, position.z};
	int x = int(std::floor(position.x)), y = int(std::floor(position.y));
	int ceiling = int(std::ceil(position.z + localBox.sizeZ - 0.001));
	bool supported = opaqueCeiling(world, x, y, ceiling);
	bool startled = traits.anger > 0;
	for (const auto &entry : world.players()) {
		const auto &player = entry.second;
		if (!player.isReady() || !player.isOnline() || !player.health() || player.dimension() != world.dimension().id()) continue;
		if ((Vector3d{player.x(), player.y(), player.z()} - origin).length() < 4) startled = true;
	}
	if (traits.sitting && (!supported || startled)) {
		traits.sitting = false;
		traits.jumpTimer = 60;
		traits.velocity = {0, 0, -0.08f};
		traits.waypoint = Vector3f(origin);
	}
	if (traits.jumpTimer) --traits.jumpTimer;
	if (!traits.sitting && !startled && !traits.jumpTimer && supported
	 && ceiling - position.z - localBox.sizeZ < 0.15 && nextRandom(mob) % 100 == 0) traits.sitting = true;
	if (traits.sitting) {
		position.z = ceiling - localBox.sizeZ;
		position.isUpdated = true;
		traits.velocity = {0, 0, 0};
		if (nextRandom(mob) % 200 == 0) mob.yaw = float(unitRandom(mob) * 2 * M_PI);
	}
	else {
		Vector3d difference = Vector3d(traits.waypoint) - origin;
		if (difference.length() < 1 || nextRandom(mob) % 30 == 0 || !clearPath(world, origin, Vector3d(traits.waypoint))) {
			for (int attempt = 0; attempt < 6; ++attempt) {
				Vector3d candidate{position.x + int(nextRandom(mob) % 7) - int(nextRandom(mob) % 7),
					position.y + int(nextRandom(mob) % 7) - int(nextRandom(mob) % 7),
					std::max(1.0, std::min(126.0, position.z + int(nextRandom(mob) % 6) - 2))};
				const Chunk *chunk = world.getChunkAtBlockPos(int(std::floor(candidate.x)), int(std::floor(candidate.y)), int(std::floor(candidate.z)));
				if (chunk && chunk->isInitialized() && !liquidAt(world, candidate) && clearPath(world, origin, candidate)) {
					traits.waypoint = Vector3f(candidate);
					difference = candidate - origin;
					break;
				}
			}
		}
		auto approach = [](float current, double delta, float speed) {
			float wanted = delta > 0.05 ? speed : delta < -0.05 ? -speed : 0;
			return current + (wanted - current) * 0.1f;
		};
		traits.velocity.x = approach(traits.velocity.x, difference.x, 0.25f);
		traits.velocity.y = approach(traits.velocity.y, difference.y, 0.25f);
		traits.velocity.z = approach(traits.velocity.z, difference.z, 0.18f);
		DoubleBox box = localBox; box.move(position.x, position.y, position.z);
		Vector3d desired = traits.velocity;
		Vector3d movement = collision::move(world, box, desired, false);
		if ((movement - desired).length() > 0.001) {
			traits.waypoint = Vector3f(origin);
			traits.velocity *= -0.25f;
		}
		position.x += movement.x; position.y += movement.y; position.z += movement.z;
		position.isUpdated = position.isUpdated || !movement.isZero();
		if (std::abs(movement.x) + std::abs(movement.y) > 0.001) mob.yaw = float(std::atan2(movement.y, movement.x));
	}
	mob.verticalSpeed = 0;
	rotation.quat = glm::angleAxis(mob.yaw + float(M_PI / 2), glm::vec3(0, 0, 1));
	rotation.isUpdated = true;
}

}

void ServerWorld::explodeAt(const Vector3d &position, float strength, u32 seed, const std::string &source) {
	explode(*this, {position, strength, seed, source});
	if (m_server) {
		m_server->sendSound("tnt_explode", position, m_dimension.id(), 1.f);
	}
}

bool ServerWorld::spawnProjectile(const std::string &id, Vector3d position, Vector3d direction) {
	Kind kind = id == "default:arrow" ? Kind::Arrow : id == "default:snowball" ? Kind::Snowball : id == "default:egg" ? Kind::Egg : betaMobKind(id);
	if (kind != Kind::Arrow && kind != Kind::Snowball && kind != Kind::Egg) return false;
	if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)
	 || !std::isfinite(direction.x) || !std::isfinite(direction.y) || !std::isfinite(direction.z) || direction.length() < 1e-8) return false;
	int x = int(std::floor(position.x)), y = int(std::floor(position.y)), z = int(std::floor(position.z));
	const Chunk *chunk = getChunkAtBlockPos(x, y, z);
	const BlockState *state = getBlockState(x, y, z);
	if (!chunk || !chunk->isInitialized() || !state || (state->block().id() && state->isCollidable())) return false;
	return spawnShot(*this, {position, direction, kind, entt::null});
}

bool ServerWorld::spawnMob(const std::string &id, double x, double y, double z, u8 slimeSize, entt::entity *spawned) {
	if (spawned) *spawned = entt::null;
	std::size_t shots = 0;
	m_scene.registry().view<BetaMobComponent>().each([&](auto, auto &traits) { if (projectile(traits.kind)) ++shots; });
	if (m_scene.registry().view<MobComponent>().size() >= 64 + shots) return false;
	Registry &definitions = Registry::getInstance();
	entt::entity model = definitions.getEntityFromStringID(id);
	if (model == entt::null || !definitions.entityRegistry().all_of<MobComponent, DoubleBox, RotationComponent>(model)) return false;
	const Chunk *chunk = getChunkAtBlockPos(int(std::floor(x)), int(std::floor(y)), int(std::floor(z)));
	if (!chunk || !chunk->isInitialized()) return false;
	DoubleBox box = definitions.entityRegistry().get<DoubleBox>(model);
	if (betaMobKind(id) == Kind::Slime) {
		MobComponent random; random.randomState = u32(m_seed) ^ u32(int(x)) ^ (u32(int(y)) * 31u) ^ m_mobTick;
		if (!slimeSize) slimeSize = u8(1 << (nextRandom(random) % 3));
		if (slimeSize != 1 && slimeSize != 2 && slimeSize != 4) return false;
		box.reset(-0.3 * slimeSize, -0.3 * slimeSize, 0, 0.6 * slimeSize, 0.6 * slimeSize, 0.6 * slimeSize);
	}
	box.move(x, y, z);
	for (int bz = int(std::floor(box.z)); bz <= int(std::floor(box.z + box.sizeZ)); ++bz)
		for (int by = int(std::floor(box.y)); by <= int(std::floor(box.y + box.sizeY)); ++by)
			for (int bx = int(std::floor(box.x)); bx <= int(std::floor(box.x + box.sizeX)); ++bx) {
				const BlockState *state = getBlockState(bx, by, bz);
				if (!state || (state->block().id() && state->isCollidable()
				 && collision::blockBox(*state, getData(bx, by, bz), bx, by, bz).intersects(box))) return false;
			}
	entt::entity entity = m_scene.createEntityFromModel(definitions.entityRegistry(), model);
	auto &registry = m_scene.registry();
	registry.emplace<PositionComponent>(entity, x, y, z, m_dimension.id());
	registry.emplace<NetworkComponent>(entity, entity);
	registry.emplace<std::string>(entity, id);
	auto &mob = registry.get<MobComponent>(entity);
	mob.randomState = u32(m_seed) ^ u32(entity) ^ 0x9e3779b9u;
	auto &traits = traitsFor(registry, entity);
	traits.eggTimer = 6000 + nextRandom(mob) % 6000;
	traits.jumpTimer = u16(10 + nextRandom(mob) % 20);
	traits.waypoint = {float(x), float(y), float(z)};
	if (traits.kind == Kind::Sheep) {
		u32 color = nextRandom(mob) % 100;
		traits.color = color < 5 ? 15 : color < 10 ? 7 : color < 15 ? 8 : color < 18 ? 12 : nextRandom(mob) % 500 == 0 ? 6 : 0;
	}
	if (traits.kind == Kind::Axolotl) { traits.color = u8(nextRandom(mob) % 7); traits.eggTimer = 6000; }
	if (traits.kind == Kind::Rabbit) {
		auto biome = m_heightmap.getOrCreateChunk(int(std::floor(x / 16)), int(std::floor(y / 16))).biomeAt(int(std::floor(x)) & 15, int(std::floor(y)) & 15);
		u32 color = nextRandom(mob) % 100;
		traits.color = biome == retro::Biome::Desert ? 1 : biome == retro::Biome::Taiga || biome == retro::Biome::Tundra
			? (color < 80 ? 2 : 3) : color < 50 ? 0 : color < 90 ? 4 : 5;
	}
	if (m_dimension.id() == 0 && z < 56 && registry.view<RareMobComponent>().size() < 2
	 && (traits.kind == Kind::Zombie || traits.kind == Kind::Skeleton || traits.kind == Kind::Spider)
	 && unitRandom(mob) < 1.0 / 192.0) {
		auto &rare = registry.emplace<RareMobComponent>(entity);
		rare.name = exploration::rareName(unsigned(traits.kind), nextRandom(mob) >> 16);
		mob.health = u16(mob.health + mob.health / 2);
		++mob.attackStrength;
		mob.moveSpeed *= 1.08f;
	}
	syncAppearance(registry, entity);
	if (traits.kind == Kind::Slime) setSlimeSize(registry, entity, slimeSize, true);
	if (spawned) *spawned = entity;
	return true;
}

void ServerWorld::updateMobs() {
	auto &registry = m_scene.registry();
	registry.view<MobComponent>().each([&](auto entity, auto &) { traitsFor(registry, entity); });
	std::vector<entt::entity> dead;
	std::vector<Shot> shots;
	std::vector<Blast> blasts;
	std::vector<std::pair<Vector3d, unsigned>> hatchings;
	registry.view<MobComponent, PositionComponent, DoubleBox, RotationComponent>().each(
		[&](auto entity, auto &mob, auto &position, auto &localBox, auto &rotation) {
			if (!mob.health) { dead.emplace_back(entity); return; }
			auto &traits = traitsFor(registry, entity);
			Kind kind = traits.kind;
			Vector3d origin{position.x, position.y, position.z};
			const Chunk *current = getChunkAtBlockPos(int(std::floor(position.x)), int(std::floor(position.y)), int(std::floor(position.z)));
			if (!current || !current->isInitialized()) {
				if (projectile(kind)) dead.emplace_back(entity);
				return;
			}
			if (auto *rare = registry.try_get<RareMobComponent>(entity); rare && !rare->announced && m_server) {
				for (const auto &entry : m_players) {
					const auto &player = entry.second;
					if (!player.isReady() || !player.isOnline() || player.dimension() != m_dimension.id()) continue;
					Vector3d eye{player.x(), player.y(), player.z() + 1.6};
					if ((eye - origin).length() <= 10 && clearPath(*this, eye, origin + Vector3d{0,0,0.8})) {
						m_server->sendPlayerMessage(player.clientID(), rare->name + " lurks nearby.");
						rare->announced = true;
					}
				}
			}
			++mob.age;
			if (mob.attackCooldown) --mob.attackCooldown;
			if (traits.anger) --traits.anger;
			if (projectile(kind)) {
				if (mob.age > 200) { dead.emplace_back(entity); return; }
				Vector3d velocity = traits.velocity;
				Vector3d end = origin + velocity;
				bool hit = !clearPath(*this, origin, end);
				for (const auto &entry : m_players) {
					const auto &player = entry.second;
					if (!player.isReady() || !player.isOnline() || !player.health() || player.dimension() != m_dimension.id() || player.name() == traits.owner) continue;
					DoubleBox box = player.hitbox(); box.move(player.x(), player.y(), player.z());
					double t = 1;
					if (segmentBox(origin, velocity, box, t) && clearPath(*this, origin, origin + velocity * t)) {
						end = origin + velocity * t; hit = true;
						if (kind == Kind::Arrow && m_server) m_server->damagePlayer(player.clientID(), 4, "arrow:" + traits.target);
						break;
					}
				}
				registry.view<MobComponent, PositionComponent, DoubleBox>().each([&](auto other, auto &otherMob, auto &otherPosition, auto &hitbox) {
					if (hit || other == entity || projectile(traitsFor(registry, other).kind)
					 || (mob.age < 5 && traits.owner == "mob:" + std::to_string(u32(other)))) return;
					DoubleBox box = hitbox; box.move(otherPosition.x, otherPosition.y, otherPosition.z);
					double t = 1;
					if (!segmentBox(origin, velocity, box, t)) return;
					end = origin + velocity * t; hit = true;
					if (kind == Kind::Arrow) otherMob.health = otherMob.health <= 4 ? 0 : otherMob.health - 4;
					if (kind == Kind::Snowball) {
						// source snowballs deal no damage to ordinary mobs, but still push them back.
						DoubleBox targetBox = hitbox; targetBox.move(otherPosition.x, otherPosition.y, otherPosition.z);
						Vector3d push = collision::move(*this, targetBox, {velocity.x * 0.4, velocity.y * 0.4, 0.1});
						otherPosition.x += push.x; otherPosition.y += push.y; otherPosition.z += push.z;
						otherPosition.isUpdated = true;
					}
				});
				position.x = end.x; position.y = end.y; position.z = end.z; position.isUpdated = true;
				if (hit) {
					if (kind == Kind::Egg && (nextRandom(mob) >> 16) % 8 == 0)
						hatchings.emplace_back(origin, (nextRandom(mob) >> 16) % 32 == 0 ? 4u : 1u);
					if (kind == Kind::Fireball) blasts.push_back({end, 1, nextRandom(mob), traits.target});
					dead.emplace_back(entity);
				}
				if (kind != Kind::Fireball) { traits.velocity *= 0.99f; traits.velocity.z -= 0.03f; }
				else traits.velocity = (traits.velocity + traits.waypoint) * 0.95f;
				rotation.quat = glm::angleAxis(float(std::atan2(velocity.y, velocity.x)
					+ (kind == Kind::Arrow ? 90 * math::DEG_TO_RAD : 0)), glm::vec3(0, 0, 1));
				rotation.isUpdated = true;
				return;
			}
			DoubleBox body = localBox; body.move(position.x, position.y, position.z);
			bool inLava = lavaInBox(*this, body);
			if (inLava && kind != Kind::PigZombie && kind != Kind::Ghast && mob.age % 10 == 0) {
				mob.health = mob.health <= 4 ? 0 : mob.health - 4;
				if (!mob.health) { dead.emplace_back(entity); return; }
			}
			if (kind == Kind::Bat) {
				updateBat(*this, mob, traits, position, localBox, rotation);
				syncAppearance(registry, entity);
				return;
			}
			if (kind == Kind::Axolotl) {
				updateAxolotl(*this, mob, traits, position, localBox, rotation);
				if (!mob.health) dead.emplace_back(entity);
				return;
			}
			bool swimming = waterAt(*this, origin + Vector3d{0, 0, localBox.sizeZ * 0.5});
			ServerPlayer *target = nullptr;
			double nearest = kind == Kind::Ghast ? 64 * 64 : kind == Kind::Ocelot ? 12 * 12 : kind == Kind::Rabbit ? 8 * 8 : 16 * 16;
			bool neutral = kind == Kind::Wolf || kind == Kind::PigZombie;
			bool sunlit = GameTime::getTicks() % GameTime::dayLength < 12000
				&& position.z >= m_heightmap.getHighestBlockAt(int(std::floor(position.x)), int(std::floor(position.y))) + 1;
			bool aggressive = mob.hostile || (neutral && traits.anger > 0);
			if (kind == Kind::Spider && sunlit && !traits.anger) aggressive = false;
			for (auto &entry : m_players) {
				auto &player = entry.second;
				if (!player.isReady() || !player.isOnline() || !player.health() || player.dimension() != m_dimension.id()) continue;
				if (player.isCreative() && kind != Kind::Ocelot && kind != Kind::Rabbit) continue;
				bool following = kind == Kind::Wolf && traits.tamed && !traits.sitting && !traits.anger && player.name() == traits.owner;
				if (kind == Kind::Rabbit) following = true;
				if (kind == Kind::Ocelot) {
					bool fish = holdingItem(player, "default:fish_cod_raw");
					following = fish || !traits.tamed || traits.anger;
				}
				if (!aggressive && !following) continue;
				if (!traits.target.empty() && traits.target != player.name() && !following && kind != Kind::Ocelot) continue;
				double dx = player.x() - position.x, dy = player.y() - position.y, dz = player.z() - position.z;
				double distance = dx * dx + dy * dy + dz * dz;
				if (distance < nearest && clearPath(*this, origin + Vector3d{0, 0, localBox.sizeZ * 0.8}, {player.x(), player.y(), player.z() + 1.5})) {
					target = &player; nearest = distance;
				}
			}
			if (target) mob.yaw = float(std::atan2(target->y() - position.y, target->x() - position.x));
			else if (mob.age % 60 == 1) mob.yaw = float(nextRandom(mob) % 6284) / 1000.f;
			bool ocelotFlee = false, rabbitFlee = false, hunting = false;
			if (kind == Kind::Ocelot) {
				traits.sitting = false;
				if (target) {
					Vector3d playerPosition{target->x(), target->y(), target->z()};
					bool fish = holdingItem(*target, "default:fish_cod_raw");
					if (!traits.tamed && traits.target == target->name() && fish
					 && ((playerPosition - Vector3d(traits.waypoint)).length() > 0.1
					  || std::abs(std::remainder(target->cameraYaw() - traits.velocity.x, 360.f)) > 5
					  || std::abs(target->cameraPitch() - traits.velocity.y) > 5)) traits.anger = 80;
					ocelotFlee = traits.anger || (!traits.tamed && !fish);
					if (ocelotFlee) mob.yaw += float(M_PI);
					traits.sitting = fish && !ocelotFlee && nearest > 6.25;
					traits.target = target->name();
					traits.waypoint = Vector3f(playerPosition);
					traits.velocity = {target->cameraYaw(), target->cameraPitch(), 0};
				}
				else {
					traits.target.clear();
					double preyDistance = 8 * 8;
					entt::entity prey = entt::null;
					registry.view<MobComponent, BetaMobComponent, PositionComponent>().each([&](auto other, auto &otherMob, auto &otherTraits, auto &otherPosition) {
						if (otherTraits.kind != Kind::Chicken || !otherMob.health || otherPosition.dimension != position.dimension) return;
						Vector3d delta = Vector3d{otherPosition.x, otherPosition.y, otherPosition.z} - origin;
						double distance = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
						if (distance < preyDistance && clearPath(*this, origin + Vector3d{0,0,0.4}, origin + delta + Vector3d{0,0,0.2})) {
							preyDistance = distance; prey = other;
						}
					});
					if (prey != entt::null) {
						const auto &preyPosition = registry.get<PositionComponent>(prey);
						mob.yaw = float(std::atan2(preyPosition.y - position.y, preyPosition.x - position.x));
						hunting = true;
						if (preyDistance < 1 && !mob.attackCooldown) {
							auto &preyMob = registry.get<MobComponent>(prey);
							preyMob.health = preyMob.health <= 3 ? 0 : preyMob.health - 3;
							mob.attackCooldown = 20;
						}
					}
				}
			}
			if (kind == Kind::Rabbit) {
				rabbitFlee = target && !holdingItem(*target, "default:dandelion");
				if (rabbitFlee) mob.yaw += float(M_PI);
				double danger = target ? nearest : 100;
				registry.view<MobComponent, BetaMobComponent, PositionComponent>().each([&](auto other, auto &otherMob, auto &otherTraits, auto &otherPosition) {
					if (!otherMob.health || other == entity || (!otherMob.hostile && otherTraits.kind != Kind::Wolf)) return;
					Vector3d delta = origin - Vector3d{otherPosition.x, otherPosition.y, otherPosition.z};
					double distance = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
					double radius = otherTraits.kind == Kind::Wolf ? 10 : 4;
					if (distance < danger && distance < radius * radius && clearPath(*this, origin + Vector3d{0,0,0.3}, origin - delta + Vector3d{0,0,0.3})) {
						danger = distance; mob.yaw = float(std::atan2(delta.y, delta.x)); rabbitFlee = true; hunting = true;
					}
				});
			}
			if (kind == Kind::SnowGolem) {
				double closest = 100;
				Vector3d aim;
				registry.view<MobComponent, BetaMobComponent, PositionComponent, DoubleBox>().each([&](auto, auto &otherMob, auto &otherTraits, auto &otherPosition, auto &otherBox) {
					if (!otherMob.health || projectile(otherTraits.kind) || (!otherMob.hostile && otherTraits.kind != Kind::PigZombie)) return;
					Vector3d delta = Vector3d{otherPosition.x, otherPosition.y, otherPosition.z + otherBox.sizeZ * 0.6} - (origin + Vector3d{0,0,1.6});
					double distance = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
					if (distance < closest && clearPath(*this, origin + Vector3d{0,0,1.6}, origin + Vector3d{0,0,1.6} + delta)) {
						closest = distance; aim = delta; hunting = true;
					}
				});
				if (hunting) {
					mob.yaw = float(std::atan2(aim.y, aim.x));
					if (!mob.attackCooldown) {
						aim.z += std::sqrt(aim.x * aim.x + aim.y * aim.y) * 0.08;
						shots.push_back({origin + Vector3d{0,0,1.6}, aim, Kind::Snowball, entity}); mob.attackCooldown = 20;
					}
				}
				if (mob.age % 10 == 0) {
					auto biome = m_heightmap.getOrCreateChunk(int(std::floor(position.x / 16)), int(std::floor(position.y / 16)))
						.biomeAt(int(std::floor(position.x)) & 15, int(std::floor(position.y)) & 15);
					if (swimming || m_dimension.id() == 1 || biome == retro::Biome::Desert) --mob.health;
				}
			}
			bool attacksPlayer = target && aggressive && !(traits.tamed && target->name() == traits.owner);
			if (kind == Kind::Creeper) {
				if (advanceCreeperFuse(traits, std::sqrt(nearest), attacksPlayer)) {
					blasts.push_back({origin, 3, nextRandom(mob), "creeper"}); dead.emplace_back(entity); return;
				}
			}
			else if (kind == Kind::Skeleton && attacksPlayer && nearest < 100 && !mob.attackCooldown) {
				Vector3d muzzle = origin + Vector3d{0, 0, 1.4};
				Vector3d direction{target->x() - position.x, target->y() - position.y, target->z() + 1.425 - muzzle.z};
				direction.z += std::sqrt(direction.x * direction.x + direction.y * direction.y) * 0.2;
				shots.push_back({muzzle, direction, Kind::Arrow, entity}); mob.attackCooldown = 30;
			}
			else if (kind == Kind::Ghast) {
				if (attacksPlayer) {
					if (++traits.attackCharge >= 20) {
						Vector3d muzzle = origin + Vector3d{std::cos(mob.yaw) * 4, std::sin(mob.yaw) * 4, 2.5};
						shots.push_back({muzzle, Vector3d{target->x(), target->y(), target->z() + 0.9} - muzzle, Kind::Fireball, entity});
						traits.attackCharge = -40;
					}
				}
				else if (traits.attackCharge > 0) --traits.attackCharge;
			}
			else if (attacksPlayer && mob.attackCooldown == 0) {
				double range = kind == Kind::Wolf ? 1.5 : kind == Kind::Slime ? traits.size * 0.6 : 2;
				if (nearest < range * range && (kind != Kind::Slime || traits.size > 1)) {
					mob.attackCooldown = 20;
					if (m_server) m_server->damagePlayer(target->clientID(), kind == Kind::Wolf && traits.tamed ? 4 : mob.attackStrength, "mob:" + std::string(betaMobName(kind)));
				}
			}
			bool walking = target || hunting || (mob.age % 100 < 60);
			if (kind == Kind::Ocelot && target && !ocelotFlee && nearest < 6.25) walking = false;
			if (kind == Kind::Wolf && (traits.sitting || (traits.tamed && target && nearest < 25))) walking = false;
			if (kind == Kind::Creeper && traits.fuse) walking = false;
			if (kind == Kind::SnowGolem && hunting) walking = false;
			if (kind == Kind::Rabbit && target && !rabbitFlee && nearest < 4) walking = false;
			if (kind == Kind::Skeleton && target && nearest < 36) walking = false;
			float speed = walking ? mob.moveSpeed * 0.2f : 0;
			if (kind == Kind::Ocelot && walking) speed *= ocelotFlee ? 1.33f : target ? 0.6f : hunting ? 1.25f : 1.f;
			if (kind == Kind::Rabbit && rabbitFlee) speed *= 1.8f;
			if (kind == Kind::PigZombie && traits.anger) speed = walking ? 0.19f : 0;
			mob.verticalSpeed = std::max(-0.5f, (mob.verticalSpeed - 0.08f) * 0.98f);
			if (kind == Kind::Chicken && mob.verticalSpeed < 0) mob.verticalSpeed *= 0.6f;
			if ((swimming || inLava) && kind != Kind::Squid) mob.verticalSpeed = std::max(mob.verticalSpeed, 0.08f);
			Vector3d desired{std::cos(mob.yaw) * speed, std::sin(mob.yaw) * speed, mob.verticalSpeed};
			if (kind == Kind::Slime) {
				if (traits.grounded) {
					if (traits.jumpTimer) --traits.jumpTimer;
					else {
						traits.jumpTimer = u16((10 + nextRandom(mob) % 20) / (target ? 3 : 1));
						mob.verticalSpeed = 0.42f; desired.z = mob.verticalSpeed;
					}
					if (traits.jumpTimer) desired.x = desired.y = 0;
				}
			}
			if (kind == Kind::Rabbit) {
				if (traits.grounded) {
					desired.x = desired.y = 0;
					if (traits.jumpTimer) --traits.jumpTimer;
					else if (walking) {
						Vector3d ahead = origin + Vector3d{std::cos(mob.yaw) * 0.6, std::sin(mob.yaw) * 0.6, 0.25};
						mob.verticalSpeed = clearPath(*this, origin + Vector3d{0,0,0.25}, ahead) ? 0.3f : 0.5f;
						desired.z = mob.verticalSpeed;
						traits.velocity = {std::cos(mob.yaw) * speed, std::sin(mob.yaw) * speed, 0};
						traits.jumpTimer = rabbitFlee ? 2 : 10;
						desired.x = traits.velocity.x; desired.y = traits.velocity.y;
					}
				}
				else { desired.x = traits.velocity.x; desired.y = traits.velocity.y; }
			}
			if ((kind == Kind::Spider || kind == Kind::Wolf) && target && nearest > 4 && nearest < 36 && traits.grounded && nextRandom(mob) % 10 == 0)
				desired.z = mob.verticalSpeed = 0.4f;
			if (kind == Kind::Ghast || (kind == Kind::Squid && swimming)) {
				if (mob.age % (kind == Kind::Ghast ? 20 : 50) == 1 || traits.velocity.isZero()) {
					traits.velocity = {float(std::cos(mob.yaw) * 0.2), float(std::sin(mob.yaw) * 0.2), float(unitRandom(mob) * 0.2 - 0.1)};
				}
				desired = traits.velocity;
			}
			if (kind == Kind::Squid && !swimming) desired.x = desired.y = 0;
			DoubleBox box = localBox; box.move(position.x, position.y, position.z);
			if (kind != Kind::Ghast && kind != Kind::Squid) {
				avoidLava(*this, box, desired, mob);
				if (kind == Kind::Rabbit) { traits.velocity.x = desired.x; traits.velocity.y = desired.y; }
			}
			Vector3d movement = collision::move(*this, box, desired);
			traits.grounded = desired.z < 0 && movement.z > desired.z;
			if (movement.z != desired.z) mob.verticalSpeed = 0;
			bool blocked = std::abs(movement.x - desired.x) > 0.001 || std::abs(movement.y - desired.y) > 0.001;
			if (kind == Kind::Spider && blocked) mob.verticalSpeed = 0.2f;
			else if (traits.grounded && blocked && kind != Kind::Rabbit) mob.verticalSpeed = 0.42f;
			position.x += movement.x; position.y += movement.y; position.z += movement.z;
			position.isUpdated = position.isUpdated || !movement.isZero();
			rotation.quat = glm::angleAxis(mob.yaw + float(M_PI / 2), glm::vec3(0, 0, 1)); rotation.isUpdated = true;
			if ((kind == Kind::Zombie || kind == Kind::Skeleton) && mob.age % 20 == 0 && sunlit) --mob.health;
			if (kind == Kind::Chicken && traits.size == 0) {
				if (traits.jumpTimer) --traits.jumpTimer;
				else {
					traits.size = 1; localBox.reset(-0.15, -0.15, 0, 0.3, 0.3, 0.4);
					auto &visual = registry.get<DrawableDef>(entity); visual.setMobModel(u8(kind)); visual.isUpdated = true;
				}
			}
			if (kind == Kind::Chicken && traits.size != 0) {
				if (!traits.eggTimer) traits.eggTimer = 6000 + nextRandom(mob) % 6000;
				if (--traits.eggTimer == 0) dropItem(*this, origin, "default:egg", 1);
			}
			syncAppearance(registry, entity);
			if (!mob.health) dead.emplace_back(entity);
		});
	for (const auto &blast : blasts) explode(*this, blast);
	registry.view<MobComponent>().each([&](auto entity, auto &mob) { if (!mob.health) dead.emplace_back(entity); });
	std::sort(dead.begin(), dead.end()); dead.erase(std::unique(dead.begin(), dead.end()), dead.end());
	for (auto entity : dead) {
		if (!registry.valid(entity)) continue;
		auto traits = traitsFor(registry, entity);
		auto position = registry.get<PositionComponent>(entity);
		const auto &mob = registry.get<MobComponent>(entity);
		if (!projectile(traits.kind) && !(traits.kind == Kind::Creeper && traits.fuse >= 30)) {
			auto &random = registry.get<MobComponent>(entity);
			u16 count = u16(nextRandom(random) % 3);
			if (traits.kind == Kind::Sheep) count = traits.sheared ? 0 : 1;
			if (traits.kind == Kind::Chicken && traits.size == 0) count = 0;
			if (traits.kind == Kind::Squid) count = u16(1 + nextRandom(random) % 3);
			if (traits.kind == Kind::SnowGolem) count = u16(nextRandom(random) % 16);
			if (traits.kind == Kind::Rabbit) {
				count = u16(nextRandom(random) % 2);
				dropItem(*this, {position.x, position.y, position.z}, "default:rabbit_hide", u16(nextRandom(random) % 2));
				if (nextRandom(random) % 10 == 0) dropItem(*this, {position.x, position.y, position.z}, "default:rabbit_foot", u16(nextRandom(random) % 2));
			}
			if (traits.kind == Kind::Slime && traits.size > 1) count = 0;
			dropItem(*this, {position.x, position.y, position.z}, mob.dropItem, count);
			if (traits.kind == Kind::Skeleton)
				dropItem(*this, {position.x, position.y, position.z}, "default:bone", u16(nextRandom(random) % 3));
		}
		if (registry.all_of<RareMobComponent>(entity)) {
			dropItem(*this, {position.x, position.y, position.z}, "default:iron_ingot", 2);
			dropItem(*this, {position.x, position.y, position.z}, "default:treasure_note", 1);
		}
		if (m_server) m_server->sendEntityDespawn(entity);
		registry.destroy(entity);
		if (traits.kind == Kind::Slime && traits.size > 1) {
			for (int child = 0; child < 4; ++child) {
				double x = position.x + ((child % 2) - 0.5) * traits.size / 4;
				double y = position.y + ((child / 2) - 0.5) * traits.size / 4;
				spawnMob("default:slime", x, y, position.z + 0.5, u8(traits.size / 2));
			}
		}
	}
	for (const auto &hatching : hatchings) for (unsigned i = 0; i < hatching.second; ++i) {
		entt::entity chick = entt::null;
		if (spawnMob("default:chicken", hatching.first.x, hatching.first.y, hatching.first.z, 0, &chick)) {
			auto &traits = registry.get<BetaMobComponent>(chick); traits.size = 0; traits.jumpTimer = 24000;
			registry.get<DoubleBox>(chick).reset(-0.075, -0.075, 0, 0.15, 0.15, 0.2);
			auto &visual = registry.get<DrawableDef>(chick); visual.setMobModel(u8(Kind::Chicken), 0.5f); visual.isUpdated = true;
		}
	}
	for (const auto &shot : shots) spawnShot(*this, shot);
	++m_mobTick;
	if (m_dimension.id() == 1) {
		if (m_mobTick % 20 == 0) spawnNetherMobs();
		return;
	}
	if (m_dimension.id() == 0 && m_mobTick % 20 == 0) spawnCaveMobs();
	if (m_mobTick % 200 != 0) return;
	bool night = GameTime::getTicks() % GameTime::dayLength >= 12000;
	static const char *passive[] = {"pig", "cow", "sheep", "chicken", "wolf"};
	static const char *hostile[] = {"zombie", "skeleton", "creeper", "spider"};
	unsigned bats = 0, ocelots = 0, rabbits = 0, axolotls = 0;
	registry.view<MobComponent, BetaMobComponent>().each([&](auto, auto &mob, auto &traits) {
		if (!mob.health) return;
		if (traits.kind == Kind::Bat) ++bats;
		if (traits.kind == Kind::Ocelot) ++ocelots;
		if (traits.kind == Kind::Rabbit) ++rabbits;
		if (traits.kind == Kind::Axolotl) ++axolotls;
	});
	for (const auto &entry : m_players) {
		const auto &player = entry.second;
		if (!player.isReady() || !player.isOnline() || !player.health() || player.dimension() != m_dimension.id()) continue;
		std::uint64_t spawnSeed = std::uint64_t(std::int64_t(m_seed)) + std::uint64_t(m_mobTick) * 341873128712ULL
			+ std::uint64_t(player.clientID()) * 132897987541ULL;
		retro::Random random(std::int64_t(spawnSeed & ((1ULL << 48) - 1)));
		int x = int(std::floor(player.x())) + random.nextInt(65) - 32;
		int y = int(std::floor(player.y())) + random.nextInt(65) - 32;
		double dx = x + 0.5 - player.x(), dy = y + 0.5 - player.y();
		if (dx * dx + dy * dy < 24 * 24) continue;
		if (m_dimension.id() != 0) continue;
		int cx = int(std::floor(x / 16.0)), cy = int(std::floor(y / 16.0));
		if (bats < 8 && random.nextInt(3) == 0) {
			DoubleBox box{-0.25, -0.25, 0, 0.5, 0.5, 0.89};
			int preferred = std::clamp(int(std::floor(player.z())) + random.nextInt(25) - 12, 1, SEALEVEL - 1);
			int floor = retro::mobCavityFloor(*this, box, x + 0.5, y + 0.5, preferred, 12, SEALEVEL - 1);
			if (floor >= 0 && floor < m_heightmap.getHighestBlockAt(x, y)) {
				int light = getLight(x, y, floor);
				if (light >= 0 && light <= 3 && spawnMob("default:bat", x + 0.5, y + 0.5, floor)) ++bats;
			}
		}
		if (axolotls < 8) {
			int base = std::clamp(int(std::floor(player.z())), 4, SEALEVEL - 4);
			for (int attempt = 0; attempt < 24; ++attempt) {
				int wz = std::clamp(base + random.nextInt(25) - 12, 1, SEALEVEL - 2);
				if (wz + 3 >= m_heightmap.getHighestBlockAt(x, y)) continue;
				if (waterAt(*this, {x + 0.5, y + 0.5, wz + 0.2}) && getLight(x, y, wz) <= 7
				 && spawnMob("default:axolotl", x + 0.5, y + 0.5, wz)) { ++axolotls; break; }
			}
		}
		if (random.nextInt(10) == 0 && retro::slimeChunk(m_seed, cx, cy)) {
			u8 size = u8(1 << random.nextInt(3));
			DoubleBox box{-0.3 * size, -0.3 * size, 0, 0.6 * size, 0.6 * size, 0.6 * size};
			int floor = retro::mobCavityFloor(*this, box, x + 0.5, y + 0.5, 1 + random.nextInt(15), 15, 15);
			if (floor >= 0) spawnMob("default:slime", x + 0.5, y + 0.5, floor, size);
		}
		int z = m_heightmap.getHighestBlockAt(x, y) + 1;
		const BlockState *ground = getBlockState(x, y, z - 1);
		if (z < SEALEVEL && liquidAt(*this, {x + 0.5, y + 0.5, SEALEVEL - 3.0})) {
			spawnMob("default:squid", x + 0.5, y + 0.5, SEALEVEL - 3.0); continue;
		}
		if (!ground || z <= SEALEVEL) continue;
		if (!night && rabbits < 8 && random.nextInt(8) == 0
		 && (ground->block().stringID() == "default:grass" || ground->block().stringID() == "default:sand")) {
			if (spawnMob("default:rabbit", x + 0.5, y + 0.5, z)) ++rabbits;
			continue;
		}
		if (night) spawnMob("default:" + std::string(hostile[(m_mobTick / 200) % 4]), x + 0.5, y + 0.5, z);
		else if (ground->block().stringID() == "default:grass") {
			auto biome = m_heightmap.getOrCreateChunk(cx, cy).biomeAt(x & 15, y & 15);
			if (ocelots < 6 && (biome == retro::Biome::Forest || biome == retro::Biome::Rainforest) && random.nextInt(16) == 0) {
				if (spawnMob("default:ocelot", x + 0.5, y + 0.5, z)) ++ocelots;
				continue;
			}
			const char *name = passive[(m_mobTick / 200) % 5];
			if (std::string(name) == "wolf") {
				auto biome = m_heightmap.getOrCreateChunk(int(std::floor(x / 16.0)), int(std::floor(y / 16.0))).biomeAt(x & 15, y & 15);
				if (biome != retro::Biome::Taiga && biome != retro::Biome::Forest) continue;
			}
			spawnMob("default:" + std::string(name), x + 0.5, y + 0.5, z);
		}
	}
}

void ServerWorld::spawnCaveMobs() {
	auto &registry = m_scene.registry();
	auto &definitions = Registry::getInstance();
	unsigned living = 0, hostiles = 0;
	registry.view<MobComponent, BetaMobComponent>().each([&](auto, const auto &mob, const auto &traits) {
		if (!mob.health || projectile(traits.kind)) return;
		++living;
		if (mob.hostile) ++hostiles;
	});
	if (living >= 64 || hostiles >= 32) return;
	static const char *names[] = {"default:zombie", "default:skeleton", "default:creeper", "default:spider"};
	for (const auto &entry : m_players) {
		const auto &player = entry.second;
		if (!player.isReady() || !player.isOnline() || !player.health() || player.dimension() != m_dimension.id()) continue;
		std::uint64_t seed = std::uint64_t(std::int64_t(m_seed)) + std::uint64_t(m_mobTick) * 341873128712ULL
			+ std::uint64_t(player.clientID()) * 132897987541ULL;
		retro::Random random(std::int64_t(seed & ((1ULL << 48) - 1)));
		unsigned spawned = 0;
		// bounded attempts search loaded caves without generating more terrain.
		for (unsigned attempt = 0; attempt < 8 && spawned < 2; ++attempt) {
			const char *name = names[random.nextInt(4)];
			if (!definitions.hasEntity(name)) continue;
			auto model = definitions.getEntityFromStringID(name);
			const auto *hitbox = definitions.entityRegistry().try_get<DoubleBox>(model);
			if (!hitbox) continue;
			int x = int(std::floor(player.x())) + random.nextInt(65) - 32;
			int y = int(std::floor(player.y())) + random.nextInt(65) - 32;
			int preferred = std::clamp(int(std::floor(player.z())) + random.nextInt(25) - 12, 1, 126);
			int z = retro::mobCavityFloor(*this, *hitbox, x + 0.5, y + 0.5, preferred, 12);
			if (z < 0 || z + hitbox->sizeZ >= m_heightmap.getHighestBlockAt(x, y)) continue;
			int light = getLight(x, y, z);
			if (light < 0 || light > 7) continue;
			bool clear = true;
			for (const auto &otherEntry : m_players) {
				const auto &other = otherEntry.second;
				if (!other.isOnline() || !other.health() || other.dimension() != m_dimension.id()) continue;
				double dx = x + 0.5 - other.x(), dy = y + 0.5 - other.y(), dz = z - other.z();
				if (dx * dx + dy * dy + dz * dz < 24 * 24) { clear = false; break; }
			}
			if (!clear) continue;
			DoubleBox box = *hitbox; box.move(x + 0.5, y + 0.5, z);
			registry.view<MobComponent, PositionComponent, DoubleBox>().each([&](auto entity, const auto &mob, const auto &position, const auto &localBox) {
				if (!clear || !mob.health || projectile(traitsFor(registry, entity).kind)) return;
				DoubleBox other = localBox; other.move(position.x, position.y, position.z);
				if (box.intersects(other)) clear = false;
			});
			if (clear && spawnMob(name, x + 0.5, y + 0.5, z)) {
				++spawned;
				++living;
				++hostiles;
				if (living >= 64 || hostiles >= 32) return;
			}
		}
	}
}

void ServerWorld::spawnNetherMobs() {
	auto &registry = m_scene.registry();
	auto &definitions = Registry::getInstance();
	unsigned living = 0;
	registry.view<MobComponent, BetaMobComponent>().each([&](auto, auto &mob, auto &traits) {
		if (mob.health && !projectile(traits.kind)) ++living;
	});
	if (living >= 64) return;
	std::unordered_set<Vector2i> visited;
	for (const auto &entry : m_players) {
		const auto &player = entry.second;
		if (!player.isReady() || !player.isOnline() || !player.health() || player.dimension() != m_dimension.id()) continue;
		int centerX = int(std::floor(player.x() / 16)), centerY = int(std::floor(player.y() / 16));
		for (int cy = centerY - 8; cy <= centerY + 8; ++cy) {
			for (int cx = centerX - 8; cx <= centerX + 8; ++cx) {
				if (!visited.emplace(cx, cy).second) continue;
				std::uint64_t seed = std::uint64_t(std::int64_t(m_seed)) + m_mobTick
					+ std::uint64_t(std::int64_t(cx)) * 341873128712ULL
					+ std::uint64_t(std::int64_t(cy)) * 132897987541ULL;
				retro::Random random(std::int64_t(seed & ((1ULL << 48) - 1)));
				bool ghast = random.nextInt(2) == 0;
				const char *name = ghast ? "default:ghast" : "default:pig_zombie";
				auto model = definitions.getEntityFromStringID(name);
				if (model == entt::null) continue;
				const auto *hitbox = definitions.entityRegistry().try_get<DoubleBox>(model);
				if (!hitbox) continue;
				int startX = cx * 16 + random.nextInt(16);
				int z = random.nextInt(128);
				int startY = cy * 16 + random.nextInt(16);
				const Chunk *chunk = getChunkAtBlockPos(startX, startY, z);
				if (!chunk || !chunk->isInitialized() || getBlock(startX, startY, z)) continue;
				int spawned = 0, limit = ghast ? 1 : 4;
				for (int group = 0; group < 3 && spawned < limit; ++group) {
					int x = startX, y = startY;
					for (int attempt = 0; attempt < 4 && spawned < limit; ++attempt) {
						x += random.nextInt(6); x -= random.nextInt(6);
						random.nextInt(1); random.nextInt(1);
						y += random.nextInt(6); y -= random.nextInt(6);
						if (retro::mobCavityFloor(*this, *hitbox, x + 0.5, y + 0.5, z, 0) != z) continue;
						bool clear = true;
						for (const auto &otherEntry : m_players) {
							const auto &other = otherEntry.second;
							if (!other.isOnline() || !other.health() || other.dimension() != m_dimension.id()) continue;
							double dx = x + 0.5 - other.x(), dy = y + 0.5 - other.y(), dz = z - other.z();
							if (dx * dx + dy * dy + dz * dz < 24 * 24) { clear = false; break; }
						}
						if (!clear) continue;
						DoubleBox box = *hitbox; box.move(x + 0.5, y + 0.5, z);
						registry.view<MobComponent, PositionComponent, DoubleBox>().each([&](auto entity, auto &, auto &position, auto &localBox) {
							if (!clear || projectile(traitsFor(registry, entity).kind)) return;
							DoubleBox other = localBox; other.move(position.x, position.y, position.z);
							if (box.intersects(other)) clear = false;
						});
						if (!clear || (ghast && random.nextInt(20) != 0)) continue;
						if (spawnMob(name, x + 0.5, y + 0.5, z)) {
							++spawned;
							if (++living >= 64) return;
						}
					}
				}
			}
		}
	}
}

entt::entity ServerWorld::findMobTarget(ServerPlayer &player, double reach) {
	Vector3d origin{player.x(), player.y(), player.z() + 1.625};
	double yaw = player.cameraYaw() * math::DEG_TO_RAD, pitch = player.cameraPitch() * math::DEG_TO_RAD;
	Vector3d direction{std::cos(yaw) * std::cos(pitch), std::sin(yaw) * std::cos(pitch), std::sin(pitch)};
	entt::entity selected = entt::null;
	m_scene.registry().view<MobComponent, PositionComponent, DoubleBox>().each([&](auto entity, auto &, auto &position, auto &hitbox) {
		DoubleBox box = hitbox; box.move(position.x, position.y, position.z);
		double distance = reach;
		if (segmentBox(origin, direction, box, distance) && distance < reach && clearPath(*this, origin, origin + direction * distance)) {
			reach = distance; selected = entity;
		}
	});
	return selected;
}

bool ServerWorld::interactMob(ServerPlayer &player) {
	entt::entity entity = findMobTarget(player, 3);
	if (entity == entt::null) return false;
	auto &registry = m_scene.registry();
	auto &traits = traitsFor(registry, entity);
	auto &mob = registry.get<MobComponent>(entity);
	ItemStack held;
	if (player.heldItemSlot() >= 0 && player.heldItemSlot() < player.inventory().width()) held = player.heldItemStack();
	std::string item = held.amount() ? held.item().stringID() : "";
	bool handled = false;
	if (traits.kind == Kind::Pig && item == "default:saddle" && !traits.saddled && consumeFor(player, "")) {
		traits.saddled = true; handled = true;
	}
	else if (traits.kind == Kind::Cow && item == "default:bucket_empty") handled = consumeFor(player, "default:bucket_milk");
	else if (traits.kind == Kind::Ocelot && item == "default:fish_cod_raw" && !traits.anger
	 && (traits.tamed || traits.target == player.name()) && consumeFor(player, "")) {
		if (!traits.tamed && nextRandom(mob) % 3 == 0) traits.tamed = true;
		if (traits.tamed) mob.health = std::min<u16>(10, mob.health + 2);
		handled = true;
	}
	else if (traits.kind == Kind::SnowGolem && item == "default:shears" && !traits.sheared) {
		traits.sheared = true;
		auto &position = registry.get<PositionComponent>(entity);
		dropItem(*this, {position.x, position.y, position.z}, "default:pumpkin", 1);
		if (!player.isCreative()) {
			if (m_server) m_server->wearHeldItem(player, 1);
			else player.inventory().getStackRef(player.heldItemSlot(), 0).wear(1);
		}
		handled = true;
	}
	else if (traits.kind == Kind::Rabbit && item == "default:dandelion" && mob.health < 3 && consumeFor(player, "")) {
		mob.health = 3; handled = true;
	}
	else if (traits.kind == Kind::Sheep && item == "default:shears" && !traits.sheared) {
		traits.sheared = true;
		auto &position = registry.get<PositionComponent>(entity);
		dropItem(*this, {position.x, position.y, position.z}, "default:wool", u16(2 + nextRandom(mob) % 3));
		if (m_server) m_server->wearHeldItem(player, 1);
		else player.inventory().getStackRef(player.heldItemSlot(), 0).wear(1);
		handled = true;
	}
	else if (traits.kind == Kind::Wolf) {
		if (!traits.tamed && !traits.anger && item == "default:bone" && consumeFor(player, "")) {
			if (nextRandom(mob) % 3 == 0) { traits.tamed = true; traits.sitting = true; traits.owner = player.name(); mob.health = 20; }
			handled = true;
		}
		else if (traits.tamed && traits.owner == player.name()) {
			if ((item == "default:porkchop_raw" || item == "default:porkchop_cooked") && mob.health < 20 && consumeFor(player, ""))
				mob.health = std::min<u16>(20, mob.health + 3);
			else traits.sitting = !traits.sitting;
			handled = true;
		}
	}
	if (handled) syncAppearance(registry, entity);
	if (handled && m_server) m_server->sendPlayerInvUpdate(player.clientID());
	return handled;
}

void ServerWorld::dropContainerItems(Vector3i position, const Inventory &inventory) {
	std::mt19937 random(u32(m_seed) ^ u32(position.x) ^ (u32(position.y) * 31) ^ (u32(position.z) * 131) ^ m_mobTick);
	std::uniform_real_distribution<float> offset(0.1f, 0.9f);
	std::uniform_int_distribution<unsigned> split(10, 30);
	std::normal_distribution<float> velocity(0, 1);
	for (const auto &stack : inventory.items()) {
		if (!stack.amount() || !stack.item().id()) continue;
		Vector3d point{position.x + offset(random), position.y + offset(random), position.z + offset(random) - 0.3};
		unsigned remaining = stack.amount();
		while (remaining) {
			u16 count = u16(std::min(remaining, split(random)));
			remaining -= count;
			auto entity = dropItem(*this, point, stack.stringID(), count, stack.damage(), false);
			if (entity == entt::null) continue;
			// beta stores velocity in blocks per tick; item physics uses blocks per second.
			auto &body = m_scene.registry().get_or_emplace<PhysicalComponent>(entity);
			body.velocity = {velocity(random), velocity(random), velocity(random) + 4};
		}
	}
}

bool ServerWorld::dropWorldItem(Vector3d position, const std::string &id, u16 count, u16 damage, bool limited) {
	return dropItem(*this, position, id, count, damage, limited) != entt::null;
}

bool ServerWorld::dropHeldItem(ServerPlayer &player) {
	int slot = player.heldItemSlot();
	if (!player.isReady() || !player.health() || player.dimension() != m_dimension.id()
	 || slot < 0 || slot >= 9 || slot >= player.inventory().width()) return false;
	ItemStack stack = player.heldItemStack();
	if (!stack.amount() || !stack.item().id()) return false;
	const Chunk *chunk = getChunkAtBlockPos(int(std::floor(player.x())), int(std::floor(player.y())), int(std::floor(player.z())));
	if (!chunk || !chunk->isInitialized()) return false;
	entt::entity entity = dropItem(*this, {player.x(), player.y(), player.z() + 1.025}, stack.stringID(), 1, stack.damage());
	if (entity == entt::null) return false;

	double yaw = player.cameraYaw() * math::DEG_TO_RAD;
	double pitch = player.cameraPitch() * math::DEG_TO_RAD;
	auto &body = m_scene.registry().emplace<PhysicalComponent>(entity);
	// beta throws at 0.3 blocks per tick; item physics uses blocks per second.
	body.velocity = {float(std::cos(yaw) * std::cos(pitch) * 6),
		float(std::sin(yaw) * std::cos(pitch) * 6), float(std::sin(pitch) * 6 + 2)};
	body.pickupDelay = 40;
	stack.setAmount(stack.amount() - 1);
	player.inventory().setItemStack(u16(slot), 0, stack.amount() ? stack : ItemStack::Empty);
	if (m_server) m_server->sendPlayerInvUpdate(player.clientID());
	return true;
}

void ServerWorld::dropPlayerItems(ServerPlayer &player) {
	for (u16 slot = 0; slot < 4; ++slot) {
		const auto &stack = player.armor().getStack(0, slot);
		if (stack.amount() && dropItem(*this, {player.x(),player.y(),player.z()}, stack.stringID(), stack.amount(), stack.damage(), false) != entt::null)
			player.armor().clearStack(0, slot);
	}
	for (u16 y = 0; y < player.inventory().height(); ++y) {
		for (u16 x = 0; x < player.inventory().width(); ++x) {
			ItemStack stack = player.inventory().getStack(x, y);
			if (stack.amount() && dropItem(*this, {player.x(), player.y(), player.z()}, stack.item().stringID(), stack.amount(), stack.damage(), false) != entt::null)
				player.inventory().clearStack(x, y);
		}
	}
	if (m_server) m_server->sendPlayerInvUpdate(player.clientID());
}

bool ServerWorld::dropPlayerStack(ServerPlayer &player, const ItemStack &stack) {
	if (!stack.amount() || stack.stringID() == BLOCK_AIR || !Registry::getInstance().hasItem(stack.stringID())) return false;
	// carried items must not disappear when ambient loot has reached its spawn cap.
	entt::entity entity = dropItem(*this, {player.x(), player.y(), player.z() + 1.025}, stack.stringID(), stack.amount(), stack.damage(), false);
	if (entity == entt::null) return false;
	double yaw = player.cameraYaw() * math::DEG_TO_RAD, pitch = player.cameraPitch() * math::DEG_TO_RAD;
	auto &body = m_scene.registry().emplace<PhysicalComponent>(entity);
	body.velocity = {float(std::cos(yaw) * std::cos(pitch) * 6), float(std::sin(yaw) * std::cos(pitch) * 6), float(std::sin(pitch) * 6 + 2)};
	body.pickupDelay = 40;
	return true;
}

bool ServerWorld::attackMob(ServerPlayer &player) {
	if (!player.health()) return false;
	entt::entity selected = findMobTarget(player, 3);
	if (selected == entt::null) return false;
	auto &registry = m_scene.registry();
	registry.view<MobComponent>().each([&](auto entity, auto &) { traitsFor(registry, entity); });
	auto &mob = registry.get<MobComponent>(selected);
	auto &traits = traitsFor(registry, selected);
	auto position = registry.get<PositionComponent>(selected);
	if (traits.kind == Kind::Fireball) {
		double yaw = player.cameraYaw() * math::DEG_TO_RAD, pitch = player.cameraPitch() * math::DEG_TO_RAD;
		traits.velocity = {float(std::cos(yaw) * std::cos(pitch)), float(std::sin(yaw) * std::cos(pitch)), float(std::sin(pitch))};
		traits.waypoint = traits.velocity * 0.1f; traits.owner = player.name(); traits.target = player.name(); return true;
	}
	if (projectile(traits.kind)) return false;
	if (traits.kind == Kind::Ocelot || traits.kind == Kind::Bat) { traits.anger = 100; traits.sitting = false; syncAppearance(registry, selected); }
	if (traits.kind == Kind::Spider) { traits.anger = 400; traits.target = player.name(); }
	if (traits.kind == Kind::PigZombie || traits.kind == Kind::Wolf) {
		registry.view<MobComponent, PositionComponent>().each([&](auto entity, auto &otherMob, auto &otherPosition) {
			auto &other = traitsFor(registry, entity);
			if (other.kind != traits.kind || (other.tamed && other.owner == player.name())) return;
			double dx = otherPosition.x - position.x, dy = otherPosition.y - position.y, dz = otherPosition.z - position.z;
			double radius = traits.kind == Kind::PigZombie ? 32 : 16;
			if (dx * dx + dy * dy + dz * dz <= radius * radius) {
				other.anger = 400 + nextRandom(otherMob) % 400; other.target = player.name(); other.sitting = false;
				syncAppearance(registry, entity);
			}
		});
	}
	u16 damage = 1;
	if (player.heldItemSlot() >= 0 && player.heldItemSlot() < player.inventory().width()) {
		auto &tool = player.inventory().getStackRef(player.heldItemSlot(), 0);
		damage = retro::attackDamage(tool.item());
		if (m_server) m_server->wearHeldItem(player, retro::attackingWear(tool.item()));
		else tool.wear(retro::attackingWear(tool.item()));
	}
	u16 dealt = std::min(damage, mob.health);
	mob.health -= dealt;
	player.statistics().general[PlayerStatistics::DamageDealt] += dealt;
	if (dealt && !mob.health) ++player.statistics().general[PlayerStatistics::MobKills];
	if (dealt && m_server) {
		const auto &box = registry.get<DoubleBox>(selected);
		m_server->sendEntityHurt(selected, dealt, position.x, position.y, position.z + box.sizeZ);
	}

	return true;
}
