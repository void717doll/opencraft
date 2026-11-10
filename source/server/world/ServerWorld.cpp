/*
 * =====================================================================================
 *
 *  OpenMiner
 *
 *  Copyright (C) 2018-2020 Unarelith, Quentin Bazin <openminer@unarelith.net>
 *  Copyright (C) 2019-2020 the OpenMiner contributors (see CONTRIBUTORS.md)
 *
 *  This file is part of OpenMiner.
 *
 *  OpenMiner is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  OpenMiner is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with OpenMiner; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * =====================================================================================
 */
#include "common/core/EngineConfig.hpp"
#include "common/core/GameClock.hpp"
#include "common/core/GameTime.hpp"
#include "common/scene/component/MobComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/world/Dimension.hpp"
#include "common/world/Chest.hpp"
#include "server/core/PlayerList.hpp"
#include "server/core/ServerConfig.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/network/Server.hpp"
#include "server/world/ServerPlayer.hpp"
#include <array>
#include <chrono>

#include "server/world/ServerWorld.hpp"
#include "common/world/BetaPlayerMovement.hpp"

ServerWorld::ServerWorld(PlayerList &players, const Dimension &dimension, s32 seed)
	: m_players(players),
	  m_dimension(dimension),
	  m_heightmap(seed, dimension.id() == 1),
	  m_terrainGenerator(m_heightmap, dimension, seed),
	  m_scene(players),
	  m_seed(seed)
{
}

void ServerWorld::update(bool doTick) {
	World::update();

	processSendRequests();

	if (doTick) {
		tickActiveChunks();

		m_blockPhysics.tick();
		updateMobs();
		updateItems();
		updateTnt();
		updateMinecarts();
		updateRailDevices();
		updateRedstoneCircuits();
		updatePlayerSurvival();
		m_scene.update();
	}
}

void ServerWorld::initializeChunkLighting(ServerChunk &chunk) {
	chunk.lightmap().clearPendingUpdates();
	for (int z = 0; z < CHUNK_HEIGHT; ++z) for (int y = 0; y < CHUNK_DEPTH; ++y) for (int x = 0; x < CHUNK_WIDTH; ++x) {
		chunk.lightmap().setLightDataRaw(x, y, z, 0);
		if (u8 emission = Chunk::lightEmission(*chunk.getBlockState(x, y, z)))
			chunk.lightmap().addTorchlight(x, y, z, emission);
	}
	if (m_dimension.id() != 1) {
		auto &terrain = m_heightmap.getOrCreateChunk(chunk.x(), chunk.y());
		std::vector<const ServerChunk *> highChunks;
		for (const auto *above : m_chunkColumns.at({chunk.x(), chunk.y()})) {
			if (above->z() > chunk.z() && above->z() * CHUNK_HEIGHT >= 128)
				highChunks.push_back(above);
		}
		std::sort(highChunks.begin(), highChunks.end(), [](const ServerChunk *a, const ServerChunk *b) { return a->z() > b->z(); });
		auto attenuate = [](u8 light, const BlockState &state) -> u8 {
			if (!light || state.isOpaque()) return 0;
			if (!state.block().id() || state.drawType() == BlockDrawType::Glass || state.drawType() == BlockDrawType::XShape) return light;
			return light - 1;
		};
		int ceiling = (chunk.z() + 1) * CHUNK_HEIGHT - 1;
		std::array<const Chunk *, 4> terrainChunks;
		for (int z = 0; z < 4; ++z) terrainChunks[z] = getChunk(chunk.x(), chunk.y(), z);
		for (int y = 0; y < CHUNK_DEPTH; ++y) for (int x = 0; x < CHUNK_WIDTH; ++x) {
			u8 sunlight = 15;
			for (const auto *above : highChunks) {
				for (int z = CHUNK_HEIGHT - 1; z >= 0 && sunlight; --z)
					sunlight = attenuate(sunlight, *above->getBlockState(x, y, z));
				if (!sunlight) break;
			}
			// unopened terrain above a cave must not become a source of sunlight.
			for (int z = 127; z > ceiling && sunlight; --z) {
				const Chunk *above = z >= 0 ? terrainChunks[z / CHUNK_HEIGHT] : getChunkAtBlockPos(chunk.x() * CHUNK_WIDTH + x, chunk.y() * CHUNK_DEPTH + y, z);
				if (above && (above->isInitialized() || above->getBlock(x, y, z & (CHUNK_HEIGHT - 1))))
					sunlight = attenuate(sunlight, *above->getBlockState(x, y, z & (CHUNK_HEIGHT - 1)));
				else {
					u8 kind = terrain.blockAt(x, y, z);
					if (kind == 8 || kind == 9 || kind == 10 || kind == 11 || kind == 79) --sunlight;
					else if (kind) sunlight = 0;
				}
			}
			sunlight = attenuate(sunlight, *chunk.getBlockState(x, y, CHUNK_HEIGHT - 1));
			if (sunlight) chunk.lightmap().addSunlight(x, y, CHUNK_HEIGHT - 1, sunlight);
		}
	}
	for (unsigned side = 0; side < 6; ++side) {
		auto *neighbor = chunk.getSurroundingChunk(side);
		if (neighbor && neighbor->isInitialized()) neighbor->lightmap().refreshBoundaryLights(side ^ 1);
	}
	addChunkToUpdate(&chunk);
}

void ServerWorld::tickActiveChunks() {
	std::vector<ServerChunk *> active;
	std::unordered_set<Vector3i> seen;
	for (const auto &entry : m_players) {
		const auto &player = entry.second;
		if (!player.isReady() || !player.isOnline() || player.dimension() != m_dimension.id()) continue;
		Vector3i center = player.getCurrentChunk();
		for (int z = -1; z <= 1; ++z) for (int y = -2; y <= 2; ++y) for (int x = -2; x <= 2; ++x) {
			Vector3i position{center.x + x, center.y + y, center.z + z};
			if (!seen.emplace(position).second) continue;
			auto found = m_chunks.find(position);
			if (found != m_chunks.end() && found->second->isInitialized()) active.push_back(found->second.get());
		}
	}
	if (active.empty()) { m_activeChunkCursor = 0; return; }
	std::size_t count = std::min<std::size_t>(64, active.size());
	for (std::size_t i = 0; i < count; ++i) {
		m_activeChunkCursor %= active.size();
		active[m_activeChunkCursor++]->tick();
	}
}

bool ServerWorld::moveSurvivalPlayer(ServerPlayer &player, double x, double y, double z) {
	if (player.isRidingMinecart()) return true;
	if (!player.isReady() || !player.isOnline() || !player.health() || player.dimension() != m_dimension.id()
	 || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)
	 || std::abs(x) > 1000000000 || std::abs(y) > 1000000000 || std::abs(z) > 1000000000) return false;
	Vector3d requested{x - player.x(), y - player.y(), z - player.z()};
	if (requested.length() > 64) return false;
	DoubleBox box = player.hitbox();
	box.move(player.x(), player.y(), player.z());
	auto previousEnvironment = playerEnvironment(*this, box);
	if (!previousEnvironment.loaded) { player.survival().fallDistance = 0; return false; }
	Vector3d allowed = player.isNoclip() ? requested : retro::movePlayer(*this, box, requested, false, false);
	if ((allowed - requested).length() > 0.002) return false;
	box = player.hitbox(); box.move(x, y, z);
	PlayerEnvironment environment = playerEnvironment(*this, box);
	if (!environment.loaded) { player.survival().fallDistance = 0; return false; }
	auto &stats = player.statistics().general;
	u64 horizontal = u64(std::round(std::hypot(requested.x, requested.y) * 100));
	const auto *feet = getBlockState(int(std::floor(x)), int(std::floor(y)), int(std::floor(z)));
	bool climbing = feet && feet->block().getGroupValue("group:climbable");
	if (player.isNoclip() || (player.isCreative() && !environment.grounded))
		stats[PlayerStatistics::FlyCm] += u64(std::round(requested.length() * 100));
	else if (environment.water) stats[PlayerStatistics::SwimCm] += u64(std::round(requested.length() * 100));
	else if (climbing && requested.z > 0) stats[PlayerStatistics::ClimbCm] += u64(std::round(requested.z * 100));
	else stats[PlayerStatistics::WalkCm] += horizontal;
	if (requested.z < 0 && !environment.water && !player.isCreative())
		stats[PlayerStatistics::FallCm] += u64(std::round(-requested.z * 100));
	if (previousEnvironment.grounded && requested.z > 0 && !environment.grounded && !climbing && !environment.water)
		++stats[PlayerStatistics::Jumps];
	player.setPosition(x, y, z);
	u16 damage = player.isCreative() ? 0 : player.survival().moved(requested.z, environment.grounded, environment.water);
	if (damage) {
		if (m_server) m_server->damagePlayer(player.clientID(), damage, player.survival().damageCause);
		else player.damage(player.survival().filterDamage(damage));
	}
	return true;
}

void ServerWorld::updatePlayerSurvival() {
	for (auto &entry : m_players) {
		auto &player = entry.second;
		if (!player.isReady() || !player.isOnline() || player.dimension() != m_dimension.id()) continue;
		++player.statistics().general[PlayerStatistics::PlayTicks];
		if (!player.health() || player.isCreative()) continue;
		DoubleBox box = player.hitbox(); box.move(player.x(), player.y(), player.z());
		if (player.isRidingMinecart()) box.sizeZ = 1.3;
		u16 damage = player.survival().tick(playerEnvironment(*this, box, player.isRidingMinecart() ? 0.85 : 1.625));
		if (damage) {
			if (m_server) m_server->damagePlayer(player.clientID(), damage, player.survival().damageCause);
			else player.damage(player.survival().filterDamage(damage));
		}
	}
}

void ServerWorld::updatePlayerChunks(ServerPlayer &player, s32 cx, s32 cy, s32 cz) {
	Vector3i currentChunk{cx, cy, cz};

	player.sentChunks.clear();
	m_chunkSendRequestQueue = {};
	m_queuedChunks.clear();
	m_queuedChunks.emplace(currentChunk);
	m_chunkSendRequestQueue.emplace(currentChunk, player, GameClock::getInstance().getTicks(true));

	std::list<Vector3i> chunksToRemove;
	for (auto &chunkPos : player.loadedChunks()) {
		glm::dvec3 chunkWorldPos{
			chunkPos.x * CHUNK_WIDTH + CHUNK_WIDTH / 2,
			chunkPos.y * CHUNK_DEPTH + CHUNK_DEPTH / 2,
			chunkPos.z * CHUNK_HEIGHT + CHUNK_HEIGHT / 2
		};

		glm::dvec3 playerPos{
			player.x(),
			player.y(),
			player.z()
		};

		if (glm::length(playerPos - chunkWorldPos) >= (ServerConfig::renderDistance + 3) * CHUNK_WIDTH) {
			m_server->sendChunkUnload(chunkPos.x, chunkPos.y, chunkPos.z, player.client());
			chunksToRemove.emplace_back(chunkPos);

			// logDebug() << "Chunk at" << chunkPos.x << chunkPos.y << chunkPos.z << "unloaded for player" << player.name();
		}
	}

	for (auto &chunkPos : chunksToRemove)
		player.removeLoadedChunk(chunkPos);
}

void ServerWorld::createChunkNeighbours(ServerChunk &chunk) {
	Vector3i surroundingChunks[6] = {
		{chunk.x() - 1, chunk.y(),     chunk.z()},
		{chunk.x() + 1, chunk.y(),     chunk.z()},
		{chunk.x(),     chunk.y() - 1, chunk.z()},
		{chunk.x(),     chunk.y() + 1, chunk.z()},
		{chunk.x(),     chunk.y(),     chunk.z() - 1},
		{chunk.x(),     chunk.y(),     chunk.z() + 1},
	};

	for (u8 i = 0 ; i < 6 ; ++i) {
		// Check if this neighbour already exists, if yes then skip it
		ServerChunk *neighbour = (ServerChunk *)getChunk(surroundingChunks[i].x, surroundingChunks[i].y, surroundingChunks[i].z);
		if (neighbour) {
			// Assign surrounding chunk pointers
			chunk.setSurroundingChunk(i, neighbour);
			neighbour->setSurroundingChunk((i % 2 == 0) ? i + 1 : i - 1, &chunk);

			continue;
		}

		// Create our neighbour
		auto it = m_chunks.emplace(
			Vector3i{
				surroundingChunks[i].x,
				surroundingChunks[i].y,
				surroundingChunks[i].z
			},
			new ServerChunk{
				surroundingChunks[i].x,
				surroundingChunks[i].y,
				surroundingChunks[i].z,
				*this
			}
		);

		// Get the created neighbour
		neighbour = it.first->second.get();
		m_chunkColumns[{neighbour->x(), neighbour->y()}].push_back(neighbour);

		// Assign surrounding chunk pointers
		chunk.setSurroundingChunk(i, neighbour);
		neighbour->setSurroundingChunk((i % 2 == 0) ? i + 1 : i - 1, &chunk);
	}
}

ServerChunk &ServerWorld::getOrCreateChunk(s32 cx, s32 cy, s32 cz) {
	ServerChunk *chunk = (ServerChunk *)getChunk(cx, cy, cz);
	if (!chunk) {
		auto it = m_chunks.emplace(Vector3i{cx, cy, cz}, new ServerChunk(cx, cy, cz, *this));
		chunk = it.first->second.get();
		m_chunkColumns[{cx, cy}].push_back(chunk);
	}

	// Create our neighbours
	createChunkNeighbours(*chunk);

	return *chunk;
}

Chunk *ServerWorld::getChunk(int cx, int cy, int cz) const {
	auto it = m_chunks.find({cx, cy, cz});
	if (it == m_chunks.end())
		return nullptr;

	return it->second.get();
}

bool ServerWorld::generateChunk(ServerChunk &chunk) {
	if (chunk.isInitialized()) return false;

	createChunkNeighbours(chunk);

	m_terrainGenerator.generate(chunk);
	chunk.setInitialized(true);
	chunk.initializeTickingBlocks();
	initializeChunkLighting(chunk);
	m_blockPhysics.seedChunk(chunk);

	return true;
}

void ServerWorld::processSendRequests() {
	using Clock = std::chrono::steady_clock;
	auto deadline = Clock::now() + std::chrono::milliseconds(3);
	unsigned generated = 0;
	unsigned processed = 0;
	while (!m_chunkSendRequestQueue.empty() && processed < 32 && Clock::now() < deadline) {
		auto &request = m_chunkSendRequestQueue.front();
		Vector3i position = request.chunkPos;
		auto &player = request.player;
		bool expired = GameClock::getInstance().getTicks(true) - request.timestamp >= 10000;
		Vector3d center{position.x * CHUNK_WIDTH + CHUNK_WIDTH / 2.0,
			position.y * CHUNK_DEPTH + CHUNK_DEPTH / 2.0, position.z * CHUNK_HEIGHT + CHUNK_HEIGHT / 2.0};
		Vector3d difference = center - Vector3d{player.x(), player.y(), player.z()};
		bool inRange = difference.length() < (ServerConfig::renderDistance + 2) * CHUNK_WIDTH;
		if (!expired && inRange && player.isOnline() && !player.sentChunks.count(position)) {
			if (!player.isChunkLoaded(position)) {
				ServerChunk &chunk = getOrCreateChunk(position.x, position.y, position.z);
				std::array<Chunk *, 7> required{{&chunk, chunk.getSurroundingChunk(Chunk::West),
					chunk.getSurroundingChunk(Chunk::East), chunk.getSurroundingChunk(Chunk::South),
					chunk.getSurroundingChunk(Chunk::North), chunk.getSurroundingChunk(Chunk::Bottom),
					chunk.getSurroundingChunk(Chunk::Top)}};
				bool ready = true;
				for (Chunk *neighbor : required) {
					if (neighbor->isInitialized()) continue;
					if (!generated && Clock::now() < deadline) {
						generateChunk(static_cast<ServerChunk &>(*neighbor));
						++generated;
					} else { ready = false; break; }
				}
				// keep the request until its neighbors can supply light and mesh borders.
				if (!ready) break;
				chunk.lightmap().updateLights();
				chunk.setReadyToSend();
				addChunkToProcess(&chunk);
				player.addLoadedChunk(position);
			}
			player.sentChunks.emplace(position);
			for (const Vector3i &offset : {Vector3i{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}}) {
				Vector3i neighbor = position + offset;
				if (!player.sentChunks.count(neighbor) && m_queuedChunks.emplace(neighbor).second)
					m_chunkSendRequestQueue.emplace(neighbor, player, GameClock::getInstance().getTicks(true));
			}
		}
		m_queuedChunks.erase(position);
		m_chunkSendRequestQueue.pop();
		++processed;
	}
}

void ServerWorld::initializeChest(int x, int y, int z) {
	if (!getBlockData(x, y, z)) addBlockData(x, y, z, 9, 3);
	const Vector3i placed{x, y, z};
	if (!chest::canPlace(*this, placed)) return;
	std::vector<Vector3i> positions{placed};
	// placement callbacks run before the new block id is written.
	for (const auto &offset : chest::neighbors) {
		const auto *next = chest::loaded(*this, placed + offset);
		if (next && next->block().hasGroup("group:om_chest")) positions.push_back(placed + offset);
	}
	if (positions.size() != 2) return;
	const Vector3i other = positions[1];
	const auto &block = getBlockState(other.x, other.y, other.z)->block();
	u16 rotation = block.param().getParam(BlockParam::Rotation, getData(other.x, other.y, other.z)) & 3;
	u16 axis = positions[0].x != positions[1].x ? 1 : 0;
	if ((rotation & 1) != axis) rotation = axis;
	for (const auto &pos : positions) {
		u16 previous = getData(pos.x, pos.y, pos.z);
		u16 data = block.param().setParam(BlockParam::Rotation, previous, rotation);
		if (data == previous) continue;
		setData(pos.x, pos.y, pos.z, data);
		if (m_server) {
			Network::Packet update;
			update << Network::Command::BlockUpdate << s32(pos.x) << s32(pos.y) << s32(pos.z)
				<< (u32(block.id()) | (u32(data) << 16));
			m_server->server().sendToAllClients(update);
		}
	}
}

std::vector<glm::ivec3> ServerWorld::prepareChest(int x, int y, int z, ServerPlayer &player) {
	const auto positions = chest::halves(*this, {x, y, z});
	if (!chest::canOpen(*this, positions)) return {};
	for (const auto &pos : positions) {
		const auto *data = getBlockData(pos.x, pos.y, pos.z);
		if (data && (data->inventory.width() != 9 || data->inventory.height() != 3)) return {};
	}
	initializeChest(x, y, z);
	std::vector<glm::ivec3> result;
	for (const auto &pos : positions) {
		auto *data = getBlockData(pos.x, pos.y, pos.z);
		if (!data) data = addBlockData(pos.x, pos.y, pos.z, 9, 3);
		static_cast<ServerChunk *>(getChunkAtBlockPos(pos.x, pos.y, pos.z))->setModified(true);
		if (m_server) m_server->sendBlockInvUpdate(pos.x, pos.y, pos.z, data->inventory, player.client());
		result.emplace_back(pos.x, pos.y, pos.z);
	}
	return result;
}

void ServerWorld::initUsertype(sol::state &lua) {
	lua.new_usertype<ServerWorld>("ServerWorld",
		sol::base_classes, sol::bases<World>(),

		"play_note", &ServerWorld::playNote,
		"break_piston_head", &ServerWorld::breakPistonHead,
		"remove_piston", &ServerWorld::removePiston,
		"tick_spawner", &ServerWorld::tickSpawner,
		"use_jukebox", &ServerWorld::useJukebox,
		"stop_jukebox", &ServerWorld::stopJukebox,
		"initialize_chest", &ServerWorld::initializeChest,
		"prepare_chest", [](ServerWorld &world, int x, int y, int z, ServerPlayer &player) {
			return sol::as_table(world.prepareChest(x, y, z, player));
		},
		"emit_particles", [](ServerWorld &world, int x, int y, int z, u8 effect) {
			if (world.server()) world.server()->sendParticles({x + 0.5, y + 0.5, z + 0.5}, world.dimension().id(), effect);
		},
		"get_ticks", [](const ServerWorld &) { return GameTime::getTicks(); },
		"is_daytime", [](const ServerWorld &world) { return world.dimension().id() == 0 && GameTime::getTicks() % GameTime::dayLength < 12000; },
		"mark_block_modified", [](ServerWorld &world, int x, int y, int z) {
			auto *chunk = static_cast<ServerChunk *>(world.getChunkAtBlockPos(x, y, z));
			if (chunk && chunk->isInitialized()) chunk->setModified(true);
		},
		"validate_portal", &ServerWorld::validatePortal,
		"grow_tree", &ServerWorld::growTree,
		"remove_bed_partner", &ServerWorld::removeBedPartner,
		"validate_bed", &ServerWorld::validateBed,
		"dimension", &ServerWorld::dimension
	);
}

void ServerWorld::appendCollisionBoxes(const DoubleBox &moving, const DoubleBox &broadphase, std::vector<DoubleBox> &boxes) const {
	auto add = [&](DoubleBox box) {
		if (std::abs(box.x - moving.x) < 1e-6 && std::abs(box.y - moving.y) < 1e-6 && std::abs(box.z - moving.z) < 1e-6
		 && std::abs(box.sizeX - moving.sizeX) < 1e-6 && std::abs(box.sizeY - moving.sizeY) < 1e-6
		 && std::abs(box.sizeZ - moving.sizeZ) < 1e-6) return;
		if (box.intersects(broadphase)) boxes.push_back(box);
	};
	for (const auto &entry : m_players) {
		const auto &player = entry.second;
		if (!player.isReady() || !player.isOnline() || player.dimension() != m_dimension.id()) continue;
		DoubleBox box = player.hitbox();
		box.move(player.x(), player.y(), player.z());
		add(box);
	}
	m_scene.registry().view<const MobComponent, const PositionComponent, const DoubleBox>().each(
		[&](auto entity, const auto &, const auto &position, const auto &localBox) {
			const auto *traits = m_scene.registry().try_get<BetaMobComponent>(entity);
			if (position.dimension != m_dimension.id()
			 || (traits && (betaMobIsProjectile(traits->kind) || traits->kind == BetaMobKind::Bat))) return;
			DoubleBox box = localBox;
			box.move(position.x, position.y, position.z);
			add(box);
		});
}
