#include <algorithm>
#include <cmath>
#include <glm/gtc/quaternion.hpp>

#include "common/core/Registry.hpp"
#include "common/core/LuaWidget.hpp"
#include "common/scene/component/CargoMinecartComponent.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/MinecartComponent.hpp"
#include "common/scene/component/NetworkComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/scene/component/RotationComponent.hpp"
#include "common/world/BetaRails.hpp"
#include "common/world/BlockSupport.hpp"
#include "common/world/Dimension.hpp"
#include "common/world/WorldCollision.hpp"
#include "server/core/PlayerList.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerPlayer.hpp"
#include "server/world/ServerWorld.hpp"

namespace {
const std::array<Vector3i, 4> railDirections{{{0,-1,0},{0,1,0},{-1,0,0},{1,0,0}}};
constexpr double railHeight = 1.0 / 16;
unsigned railShape(const BlockState &state) {
	return state.block().getGroupValue("group:om_rail_type") ? state.id()&7 : state.id();
}

const BlockState *railAt(const World &world, Vector3i point) {
	const Chunk *chunk = world.getChunkAtBlockPos(point.x, point.y, point.z);
	if (!chunk || !chunk->isInitialized()) return nullptr;
	const auto *state = world.getBlockState(point.x, point.y, point.z);
	return state && state->block().getGroupValue("group:om_rail") ? state : nullptr;
}

const BlockState *neighborRail(const World &world, Vector3i &point) {
	for (int height : {0, 1, -1}) {
		Vector3i candidate = point + Vector3i{0, 0, height};
		if (const auto *state = railAt(world, candidate)) {
			point = candidate;
			return state;
		}
	}
	return nullptr;
}

Vector3i railNeighborOffset(Vector3d end) {
	return {int(std::round((end.x - 0.5) * 2)), int(std::round((end.y - 0.5) * 2)), int(end.z)};
}

bool pointsToRail(const Vector3i &from, unsigned shape, const Vector3i &to) {
	for (auto end : retro::railEnds(shape)) {
		auto neighbor = from + railNeighborOffset(end);
		if (neighbor.x == to.x && neighbor.y == to.y) return true;
	}
	return false;
}

std::vector<Vector3i> railConnections(const World &world, const Vector3i &point, unsigned shape) {
	std::vector<Vector3i> connected;
	for (auto end : retro::railEnds(shape)) {
		Vector3i neighbor = point + railNeighborOffset(end);
		const auto *state = neighborRail(world, neighbor);
		if (state && pointsToRail(neighbor, railShape(*state), point)) connected.push_back(neighbor);
	}
	return connected;
}

bool canConnectRail(const World &world, const Vector3i &from, const BlockState &state, const Vector3i &to) {
	auto connected = railConnections(world, from, railShape(state));
	for (auto point : connected) if (point.x == to.x && point.y == to.y) return true;
	return connected.size() < 2;
}

const BlockState *findRail(const World &world, Vector3d position, Vector3i &point) {
	point = {int(std::floor(position.x)), int(std::floor(position.y)), int(std::floor(position.z))};
	if (auto *state = railAt(world, point)) return state;
	--point.z;
	return railAt(world, point);
}

DoubleBox cartBox(Vector3d position) {
	return {position.x - 0.49, position.y - 0.49, position.z, 0.98, 0.98, 0.7};
}

bool clearBox(const World &world, const DoubleBox &box) {
	bool loaded;
	auto obstacles = collision::collectObstacles(world, box, {0,0,0}, 0, loaded, false);
	if (!loaded) return false;
	for (const auto &obstacle : obstacles) if (box.intersects(obstacle.box)) return false;
	return true;
}

void showCargoInventory(ServerPlayer &player, entt::entity entity, CargoMinecartComponent &cargo) {
	if (!player.client()) return;
	cargo.viewer = player.name();
	cargo.inventory.setBlockPos({s32(u32(entity)), 0, 0});
	cargo.inventory.setInBlock(false);
	Network::Packet packet;
	packet << Network::Command::BlockGUIData << u16(176) << u16(166) << true << s16(-1);
	packet << u8(LuaWidget::Inventory) << "cargo" << cargo.inventory;
	packet << u8(LuaWidget::Image) << "background" << s32(0) << s32(0)
		<< "mods/default/textures/gui/inventory.png" << 0.f << 0.f << 0.f << 0.f << u16(176) << u16(166) << u16(5);
	auto slots = [&](const std::string &name, s32 y, u16 height, const std::string &destination, u16 offset, u16 count, bool cargoSlots) {
		packet << u8(LuaWidget::InventoryWidget) << name << s32(7) << y
			<< u16(9) << height << destination << offset << count
			<< std::string(cargoSlots ? "temp" : "player") << std::string{} << false << false;
		if (cargoSlots) packet << "cargo";
		else packet << player.name() << "main";
	};
	slots("cart",17,3,"inv_main,inv_hotbar",0,27,true);
	slots("inv_main",83,3,"cart",9,27,false);
	slots("inv_hotbar",141,1,"cart",0,9,false);
	player.client()->send(packet);
}

void sendRide(ServerPlayer &player, bool mounted) {
	player.setRidingMinecart(mounted);
	if (mounted) player.setNoclip(false);
	if (!player.client()) return;
	Network::Packet packet;
	packet << Network::Command::PlayerRideState << mounted << player.x() << player.y() << player.z();
	player.client()->send(packet);
}

entt::entity cartTarget(ServerWorld &world, ServerPlayer &player) {
	Vector3d origin{player.x(), player.y(), player.z() + (player.isRidingMinecart() ? 0.85 : 1.625)};
	double yaw = player.cameraYaw() * math::DEG_TO_RAD, pitch = player.cameraPitch() * math::DEG_TO_RAD;
	Vector3d ray{std::cos(yaw) * std::cos(pitch), std::sin(yaw) * std::cos(pitch), std::sin(pitch)};
	double reach = 3;
	entt::entity target = entt::null;
	world.scene().registry().view<MinecartComponent, PositionComponent>().each([&](auto entity, auto &, auto &position) {
		double hit = collision::rayDistance(origin, ray, cartBox({position.x,position.y,position.z}), reach);
		if (hit < 0 || hit >= reach) return;
		for (double step = 0; step < hit; step += 0.05) {
			Vector3d point = origin + ray * step;
			Vector3i block{int(std::floor(point.x)),int(std::floor(point.y)),int(std::floor(point.z))};
			const auto *state = world.getBlockState(block.x,block.y,block.z);
			if (!state) return;
			if (state->block().id() && state->isCollidable()) {
				double distance = collision::rayDistance(origin,ray,
					collision::blockBox(*state,world.getData(block.x,block.y,block.z),block.x,block.y,block.z),hit);
				if (distance >= 0 && distance < hit) return;
			}
		}
		reach = hit;
		target = entity;
	});
	return target;
}
}

void ServerWorld::refreshRail(const Vector3i &point) {
	const auto *state = railAt(*this, point);
	if (!state) return;
	std::array<int, 4> heights{{-2, -2, -2, -2}};
	for (unsigned side = 0; side < railDirections.size(); ++side) {
		Vector3i neighbor = point + railDirections[side];
		const auto *other = neighborRail(*this, neighbor);
		if (other && canConnectRail(*this, neighbor, *other, point)) heights[side] = neighbor.z - point.z;
	}
	unsigned type = state->block().getGroupValue("group:om_rail_type");
	unsigned shape = retro::railShape(heights, !type, railDirectlyPowered(point));
	u16 next = u16(shape | (type ? state->id() & 8 : 0));
	if (!blockHasSupport(*this, point, state->block().getState(next), getData(point.x, point.y, point.z))) return;
	bool changed = next != state->id();
	if (changed) setBlockState(point.x, point.y, point.z, next);
	if (type == 2) refreshPoweredRail(point);
	if (!changed) return;
	for (auto end : retro::railEnds(shape)) {
		Vector3i neighbor = point + railNeighborOffset(end);
		const auto *other = neighborRail(*this, neighbor);
		if (!other || !canConnectRail(*this, neighbor, *other, point)) continue;
		auto connected = railConnections(*this, neighbor, railShape(*other));
		connected.push_back(point);
		std::array<int, 4> otherHeights{{-2, -2, -2, -2}};
		for (auto connection : connected) for (unsigned side = 0; side < railDirections.size(); ++side) {
			auto target = neighbor + railDirections[side];
			if (connection.x == target.x && connection.y == target.y) otherHeights[side] = connection.z - neighbor.z;
		}
		unsigned otherType = other->block().getGroupValue("group:om_rail_type");
		u16 otherShape = u16(retro::railShape(otherHeights, !otherType) | (otherType ? other->id() & 8 : 0));
		if (otherShape != other->id() && blockHasSupport(*this, neighbor, other->block().getState(otherShape),
			getData(neighbor.x, neighbor.y, neighbor.z))) setBlockState(neighbor.x, neighbor.y, neighbor.z, otherShape);
	}
	for (auto direction : railDirections) for (int height : {-1, 0, 1})
		m_blockPhysics.enqueue(point + direction + Vector3i{0, 0, height});
}

bool ServerWorld::placeMinecart(Player &player, const Vector3i &point) {
	const auto *rail = railAt(*this, point);
	if (!rail || !player.health() || player.dimension() != m_dimension.id()) return false;
	auto &registry = m_scene.registry();
	if (registry.view<MinecartComponent>().size() >= 64) return false;
	auto held = player.heldItemStack();
	if (!held.amount() || !held.item().getGroupValue("group:om_minecart")) return false;
	Vector3d position = Vector3d(point) + retro::railPoint(railShape(*rail), {0.5,0.5,0}) + Vector3d{0,0,railHeight};
	DoubleBox box = cartBox(position);
	// slope support intentionally enters the cart's broad box, as in the beta rail projection.
	DoubleBox clearance = box;
	if (railShape(*rail) >= 2 && railShape(*rail) <= 5) clearance.z = point.z + 1 + railHeight;
	if (!clearBox(*this, clearance)) return false;
	bool occupied = false;
	registry.view<MinecartComponent, PositionComponent>().each([&](auto &, auto &other) {
		if (box.intersects(cartBox({other.x,other.y,other.z}))) occupied = true;
	});
	if (occupied) return false;
	auto entity = registry.create();
	registry.emplace<MinecartComponent>(entity);
	unsigned kind = held.item().getGroupValue("group:om_minecart");
	if (kind == 2 || kind == 3) registry.emplace<CargoMinecartComponent>(entity).kind = u8(kind - 1);
	registry.emplace<PositionComponent>(entity,position.x,position.y,position.z,m_dimension.id());
	auto &rotation = registry.emplace<RotationComponent>(entity);
	auto ends = retro::railEnds(railShape(*rail));
	Vector3d axis = ends[1] - ends[0];
	rotation.quat = glm::angleAxis(float(std::atan2(axis.y,axis.x)),glm::vec3(0,0,1));
	registry.emplace<DoubleBox>(entity,-0.49,-0.49,0,0.98,0.98,0.7);
	registry.emplace<NetworkComponent>(entity,entity);
	registry.emplace<DrawableDef>(entity).setMobModel(32, 1.f, kind > 1 ? u8(kind - 1) : 0);
	if (!player.isCreative()) player.inventory().setStack(u16(player.heldItemSlot()),0,
		held.amount() > 1 ? held.stringID() : "",held.amount() - 1);
	if (m_server) m_server->sendPlayerInvUpdate(player.clientID());
	return true;
}

bool ServerWorld::dismountMinecart(ServerPlayer &player) {
	auto &registry = m_scene.registry();
	entt::entity selected = entt::null;
	registry.view<MinecartComponent>().each([&](auto entity, auto &cart) {
		if (cart.rider == player.name()) selected = entity;
	});
	if (selected == entt::null) {
		if (player.isRidingMinecart()) sendRide(player,false);
		return true;
	}
	auto &cart = registry.get<MinecartComponent>(selected);
	const auto &position = registry.get<PositionComponent>(selected);
	for (int height : {0,1,-1}) for (auto direction : railDirections) {
		Vector3d target{std::floor(position.x) + 0.5 + direction.x,
			std::floor(position.y) + 0.5 + direction.y,std::floor(position.z) + height};
		DoubleBox body = player.hitbox(); body.move(target);
		if (!clearBox(*this,body)) continue;
		const auto environment=playerEnvironment(*this,body);
		if (!environment.loaded || environment.lava || environment.fire || environment.cactus || environment.submerged) continue;
		if (collision::move(*this,body,{0,0,-0.2},false).z < -0.19) continue;
		const auto *floor = getBlockState(int(std::floor(target.x)),int(std::floor(target.y)),int(std::floor(target.z - 0.1)));
		if (!floor || floor->block().getGroupValue("group:igniter") || floor->block().stringID() == "default:cactus") continue;
		cart.rider.clear();
		player.setPosition(target.x,target.y,target.z);
		player.survival().fallDistance = 0;
		sendRide(player,false);
		if (m_server) m_server->sendPlayerPosUpdate(player.clientID(),true);
		return true;
	}
	// keep the rider seated when every nearby exit is blocked or unloaded.
	cart.velocity = {0,0,0};
	return false;
}

bool ServerWorld::interactMinecart(ServerPlayer &player) {
	if (player.isRidingMinecart()) { dismountMinecart(player); return true; }
	auto target = cartTarget(*this,player);
	if (target == entt::null) return false;
	auto &cart = m_scene.registry().get<MinecartComponent>(target);
	if (auto *cargo = m_scene.registry().try_get<CargoMinecartComponent>(target)) {
		if (cargo->kind == 1) showCargoInventory(player, target, *cargo);
		else {
			auto held = player.heldItemStack();
			if (held.amount() && (held.stringID() == "default:coal" || held.stringID() == "default:charcoal") && cargo->addCoal()) {
				if (!player.isCreative()) player.inventory().setStack(u16(player.heldItemSlot()),0,
					held.amount() > 1 ? held.stringID() : "",held.amount() - 1);
				if (m_server) m_server->sendPlayerInvUpdate(player.clientID());
			}
			const auto &position = m_scene.registry().get<PositionComponent>(target);
			cargo->push = {position.x-player.x(),position.y-player.y(),0};
			double length = std::hypot(cargo->push.x,cargo->push.y);
			if (length > 0.01) cargo->push *= 1 / length;
		}
		return true;
	}
	if (!cart.rider.empty()) return true;
	auto &position = m_scene.registry().get<PositionComponent>(target);
	Vector3d seat{position.x,position.y,position.z + 0.15};
	DoubleBox body{seat.x-0.3,seat.y-0.3,seat.z+0.5,0.6,0.6,0.8};
	if (!clearBox(*this,body)) return true;
	cart.rider = player.name();
	player.setPosition(seat.x,seat.y,seat.z);
	player.survival().fallDistance = 0;
	sendRide(player,true);
	return true;
}

bool ServerWorld::attackMinecart(ServerPlayer &player) {
	auto target = cartTarget(*this,player);
	if (target == entt::null) return false;
	auto &registry = m_scene.registry();
	auto &cart = registry.get<MinecartComponent>(target);
	cart.damage += 20;
	double yaw = player.cameraYaw() * math::DEG_TO_RAD;
	cart.velocity.x += std::cos(yaw) * 0.08;
	cart.velocity.y += std::sin(yaw) * 0.08;
	if (cart.damage <= 40 && !player.isCreative()) return true;
	if (auto *rider = m_players.getPlayer(cart.rider)) {
		if (!dismountMinecart(*rider)) { cart.damage = 40; return true; }
	}
	const auto position = registry.get<PositionComponent>(target);
	if (const auto *cargo = registry.try_get<CargoMinecartComponent>(target)) {
		if (cargo->kind == 1) dropContainerItems({int(std::floor(position.x)),int(std::floor(position.y)),int(std::floor(position.z))},cargo->inventory);
		if (!player.isCreative()) dropWorldItem({position.x,position.y,position.z},cargo->kind == 1 ? "default:chest" : "default:furnace",1,0,false);
	}
	if (!player.isCreative()) dropWorldItem({position.x,position.y,position.z},"default:minecart",1,0,false);
	if (m_server) m_server->sendEntityDespawn(target);
	registry.destroy(target);
	return true;
}

void ServerWorld::updateMinecarts() {
	auto &registry = m_scene.registry();
	registry.view<MinecartComponent,PositionComponent,RotationComponent>().each([&](auto entity, auto &cart, auto &position, auto &rotation) {
		if (cart.damage) --cart.damage;
		Vector3d start{position.x,position.y,position.z};
		const Chunk *chunk = getChunkAtBlockPos(int(std::floor(start.x)),int(std::floor(start.y)),int(std::floor(start.z)));
		if (!chunk || !chunk->isInitialized()) return;
		auto *cargo = registry.try_get<CargoMinecartComponent>(entity);
		bool fueled = cargo && cargo->kind == 2 && cargo->fuel;
		if (cargo) registry.get<DrawableDef>(entity).setMobAppearance(u8(cargo->kind | (fueled ? 4 : 0)));
		ServerPlayer *rider = cart.rider.empty() ? nullptr : m_players.getPlayer(cart.rider);
		if (rider && (!rider->isOnline() || !rider->isReady() || !rider->health() || rider->dimension() != m_dimension.id())) {
			sendRide(*rider,false); cart.rider.clear(); rider = nullptr;
		}
		if (!rider) cart.rider.clear();
		for (const auto &entry : m_players) {
			const auto &player = entry.second;
			if (!player.isOnline() || !player.isReady() || !player.health() || player.isRidingMinecart() || player.dimension() != m_dimension.id()) continue;
			double dx = start.x-player.x(), dy = start.y-player.y(), distance = std::hypot(dx,dy);
			if (distance < 0.05 || distance > 0.9 || std::abs(start.z-player.z()) > 1) continue;
			cart.velocity.x += dx/distance*0.05;
			cart.velocity.y += dy/distance*0.05;
		}
		registry.view<MinecartComponent,PositionComponent>().each([&](auto otherEntity, auto &other, auto &otherPosition) {
			if (u32(otherEntity) <= u32(entity)) return;
			double dx = otherPosition.x-start.x, dy = otherPosition.y-start.y, distance = std::hypot(dx,dy);
			if (distance < 0.05 || distance > 0.98 || std::abs(otherPosition.z-start.z) > 0.7) return;
			glm::vec3 forward = rotation.quat * glm::vec3(1, 0, 0);
			if (std::abs((dx * forward.x + dy * forward.y) / distance) < 0.8) return;
			const auto *otherCargo = registry.try_get<CargoMinecartComponent>(otherEntity);
			bool furnace = cargo && cargo->kind == 2;
			bool otherFurnace = otherCargo && otherCargo->kind == 2;
			if (furnace != otherFurnace) {
				if (furnace) {
					other.velocity.x = other.velocity.x * 0.2 + cart.velocity.x + dx / distance * 0.05;
					other.velocity.y = other.velocity.y * 0.2 + cart.velocity.y + dy / distance * 0.05;
					cart.velocity *= 0.7;
				} else {
					cart.velocity.x = cart.velocity.x * 0.2 + other.velocity.x - dx / distance * 0.05;
					cart.velocity.y = cart.velocity.y * 0.2 + other.velocity.y - dy / distance * 0.05;
					other.velocity *= 0.7;
				}
				return;
			}
			double vx = (cart.velocity.x + other.velocity.x) * 0.5;
			double vy = (cart.velocity.y + other.velocity.y) * 0.5;
			cart.velocity.x = cart.velocity.x * 0.2 + vx - dx / distance * 0.05;
			cart.velocity.y = cart.velocity.y * 0.2 + vy - dy / distance * 0.05;
			other.velocity.x = other.velocity.x * 0.2 + vx + dx / distance * 0.05;
			other.velocity.y = other.velocity.y * 0.2 + vy + dy / distance * 0.05;
		});
		Vector3d next = start;
		Vector3i rail;
		if (const auto *state = findRail(*this,start,rail)) {
			unsigned shape = railShape(*state);
			start = Vector3d(rail) + retro::railPoint(shape,start-Vector3d(rail)) + Vector3d{0,0,railHeight};
			retro::alignCartVelocity(cart.velocity,shape);
			bool golden=state->block().getGroupValue("group:om_rail_type")==2;
			bool powered=golden && (state->id()&8);
			if (golden && !powered) {
				double speed = std::hypot(cart.velocity.x, cart.velocity.y);
				cart.velocity *= speed < 0.03 ? 0.0 : 0.5;
			}
			Vector3d movement = cart.velocity * (rider ? 0.75 : 1.0);
			movement.x = std::clamp(movement.x, -0.4, 0.4);
			movement.y = std::clamp(movement.y, -0.4, 0.4);
			next = Vector3d(rail) + retro::railPoint(shape,start+movement-Vector3d(rail)) + Vector3d{0,0,railHeight};
			Vector3i nextRail;
			const auto *nextState = findRail(*this,next,nextRail);
			if (nextState) next = Vector3d(nextRail) + retro::railPoint(railShape(*nextState),next-Vector3d(nextRail)) + Vector3d{0,0,railHeight};
			DoubleBox box = cartBox(start);
			if (shape >= 2 && shape <= 5) box.z = std::max(box.z,double(rail.z)+1+railHeight);
			if (nextState && railShape(*nextState) >= 2 && railShape(*nextState) <= 5) box.z = std::max(box.z,double(nextRail.z)+1+railHeight);
			Vector3d horizontal{next.x-start.x,next.y-start.y,0};
			Vector3d allowed = collision::move(*this,box,horizontal,false);
			if ((allowed-horizontal).length() > 0.001) { next = start; cart.velocity = {0,0,0}; }
			else {
				if (cargo && cargo->kind == 2) cargo->drive(cart.velocity);
				cart.velocity *= rider ? double(0.997f) : double(0.96f);
				double speed = std::hypot(cart.velocity.x,cart.velocity.y);
				if (speed > 0) {
					double adjusted = std::max(0.0,speed+(start.z-next.z)*0.05);
					cart.velocity *= adjusted/speed;
				}
				if (nextState && nextRail != rail) retro::alignCartVelocity(cart.velocity,railShape(*nextState),false);
				if (cargo) cargo->followTrack(cart.velocity);
				if (powered) {
					speed = std::hypot(cart.velocity.x, cart.velocity.y);
					if (speed > 0.01) cart.velocity *= (speed + 0.06) / speed;
					else if (shape < 2) {
						Vector3i axis = shape == 1 ? Vector3i{1,0,0} : Vector3i{0,1,0};
						for (int sign : {-1, 1}) {
							Vector3i end = rail + axis * sign;
							const auto *wall = loadedSupport(*this, end);
							if (wall && wall->isOpaque() && hasSolidTop(*this, end)) {
								cart.velocity = Vector3d(axis) * (-sign * 0.02);
								break;
							}
						}
					}
				}
			}
		} else {
			cart.velocity.x = std::clamp(cart.velocity.x,-0.4,0.4);
			cart.velocity.y = std::clamp(cart.velocity.y,-0.4,0.4);
			cart.velocity.z -= double(0.04f);
			bool grounded = collision::move(*this, cartBox(start), {0, 0, -0.001}, false).z > -0.001;
			if (grounded) cart.velocity *= 0.5;
			Vector3d allowed = collision::move(*this,cartBox(start),cart.velocity,false);
			next += allowed;
			bool ground = cart.velocity.z < 0 && allowed.z != cart.velocity.z;
			if (allowed.x != cart.velocity.x) cart.velocity.x = 0;
			if (allowed.y != cart.velocity.y) cart.velocity.y = 0;
			if (allowed.z != cart.velocity.z) cart.velocity.z = 0;
			if (!ground) cart.velocity *= double(0.95f);
		}
		if (rider) {
			DoubleBox body{start.x-0.3,start.y-0.3,start.z+0.65,0.6,0.6,0.8};
			Vector3d movement = next-start;
			if ((collision::move(*this,body,movement,false)-movement).length() > 0.001) { next=start; cart.velocity={0,0,0}; }
		}
		Vector3i detectorPoint;
		if (const auto *detector=findRail(*this,next,detectorPoint)) {
			if (detector->block().getGroupValue("group:om_rail_type")==3 && !(detector->id()&8)) {
				setBlockState(detectorPoint.x,detectorPoint.y,detectorPoint.z,u16(detector->id()|8));
				watchRailDevice(detectorPoint);
			}
		}
		position.isUpdated = position.isUpdated || position.x != next.x || position.y != next.y || position.z != next.z;
		position.x=next.x; position.y=next.y; position.z=next.z;
		Vector3d movement = next-start;
		double horizontal = std::hypot(movement.x,movement.y);
		if (horizontal > 0.0001) {
			rotation.quat = glm::angleAxis(float(std::atan2(movement.y,movement.x)),glm::vec3(0,0,1))
				* glm::angleAxis(float(-std::atan2(movement.z,horizontal)),glm::vec3(0,1,0));
			rotation.isUpdated=true;
		}
		if (rider) {
			rider->statistics().general[PlayerStatistics::CartCm] += u64(std::round(movement.length() * 100));
			rider->setPosition(next.x,next.y,next.z+0.15);
			rider->survival().fallDistance=0;
			sendRide(*rider,true);
		}
	});
}
