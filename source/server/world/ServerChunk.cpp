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
#include <array>

#include "common/network/Network.hpp"
#include "common/core/Registry.hpp"
#include "common/world/Dimension.hpp"
#include "common/world/Player.hpp"
#include "common/world/World.hpp"
#include "server/core/PlayerList.hpp"
#include "server/network/ClientInfo.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerChunk.hpp"
#include "server/world/ServerWorld.hpp"

ServerChunk::ServerChunk(s32 x, s32 y, s32 z, ServerWorld &world) : Chunk(x, y, z, world), m_world(world) {
	m_random.seed((unsigned int)std::time(nullptr));
}

void ServerChunk::update() {
	if (areAllNeighboursInitialized())
		m_lightmap.updateLights();

	if (m_hasChanged || m_lightmap.hasChanged()) {
		// logDebug() << "Chunk update at" << m_x << m_y << m_z << "| D:" << m_hasChanged << "| L:" << m_lightmap.hasChanged();

		m_hasChanged = false;
		m_lightmap.resetChangedFlag();

		if (m_isInitialized && m_isReadyToSend)
			m_world.addChunkToProcess(this);
	}
}

void ServerChunk::process() {
	for (auto &client : m_world.server()->server().info().clients()) {
		const ServerPlayer *player = m_world.players().getPlayer(client.playerName);
		bool isChunkLoaded = player && player->isChunkLoaded({m_x, m_y, m_z});
		if (player && player->isReady() && isChunkLoaded && player->dimension() == m_world.dimension().id())
			sendChanges(client);
	}
}

void ServerChunk::onBlockChanged(int x, int y, int z) {
	if (!m_isInitialized) return;
	const auto &block = static_cast<const ServerBlock &>(Registry::getInstance().getBlock(getBlock(x, y, z)));
	if (block.canUpdate()) addTickingBlock(x, y, z, block);
	else m_tickingBlocks.erase((z * depth + y) * width + x);
	setModified(true);
	m_world.blockPhysics().enqueueNeighbors({x + m_x * width, y + m_y * depth, z + m_z * height});
}

void ServerChunk::onBlockPlaced(int x, int y, int z, const Block &block) {
	const ServerBlock &serverBlock = (ServerBlock &)block;
	if (block.canUpdate()) {
		addTickingBlock(x, y, z, serverBlock);
	}
	else {
		m_tickingBlocks.erase((z * depth + y) * width + x);
	}

	serverBlock.onBlockPlaced(glm::ivec3{x + m_x * CHUNK_WIDTH, y + m_y * CHUNK_DEPTH, z + m_z * CHUNK_HEIGHT}, (ServerWorld &)m_world);

	setModified(true);
}

void ServerChunk::onBlockDestroyed(int x, int y, int z, const Block &block) {
	const ServerBlock &serverBlock = (ServerBlock &)block;
	bool leaves = block.getState(0).drawType() == BlockDrawType::Leaves;
	const auto &id = block.stringID();
	bool log = block.hasGroup("group:om_material_wood") && id.size() >= 5 && id.compare(id.size()-5,5,"_wood") == 0;
	if (leaves || log)
		m_world.blockPhysics().markLeaves({x + m_x * width, y + m_y * depth, z + m_z * height}, leaves ? 1 : 4);
	serverBlock.onBlockDestroyed(glm::ivec3{x + m_x * CHUNK_WIDTH, y + m_y * CHUNK_DEPTH, z + m_z * CHUNK_HEIGHT}, (ServerWorld &)m_world);

	setModified(true);
}

void ServerChunk::onBlockDataRemoved(int x, int y, int z, const BlockData &data) {
	if (!m_isInitialized) return;
	m_world.dropContainerItems({x + m_x * width, y + m_y * depth, z + m_z * height}, data.inventory);
}

void ServerChunk::initializeTickingBlocks() {
	m_tickingBlocks.clear();
	m_tickCursor = 0;
	for (int z = 0; z < height; ++z) for (int y = 0; y < depth; ++y) for (int x = 0; x < width; ++x) {
		const auto &block = static_cast<const ServerBlock &>(Registry::getInstance().getBlock(getBlock(x, y, z)));
		if (block.canUpdate()) addTickingBlock(x, y, z, block);
	}
}

void ServerChunk::tick(unsigned budget) {
	if (!m_isInitialized || !m_world.server() || m_tickingBlocks.empty()) return;
	std::array<u16, 64> selected;
	unsigned count = std::min<unsigned>(std::min<unsigned>(budget, selected.size()), m_tickingBlocks.size());
	auto it = m_tickingBlocks.lower_bound(m_tickCursor);
	for (unsigned i = 0; i < count; ++i) {
		if (it == m_tickingBlocks.end()) it = m_tickingBlocks.begin();
		selected[i] = *it++;
		m_tickCursor = selected[i] + 1;
	}
	// callbacks may replace themselves or other ticking blocks.
	for (unsigned i = 0; i < count; ++i) {
		u16 index = selected[i];
		if (!m_tickingBlocks.count(index)) continue;
		int x = index % width, y = (index / width) % depth, z = index / (width * depth);
		const auto &block = static_cast<const ServerBlock &>(Registry::getInstance().getBlock(getBlock(x, y, z)));
		if (!block.canUpdate()) { m_tickingBlocks.erase(index); continue; }
		if (!block.isTickingRandomly() || m_random.get<bool>(block.tickProbability()))
			block.onTick({x + m_x * width, y + m_y * depth, z + m_z * height}, *this, m_world, *m_world.server());
	}
}

void ServerChunk::sendData(const ClientInfo &client) {
	Network::Packet packet;
	packet << Network::Command::ChunkData;
	packet << m_x << m_y << m_z;
	std::array<u8, CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT * 5> cells;
	std::size_t offset = 0;
	m_sentCells.resize(CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT);
	for (u16 z = 0; z < CHUNK_HEIGHT; ++z) {
		for (u16 y = 0; y < CHUNK_DEPTH; ++y) {
			for (u16 x = 0; x < CHUNK_WIDTH; ++x) {
				u32 value = data(x, y, z);
				m_sentCells[(z * depth + y) * width + x] = u64(value) | (u64(m_lightmap.getLightDataRaw(x, y, z)) << 32);
				// block words use the same network byte order as sf::Packet.
				cells[offset++] = u8(value >> 24);
				cells[offset++] = u8(value >> 16);
				cells[offset++] = u8(value >> 8);
				cells[offset++] = u8(value);
				cells[offset++] = m_lightmap.getLightDataRaw(x, y, z);
			}
		}
	}
	packet.append(cells.data(), cells.size());
	client.send(packet);
	sendMetadata(client);
}

void ServerChunk::sendMetadata(const ClientInfo &client) {
	// the receiving chunk must exist before its metadata packets arrive.
	for (const auto &entry : m_blockData) {
		const auto &position = entry.first;
		const BlockData *data = entry.second.get();
		s32 x = position.x + m_x * CHUNK_WIDTH;
		s32 y = position.y + m_y * CHUNK_DEPTH;
		s32 z = position.z + m_z * CHUNK_HEIGHT;
		m_world.server()->sendBlockDataUpdate(x, y, z, data, &client);
		m_world.server()->sendBlockInvUpdate(x, y, z, data->inventory, &client);
	}

	// logDebug() << "Chunk at" << chunk.x() << chunk.y() << chunk.z() << "sent to client";
}

void ServerChunk::sendChanges(const ClientInfo &client) {
	if (m_sentCells.empty()) { sendData(client); return; }
	Network::Packet changes;
	u16 count = 0;
	for (u16 z = 0; z < height; ++z) for (u16 y = 0; y < depth; ++y) for (u16 x = 0; x < width; ++x) {
		u16 index = (z * depth + y) * width + x;
		u32 block = data(x,y,z); u8 light = m_lightmap.getLightDataRaw(x,y,z);
		u64 value = u64(block) | (u64(light) << 32);
		if (value == m_sentCells[index]) continue;
		changes << index << block << light;
		m_sentCells[index] = value;
		++count;
	}
	if (count > 4096) { sendData(client); return; }
	if (count) {
		Network::Packet packet;
		packet << Network::Command::ChunkDelta << m_x << m_y << m_z << count;
		packet.append(changes.getData(), changes.getDataSize());
		client.send(packet);
	}
	sendMetadata(client);
}
