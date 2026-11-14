// SPDX-License-Identifier: LGPL-2.1-or-later
// adapted from lunarchy/luanti builtin/game/falling.lua and src/servermap.cpp.
// liquid algorithm copyright (c) 2010-2024 celeron55, perttu ahola and contributors.
#include "common/world/BlockSupport.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "common/core/BlockGeometry.hpp"
#include "common/core/Registry.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/FallingBlockComponent.hpp"
#include "common/scene/component/NetworkComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/world/Dimension.hpp"
#include "common/world/WorldCollision.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/BlockPhysics.hpp"
#include "server/world/ServerChunk.hpp"
#include "server/world/ServerWorld.hpp"

namespace {
const std::array<Vector3i, 6> directions{{{0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, -1, 0}, {0, 1, 0}, {0, 0, -1}}};

int fluidKind(const Block &block) {
	if (block.getGroupValue("group:om_waterlogged")) return 1;
	const auto &name = block.stringID();
	if (name == "default:water" || name == "default:water_flowing") return 1;
	if (name == "default:lava" || name == "default:lava_flowing") return 2;
	return 0;
}

bool source(const Block &block) {
	return block.getGroupValue("group:om_waterlogged") || block.stringID() == "default:water" || block.stringID() == "default:lava";
}

u16 fluidParam(const Block &block, u16 data) {
	return block.param().hasParam(BlockParam::Custom) ? block.param().getParam(BlockParam::Custom, data) : 0;
}

bool falls(const Block &block) {
	return block.getGroupValue("group:falling_node") || block.stringID() == "default:sand" || block.stringID() == "default:gravel";
}

u16 blockID(const char *id) {
	auto &definitions = Registry::getInstance();
	return definitions.hasBlock(id) ? definitions.getBlockFromStringID(id).id() : 0;
}
}

bool BlockPhysics::loaded(const Vector3i &position) const {
	const Chunk *chunk = m_world.getChunkAtBlockPos(position.x, position.y, position.z);
	return chunk && chunk->isInitialized();
}

bool BlockPhysics::replaceable(const Vector3i &position) const {
	if (!loaded(position)) return false;
	const auto *state = m_world.getBlockState(position.x, position.y, position.z);
	return state && !state->block().hasGroup("group:om_amethyst_cluster")
		&& (!state->block().id() || !state->isCollidable());
}

void BlockPhysics::enqueue(const Vector3i &position) {
	if (loaded(position) && m_pending.emplace(position).second) m_queue.push_back(position);
}

void BlockPhysics::enqueueNeighbors(const Vector3i &position) {
	m_world.notifyRedstone(position);
	// the source falling-node walk visits support and sides before the block above.
	enqueue(position + Vector3i{0, 0, -1});
	for (int i = 1; i <= 4; ++i) enqueue(position + directions[i]);
	enqueue(position);
	enqueue(position + Vector3i{0, 0, 1});
	// power can pass through one solid support block.
	for (auto a:directions) for (auto b:directions) {
		Vector3i neighbor=position+a+b;
		const auto *state=m_world.getBlockState(neighbor.x,neighbor.y,neighbor.z);
		if (state && (state->block().getGroupValue("group:om_rail_type")
		 || state->block().getGroupValue("group:om_power_source"))) enqueue(neighbor);
	}
	for (int i = 1; i <= 4; ++i) for (int height : {-1, 1}) {
		Vector3i neighbor = position + directions[i] + Vector3i{0, 0, height};
		const auto *state = m_world.getBlockState(neighbor.x, neighbor.y, neighbor.z);
		if (state && (state->block().getGroupValue("group:om_rail")
		 || (height == 1 && state->block().stringID() == "default:reeds"))) enqueue(neighbor);
	}
}

// beta 1.7.3 connects leaves to logs through at most four face-adjacent leaves.
void BlockPhysics::markLeaves(const Vector3i &position, int radius) {
	for (int z=-radius; z<=radius; ++z) for (int y=-radius; y<=radius; ++y) for (int x=-radius; x<=radius; ++x) {
		Vector3i point = position + Vector3i{x,y,z};
		if (!loaded(point)) continue;
		const auto &block = m_world.getBlockState(point.x,point.y,point.z)->block();
		if (block.getState(0).drawType() != BlockDrawType::Leaves || !block.param().hasParam(BlockParam::Custom)) continue;
		u16 data = m_world.getData(point.x,point.y,point.z);
		u16 flags = block.param().getParam(BlockParam::Custom,data);
		m_world.setData(point.x,point.y,point.z,block.param().setParam(BlockParam::Custom,data,flags | 1));
		enqueue(point);
	}
}

void BlockPhysics::updateLeaves(const Vector3i &position) {
	const auto &block = m_world.getBlockState(position.x,position.y,position.z)->block();
	if (!block.param().hasParam(BlockParam::Custom)) return;
	u16 data = m_world.getData(position.x,position.y,position.z);
	u16 flags = block.param().getParam(BlockParam::Custom,data);
	if (!(flags & 1)) return;
	for (int z : {-5,5}) for (int y : {-5,5}) for (int x : {-5,5}) {
		Vector3i corner = position + Vector3i{x,y,z};
		corner.z = std::clamp(corner.z, worldBounds::minHeight, worldBounds::height-1);
		if (!loaded(corner)) {
			m_waitingLeaves[{(corner.x & -CHUNK_WIDTH)/CHUNK_WIDTH, (corner.y & -CHUNK_DEPTH)/CHUNK_DEPTH, (corner.z & -CHUNK_HEIGHT)/CHUNK_HEIGHT}].emplace(position);
			return;
		}
	}
	std::vector<Vector3i> frontier{position};
	std::array<bool,729> visited{};
	auto index = [&](const Vector3i &point) {
		Vector3i offset = point - position + Vector3i{4,4,4};
		return std::size_t(offset.x + offset.y*9 + offset.z*81);
	};
	visited[index(position)] = true;
	for (int distance=1; distance<=4; ++distance) {
		std::vector<Vector3i> next;
		for (const auto &point : frontier) for (const auto &side : directions) {
			Vector3i neighbor = point + side;
			if (!worldBounds::contains(neighbor.z)) continue;
			const auto &candidate = m_world.getBlockState(neighbor.x,neighbor.y,neighbor.z)->block();
			const auto &id = candidate.stringID();
			if (candidate.hasGroup("group:om_material_wood") && id.size() >= 5 && id.compare(id.size()-5,5,"_wood") == 0) {
				m_world.setData(position.x,position.y,position.z,block.param().setParam(BlockParam::Custom,data,flags & ~1));
				return;
			}
			if (candidate.getState(0).drawType() != BlockDrawType::Leaves || visited[index(neighbor)]) continue;
			visited[index(neighbor)] = true; next.push_back(neighbor);
		}
		frontier = std::move(next);
	}
	dropAttached(position);
}

void BlockPhysics::seedNode(const Vector3i &position) {
	if (!loaded(position)) return;
	const Block &block = m_world.getBlockState(position.x, position.y, position.z)->block();
	if (falls(block) && replaceable(position + Vector3i{0, 0, -1})) enqueue(position);
	if (block.getGroupValue("group:attached_node")) enqueue(position);
	if (block.getState(0).drawType() == BlockDrawType::Leaves) enqueue(position);
	if (block.getGroupValue("group:om_power_source") || block.getGroupValue("group:om_redstone_device")
	 || block.stringID() == "default:tnt") m_world.queueRedstone(position);
	int kind = fluidKind(block);
	if (!kind) return;
	if (!source(block)) { enqueue(position); return; }
	for (int i = 0; i < 6; ++i) {
		Vector3i neighbor = position + directions[i];
		if (!loaded(neighbor)) continue;
		const Block &other = m_world.getBlockState(neighbor.x, neighbor.y, neighbor.z)->block();
		int otherKind = fluidKind(other);
		bool interaction = otherKind && otherKind != kind;
		bool canFlow = i != 0 && ((!otherKind && replaceable(neighbor)) || (otherKind == kind && !source(other)));
		if (interaction || canFlow) { enqueue(position); enqueue(neighbor); }
	}
}

void BlockPhysics::seedChunk(ServerChunk &chunk) {
	if (!chunk.isInitialized()) return;
	auto waiting = m_waitingLeaves.find({chunk.x(), chunk.y(), chunk.z()});
	if (waiting != m_waitingLeaves.end()) {
		for (const auto &position : waiting->second) enqueue(position);
		m_waitingLeaves.erase(waiting);
	}
	const auto &blocks = Registry::getInstance().blocks();
	std::vector<bool> active(blocks.size());
	for (const auto &entry : blocks) {
		const Block &block = *entry;
		active[block.id()] = falls(block) || fluidKind(block) || block.getGroupValue("group:attached_node")
			|| block.getGroupValue("group:om_power_source") || block.getGroupValue("group:om_redstone_device")
			|| block.stringID() == "default:tnt" || block.getState(0).drawType() == BlockDrawType::Leaves;
	}
	auto seed = [&](int x, int y, int z) {
		if (active[chunk.getBlock(x, y, z)])
			seedNode({x + chunk.x() * CHUNK_WIDTH, y + chunk.y() * CHUNK_DEPTH, z + chunk.z() * CHUNK_HEIGHT});
	};
	for (int z = 0; z < CHUNK_HEIGHT; ++z) {
		for (int y = 0; y < CHUNK_DEPTH; ++y) {
			for (int x = 0; x < CHUNK_WIDTH; ++x) {
				seed(x, y, z);
				// only relevant nodes outside the new chunk need waking at its boundary.
				if (!x) seed(x - 1, y, z);
				if (x == CHUNK_WIDTH - 1) seed(x + 1, y, z);
				if (!y) seed(x, y - 1, z);
				if (y == CHUNK_DEPTH - 1) seed(x, y + 1, z);
				if (!z) seed(x, y, z - 1);
				if (z == CHUNK_HEIGHT - 1) seed(x, y, z + 1);
			}
		}
	}
}

void BlockPhysics::forgetChunk(const Vector3i &position) {
	auto belongs = [&](const Vector3i &point) {
		return Vector3i{(point.x & -CHUNK_WIDTH)/CHUNK_WIDTH, (point.y & -CHUNK_DEPTH)/CHUNK_DEPTH,
			(point.z & -CHUNK_HEIGHT)/CHUNK_HEIGHT} == position;
	};
	m_queue.erase(std::remove_if(m_queue.begin(),m_queue.end(),[&](const Vector3i &point) {
		if (!belongs(point)) return false;
		m_pending.erase(point); return true;
	}),m_queue.end());
	for (auto entry=m_waitingLeaves.begin(); entry!=m_waitingLeaves.end();) {
		for (auto leaf=entry->second.begin(); leaf!=entry->second.end();) {
			if (belongs(*leaf)) leaf=entry->second.erase(leaf); else ++leaf;
		}
		if (entry->second.empty()) entry=m_waitingLeaves.erase(entry); else ++entry;
	}
}

void BlockPhysics::write(const Vector3i &position, u16 block, u16 data) {
	if (!loaded(position)) return;
	m_world.setData(position.x, position.y, position.z, 0);
	m_world.setBlock(position.x, position.y, position.z, block);
	m_world.setData(position.x, position.y, position.z, data);
	static_cast<ServerChunk *>(m_world.getChunkAtBlockPos(position.x, position.y, position.z))->setModified(true);
	enqueueNeighbors(position);
}

std::size_t BlockPhysics::tick(std::size_t budget) {
	std::size_t count = std::min(budget, m_queue.size());
	for (std::size_t i = 0; i < count; ++i) {
		Vector3i position = m_queue.front();
		m_queue.pop_front();
		m_pending.erase(position);
		if (loaded(position)) update(position);
	}
	updateFalling();
	return count;
}

void BlockPhysics::update(const Vector3i &position) {
	const auto *state = m_world.getBlockState(position.x, position.y, position.z);
	if (!state) return;
	const Block &block = state->block();
	if (state->drawType() == BlockDrawType::Leaves) { updateLeaves(position); return; }
	if (falls(block) && replaceable(position + Vector3i{0, 0, -1})) {
		startFalling(position);
		return;
	}

	// fences and chests have placement rules, but do not break when those conditions change.
	if (block.id() && !block.hasGroup("group:fence") && !block.hasGroup("group:om_chest")
	 && (!fluidKind(block) || block.getGroupValue("group:om_waterlogged"))) {
		bool neighborsLoaded = std::all_of(directions.begin(), directions.end(), [&](const auto &offset) { return loaded(position + offset); });
		if (block.hasGroup("group:om_rope")) neighborsLoaded = loaded(position + Vector3i{0,0,1});
		if (block.hasGroup("group:om_soil_plant") || block.hasGroup("group:om_sapling")) neighborsLoaded = loaded(position + Vector3i{0,0,-1});
		if (block.stringID() == "default:reeds" || block.stringID() == "default:cactus") {
			Vector3i below = position + Vector3i{0,0,-1};
			neighborsLoaded = loaded(below);
			const auto *floor = neighborsLoaded ? m_world.getBlockState(below.x,below.y,below.z) : nullptr;
			const std::string floorID = floor ? floor->block().stringID() : "";
			bool checkSides = block.stringID() == "default:cactus"
				? floorID == "default:cactus" || floorID == "default:sand"
				: floorID == "default:grass" || floorID == "default:dirt";
			if (floor && checkSides) {
				for (int side = 1; side <= 4; ++side)
					neighborsLoaded = neighborsLoaded && loaded((block.stringID() == "default:cactus" ? position : below) + directions[side]);
			}
		}
		if (neighborsLoaded) {
			bool supported = blockHasSupport(m_world, position, *state,
				m_world.getData(position.x, position.y, position.z));
			if (!supported) { dropAttached(position); return; }
		}
	}
	if (block.getGroupValue("group:om_rail")) m_world.refreshRail(position);
	if (block.getGroupValue("group:om_rail_type")==3 || block.getGroupValue("group:om_power_source")==1
	 || block.getGroupValue("group:om_power_source")==3) m_world.watchRailDevice(position);
	updateFluid(position);
}

void BlockPhysics::dropAttached(const Vector3i &position) {
	const auto *state = m_world.getBlockState(position.x, position.y, position.z);
	if (!state || !state->block().id()) return;
	const auto drops = state->getItemDrops();
	if (state->block().stringID() == "default:bed") {
		// the partner callback needs the old half and orientation bits.
		m_world.setBlock(position.x, position.y, position.z, 0);
		m_world.setData(position.x, position.y, position.z, 0);
	}
	else write(position, 0);
	for (const auto &drop : drops)
		m_world.dropWorldItem({position.x + 0.5, position.y + 0.5, double(position.z)}, drop.stringID(), drop.amount());
}

void BlockPhysics::startFalling(const Vector3i &position) {
	auto &registry = m_world.scene().registry();
	if (registry.view<FallingBlockComponent>().size() >= 256) { enqueue(position); return; }
	u16 block = m_world.getBlock(position.x, position.y, position.z);
	entt::entity entity = registry.create();
	auto &falling = registry.emplace<FallingBlockComponent>(entity);
	falling.block = block;
	falling.data = m_world.getData(position.x, position.y, position.z);
	falling.origin = position;
	if (const auto *data = m_world.getBlockData(position.x, position.y, position.z)) falling.blockData = *data;
	registry.emplace<PositionComponent>(entity, position.x + 0.5, position.y + 0.5, double(position.z), m_world.dimension().id());
	registry.emplace<NetworkComponent>(entity, entity);
	auto &cube = registry.emplace<DrawableDef>(entity).addInventoryCube();
	cube.blockID = Registry::getInstance().getBlock(block).stringID();
	cube.size = 1;
	cube.origin = {-0.5f, -0.5f, 0};
	write(position, 0);
}

void BlockPhysics::updateFalling() {
	auto &registry = m_world.scene().registry();
	std::vector<entt::entity> finished;
	registry.view<FallingBlockComponent, PositionComponent>().each([&](auto entity, auto &falling, auto &position) {
		Vector3i current{int(std::floor(position.x)), int(std::floor(position.y)), int(std::floor(position.z))};
		if (!loaded(current) || !loaded(current + Vector3i{0, 0, -1})) return;
		DoubleBox box{position.x - 0.49, position.y - 0.49, position.z, 0.98, 0.98, 0.98};
		auto result = collision::step(m_world, box, {falling.velocity.x, falling.velocity.y, falling.velocity.z},
			{0, 0, -9.81 * m_world.dimension().gravity()}, 0.05, 0, false);
		falling.velocity = {float(result.velocity.x), float(result.velocity.y), float(result.velocity.z)};
		position.x += result.movement.x;
		position.y += result.movement.y;
		position.z += result.movement.z;
		position.isUpdated = true;
		if (!result.grounded) return;
		Vector3i landing{int(std::floor(position.x)), int(std::floor(position.y)), int(std::floor(position.z + 0.5))};
		if (!loaded(landing) || !loaded(landing + Vector3i{0, 0, -1})) return;
		if (replaceable(landing)) {
			const Block &oldBlock = m_world.getBlockState(landing.x, landing.y, landing.z)->block();
			if (oldBlock.id() && !fluidKind(oldBlock)) dropAttached(landing);
			write(landing, falling.block, falling.data);
			if (falling.blockData) {
				auto *data = m_world.addBlockData(landing.x, landing.y, landing.z);
				if (data) {
					*data = *falling.blockData;
					data->inventory.setBlockPos(landing);
				}
			}
		}
		else {
			const auto &block = Registry::getInstance().getBlock(falling.block);
			m_world.dropWorldItem({position.x, position.y, position.z}, block.stringID(), 1);
		}
		finished.push_back(entity);
	});
	for (auto entity : finished) {
		if (m_world.server()) m_world.server()->sendEntityDespawn(entity, m_world.dimension().id());
		registry.destroy(entity);
	}
}

void BlockPhysics::updateFluid(const Vector3i &position) {
	const auto *state = m_world.getBlockState(position.x, position.y, position.z);
	if (!state) return;
	const Block &block = state->block();
	if (block.getGroupValue("group:om_waterlogged")) return;
	int kind = fluidKind(block);
	if (!kind && !replaceable(position)) return;
	bool isSource = source(block);
	u16 floodableBlock = fluidKind(block) ? 0 : block.id();
	int oldLevel = kind ? (isSource ? 8 : int(fluidParam(block, m_world.getData(position.x, position.y, position.z)) & 7)) : -1;
	struct Neighbor { Vector3i position; const Block *block; int kind; int level; bool down; bool loaded; };
	std::array<Neighbor, 6> neighbors;
	bool unknownSource = false, flowingDown = false;
	for (int i = 0; i < 6; ++i) {
		auto &neighbor = neighbors[i];
		neighbor.position = position + directions[i];
		neighbor.loaded = loaded(neighbor.position);
		neighbor.block = neighbor.loaded ? &m_world.getBlockState(neighbor.position.x, neighbor.position.y, neighbor.position.z)->block() : nullptr;
		neighbor.kind = neighbor.block ? fluidKind(*neighbor.block) : 0;
		u16 param = neighbor.block ? fluidParam(*neighbor.block, m_world.getData(neighbor.position.x, neighbor.position.y, neighbor.position.z)) : 0;
		neighbor.level = neighbor.block && source(*neighbor.block) ? 8 : param & 7;
		neighbor.down = param & 8;
		if (!neighbor.loaded) { if (i == 5) flowingDown = true; else unknownSource = true; }
		else if (i == 5 && (replaceable(neighbor.position) || (neighbor.kind && !source(*neighbor.block)))) flowingDown = true;
		if (!kind && neighbor.kind && i != 5 && (i == 0 || !neighbor.down)) kind = neighbor.kind;
	}
	if (!kind) return;

	if (kind == 2 && fluidKind(block) == 2) {
		bool touchingWater = std::any_of(neighbors.begin(), neighbors.end(), [](const auto &neighbor) { return neighbor.kind == 1; });
		if (touchingWater) {
			u16 result = blockID(isSource ? "default:obsidian" : "default:cobblestone");
			if (result) write(position, result);
			return;
		}
	}
	if (fluidKind(block) == 1 && neighbors[0].kind == 2) {
		u16 stone = blockID("default:stone");
		if (stone) write(position, stone);
		return;
	}

	u16 flowingID = blockID(kind == 1 ? "default:water_flowing" : "default:lava_flowing");
	u16 sourceID = blockID(kind == 1 ? "default:water" : "default:lava");
	if (!flowingID || !sourceID) return;
	const Block &flowingBlock = Registry::getInstance().getBlock(flowingID);
	if (!flowingBlock.param().hasParam(BlockParam::Custom)) return;
	bool renewable = flowingBlock.hasGroup("group:liquid_renewable") ? flowingBlock.getGroupValue("group:liquid_renewable") > 0 : kind == 1;
	int sources = 0, maxLevel = -1;
	for (int i = 0; i < 5; ++i) {
		const auto &neighbor = neighbors[i];
		if (neighbor.kind != kind) continue;
		if (source(*neighbor.block)) { ++sources; maxLevel = 7; }
		else if (i == 0) maxLevel = std::max(maxLevel, std::min(7, neighbor.level + 4));
		else if (!neighbor.down && neighbor.level > 0) maxLevel = std::max(maxLevel, neighbor.level - 1);
	}
	u16 newBlock = flowingID;
	int newLevel = maxLevel;
	if (isSource || (sources >= 2 && renewable)) { newBlock = sourceID; newLevel = -1; }
	else if (sources) newLevel = 7;
	else if (unknownSource && oldLevel >= 0) newLevel = oldLevel;
	else {
		int viscosity = flowingBlock.hasGroup("group:liquid_viscosity") ? flowingBlock.getGroupValue("group:liquid_viscosity") : kind == 2 ? 3 : 1;
		if (viscosity > 1 && maxLevel != oldLevel) {
			int difference = maxLevel - oldLevel;
			int increment = std::abs(difference) > viscosity ? difference / viscosity : difference > 0 ? 1 : -1;
			newLevel = oldLevel + increment;
			if (newLevel != maxLevel) enqueue(position);
		}
	}
	int range = flowingBlock.hasGroup("group:liquid_range") ? flowingBlock.getGroupValue("group:liquid_range") : 8;
	if (!isSource && newBlock != sourceID && (maxLevel < 8 - std::min(range, 8)) && !(unknownSource && oldLevel >= 0)) newBlock = floodableBlock;
	if (newLevel < 0 && newBlock != sourceID) newBlock = floodableBlock;
	u16 data = newBlock == floodableBlock && floodableBlock ? m_world.getData(position.x, position.y, position.z) : 0;
	if (newBlock == flowingID) {
		data = block.id() == newBlock ? m_world.getData(position.x, position.y, position.z) : 0;
		data = flowingBlock.param().setParam(BlockParam::Custom, data, u16(newLevel & 7) | (flowingDown ? 8 : 0));
	}
	if (block.id() != newBlock || m_world.getData(position.x, position.y, position.z) != data) {
		if (block.id() && !fluidKind(block) && newBlock != block.id()) dropAttached(position);
		write(position, newBlock, data);
	}
	else if (isSource) {
		for (int i = 1; i < 6; ++i) {
			if (neighbors[i].loaded && replaceable(neighbors[i].position) && neighbors[i].kind != kind) enqueue(neighbors[i].position);
		}
	}
}
