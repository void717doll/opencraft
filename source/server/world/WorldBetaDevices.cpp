#include "common/core/Registry.hpp"
#include "common/world/WorldCollision.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "server/core/PlayerList.hpp"
#include "common/world/BetaPiston.hpp"
#include "common/world/Dimension.hpp"
#include "common/world/RedstonePower.hpp"
#include "common/world/WorldBounds.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerPlayer.hpp"
#include "server/world/ServerWorld.hpp"

namespace {
bool movable(const ServerWorld &world, Vector3i point) {
	const auto *state = redstone::stateAt(world, point);
	if (!state || !worldBounds::contains(point.z) || world.getBlockData(point.x, point.y, point.z)) return false;
	const auto &block = state->block();
	if (state->hardness() < 0 || block.stringID() == "default:obsidian"
	 || block.stringID() == "default:bedrock" || block.hasGroup("group:om_piston_head")
	 || block.hasGroup("group:om_note_block") || block.hasGroup("group:om_chest") || block.hasGroup("group:om_sign")
	 || block.stringID() == "default:furnace" || block.stringID() == "default:blast_furnace"
	 || block.stringID() == "default:jukebox" || block.stringID() == "default:bed") return false;
	return !block.hasGroup("group:om_piston") || state->id() == 0;
}

bool movePistonEntities(ServerWorld &world, PlayerList &players, const std::vector<Vector3i> &moving,
	const std::vector<Vector3i> &occupied, Vector3i direction) {
	struct EntityMove { entt::entity entity; Vector3d movement; };
	struct PlayerMove { ServerPlayer *player; Vector3d movement; };
	std::vector<EntityMove> entities;
	std::vector<PlayerMove> playerMoves;
	auto displacement = [&](DoubleBox body, Vector3d &movement) {
		int axis = direction.x ? 0 : direction.y ? 1 : 2;
		double sign = direction.x + direction.y + direction.z;
		double low = axis == 0 ? body.x : axis == 1 ? body.y : body.z;
		double size = axis == 0 ? body.sizeX : axis == 1 ? body.sizeY : body.sizeZ;
		double distance = 0;
		for (auto point : occupied) {
			DoubleBox cell{double(point.x), double(point.y), double(point.z), 1, 1, 1};
			if (!body.intersects(cell)) continue;
			double edge = axis == 0 ? cell.x : axis == 1 ? cell.y : cell.z;
			distance = std::max(distance, sign > 0 ? edge + 1 - low : low + size - edge);
		}
		if (!distance) return true;
		movement = Vector3d(direction) * (distance + 0.00001);
		bool loaded;
		auto obstacles = collision::collectObstacles(world, body, movement, 0, loaded, false);
		if (!loaded) return false;
		obstacles.erase(std::remove_if(obstacles.begin(), obstacles.end(), [&](const auto &obstacle) {
			return !obstacle.unloaded && std::find(moving.begin(), moving.end(), obstacle.node) != moving.end();
		}), obstacles.end());
		auto allowed = collision::sweep(obstacles, body, movement, {0,0,0}, 1, 0, false, false).movement;
		if ((allowed - movement).length() > 0.000001) return false;
		body.move(movement);
		for (const auto &obstacle : obstacles) if (body.intersects(obstacle.box)) return false;
		return true;
	};
	for (auto &entry : players) {
		auto &player = entry.second;
		if (!player.isOnline() || !player.health() || player.isNoclip() || player.dimension() != world.dimension().id()) continue;
		DoubleBox body = player.hitbox();
		body.move(player.x(), player.y(), player.z());
		Vector3d movement{0,0,0};
		if (!displacement(body, movement)) return false;
		if (!movement.isZero()) playerMoves.push_back({&player, movement});
	}
	auto &registry = world.scene().registry();
	bool clear = true;
	registry.view<PositionComponent, DoubleBox>().each([&](auto entity, const auto &position, const auto &localBox) {
		if (!clear) return;
		DoubleBox body = localBox;
		body.move(position.x, position.y, position.z);
		Vector3d movement{0,0,0};
		if (!displacement(body, movement)) { clear = false; return; }
		if (!movement.isZero()) entities.push_back({entity, movement});
	});
	if (!clear) return false;
	// commit entity positions only after every destination can be reached without crossing a wall.
	for (const auto &entry : playerMoves) {
		auto &player = *entry.player;
		player.setPosition(player.x()+entry.movement.x, player.y()+entry.movement.y, player.z()+entry.movement.z);
		if (entry.movement.z > 0) player.survival().fallDistance = 0;
		if (world.server()) world.server()->sendPlayerPosUpdate(player.clientID(), true);
	}
	for (const auto &entry : entities) {
		auto &position = registry.get<PositionComponent>(entry.entity);
		position.x += entry.movement.x; position.y += entry.movement.y; position.z += entry.movement.z;
		position.isUpdated = true;
	}
	return true;
}

void copyBlock(ServerWorld &world, Vector3i from, Vector3i to) {
	u16 id = world.getBlock(from.x, from.y, from.z), data = world.getData(from.x, from.y, from.z);
	world.setData(to.x, to.y, to.z, data);
	world.setBlock(to.x, to.y, to.z, id);
	world.setBlock(from.x, from.y, from.z, 0);
	world.setData(from.x, from.y, from.z, 0);
}
}

void ServerWorld::playNote(int x, int y, int z, bool tune) {
	Vector3i point{x,y,z};
	const auto *state = redstone::stateAt(*this, point);
	if (!state || !state->block().hasGroup("group:om_note_block")) return;
	auto *data = addBlockData(x,y,z,0,0);
	int note = data->meta.hasInt("note") ? std::clamp(data->meta.get<int>("note"),0,24) : 0;
	if (tune) note = (note + 1) % 25;
	data->meta.setInt("note",note);
	static_cast<ServerChunk *>(getChunkAtBlockPos(x,y,z))->setModified(true);
	const auto *above = redstone::stateAt(*this, point + Vector3i{0,0,1});
	const auto *below = redstone::stateAt(*this, point + Vector3i{0,0,-1});
	if (!above || above->block().id() || !m_server) return;
	std::string instrument = "piano";
	if (below) {
		const auto &block = below->block();
		if (block.hasGroup("group:om_material_wood")) instrument = "bass";
		else if (block.hasGroup("group:om_material_sand")) instrument = "snare";
		else if (block.hasGroup("group:om_material_glass")) instrument = "hat";
		else if (block.hasGroup("group:om_material_stone")) instrument = "drum";
	}
	m_server->sendSound("note_" + instrument,{x+0.5,y+0.5,z+0.5},m_dimension.id(),1.f,std::pow(2.f,float(note-12)/12.f));
	if (tune) m_server->sendParticles({x+0.5,y+0.5,z+1.1},m_dimension.id(),1);
}

void ServerWorld::removePiston(int x, int y, int z) {
	// the destroyed base has already been removed; locate its matching head.
	Vector3i point{x,y,z};
	for (auto side : redstone::sides) {
		Vector3i head = point + side;
		const auto *state = redstone::stateAt(*this,head);
		if (!state || !state->block().hasGroup("group:om_piston_head")) continue;
		if (retro::pistonDirection(redstone::rotation(*this,head,*state)) == side)
			setBlock(head.x,head.y,head.z,0);
	}
}

void ServerWorld::breakPistonHead(int x, int y, int z) {
	if (m_movingPiston) return;
	auto &headBlock = Registry::getInstance().getBlockFromStringID("default:piston_head");
	Vector3i direction = retro::pistonDirection(headBlock.param().getParam(BlockParam::Rotation,getData(x,y,z)));
	Vector3i base = Vector3i{x,y,z} - direction;
	const auto *state = redstone::stateAt(*this,base);
	if (!state || !state->block().hasGroup("group:om_piston") || !state->id()
	 || retro::pistonDirection(redstone::rotation(*this,base,*state)) != direction) return;
	std::string item = state->block().stringID();
	setBlock(base.x,base.y,base.z,0);
	if (!isCreative()) dropWorldItem({base.x+0.5,base.y+0.5,base.z+0.5},item,1,0,false);
}

void ServerWorld::refreshBetaDevice(const Vector3i &point) {
	const auto *state = redstone::stateAt(*this,point);
	if (!state) return;
	if (state->block().hasGroup("group:om_note_block")) {
		auto *data = addBlockData(point.x,point.y,point.z,0,0);
		bool powered = redstonePower(point) != 0;
		bool previous = data->meta.hasInt("note_power") && data->meta.get<int>("note_power");
		if (powered == previous) return;
		data->meta.setInt("note_power",powered);
		static_cast<ServerChunk *>(getChunkAtBlockPos(point.x,point.y,point.z))->setModified(true);
		if (powered) playNote(point.x,point.y,point.z,false);
		return;
	}
	if (state->block().hasGroup("group:om_piston_head")) {
		Vector3i direction = retro::pistonDirection(redstone::rotation(*this,point,*state));
		Vector3i base = point - direction;
		const auto *parent = redstone::stateAt(*this,base);
		if (parent && (!parent->block().hasGroup("group:om_piston") || !parent->id()
		 || retro::pistonDirection(redstone::rotation(*this,base,*parent)) != direction)) setBlock(point.x,point.y,point.z,0);
		return;
	}
	unsigned kind = state->block().getGroupValue("group:om_piston");
	if (!kind) return;
	u16 rotation = u16(redstone::rotation(*this,point,*state));
	Vector3i direction = retro::pistonDirection(rotation), head = point + direction;
	bool powered = false;
	for (auto side : redstone::sides) {
		if (side != direction && redstone::inputFrom(*this,point,point+side)) { powered = true; break; }
	}
	// beta also checks the power neighborhood one block above the base.
	Vector3i above = point + Vector3i{0,0,1};
	if (!powered) for (auto side : redstone::sides) {
		if (side != Vector3i{0,0,-1} && redstone::inputFrom(*this,above,above+side)) { powered = true; break; }
	}
	bool extended = state->id() != 0;
	if (powered == extended) return;
	// blocked pushes retry until entities or obstructions move; settled pistons use neighbor events.
	watchRailDevice(point);
	auto &definitions = Registry::getInstance();
	if (!definitions.hasBlock("default:piston_head")) return;
	if (powered) {
		Vector3i end = head;
		unsigned count = 0;
		std::vector<ItemStack> fragileDrops;
		while (true) {
			const auto *next = redstone::stateAt(*this,end);
			if (!next || !worldBounds::contains(end.z)) return;
			if (!next->block().id() || next->drawType() == BlockDrawType::Liquid) break;
			bool fragileCircuit = next->block().hasGroup("group:om_power_source") || next->block().hasGroup("group:om_rail");
			if (fragileCircuit || ((!next->isCollidable() || next->drawType() == BlockDrawType::XShape)
			 && !next->block().hasGroup("group:om_sign") && !next->block().hasGroup("group:om_painting"))) {
				fragileDrops = next->getItemDrops();
				break;
			}
			if (!movable(*this,end)) return;
			if (++count > 12) return;
			end += direction;
		}
		std::vector<Vector3i> moving;
		for (Vector3i cell = head;; cell += direction) {
			moving.push_back(cell);
			if (cell == end) break;
		}
		if (!movePistonEntities(*this, m_players, moving, moving, direction)) return;
		// block changes are applied only after the entire loaded push line is valid.
		m_movingPiston = true;
		for (const auto &drop : fragileDrops)
			dropWorldItem({end.x+0.5,end.y+0.5,end.z+0.5},drop.stringID(),drop.amount(),drop.damage(),false);
		setBlock(end.x,end.y,end.z,0);
		while (end != head) {
			Vector3i from = end - direction;
			copyBlock(*this,from,end);
			end = from;
		}
		auto &headBlock = definitions.getBlockFromStringID("default:piston_head");
		u16 data = headBlock.param().setParam(BlockParam::Rotation,0,rotation);
		data = headBlock.param().setParam(BlockParam::State,data,kind == 2 ? 1 : 0);
		setData(head.x,head.y,head.z,data);
		setBlock(head.x,head.y,head.z,headBlock.id());
		setBlockState(point.x,point.y,point.z,1);
	} else {
		const auto *front = redstone::stateAt(*this,head);
		if (!front) return;
		m_movingPiston = true;
		if (front->block().hasGroup("group:om_piston_head")) setBlock(head.x,head.y,head.z,0);
		setBlockState(point.x,point.y,point.z,0);
		Vector3i pull = head + direction;
		const auto *target = redstone::stateAt(*this,pull);
		if (kind == 2 && !getBlock(head.x,head.y,head.z) && target && target->block().id()
		 && target->isCollidable() && movable(*this,pull)
		 && movePistonEntities(*this, m_players, {head, pull}, {head}, Vector3i{} - direction)) copyBlock(*this,pull,head);
	}
	m_movingPiston = false;
	if (m_server) m_server->sendSound("default_wood_footstep",{point.x+0.5,point.y+0.5,point.z+0.5},m_dimension.id(),0.6f,powered?0.8f:0.65f);
}
