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
#include <algorithm> // std::max
#include <unordered_set>

#include "common/core/EventHandler.hpp"
#include "common/core/Registry.hpp"
#include "common/core/GameTime.hpp"
#include "common/world/Dimension.hpp"
#include "common/event/Events.hpp"
#include "common/resource/ResourceHandler.hpp"
#include "common/world/Sky.hpp"
#include "common/world/World.hpp"
#include "common/world/SignText.hpp"

#include "client/core/ClientProfiler.hpp"
#include "client/graphics/Camera.hpp"
#include "client/graphics/MobModel.hpp"
#include "client/scene/component/DrawableComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "client/graphics/TextureAtlas.hpp"
#include "client/network/ClientCommandHandler.hpp"
#include "client/world/ClientPlayer.hpp"
#include "client/world/ClientWorld.hpp"

ClientWorld::ClientWorld()
	: m_textureAtlas(ResourceHandler::getInstance().get<TextureAtlas>("atlas-blocks")),
	  m_chunkRenderer(m_textureAtlas)
{
	m_chunkRenderer.setOnChunkDeletionRequestedCallback([this](const Vector3i &chunkPos) {
		m_chunksToRemove.emplace(chunkPos);
	});

	m_chunkRenderer.setOnMeshingRequestedCallback([this](float distance, const Vector3i &chunkPos) {
		m_chunksToMesh.emplace(distance, chunkPos);
	});
}

void ClientWorld::update(bool allowWorldReload) {
	World::update();
	if (m_dimension) m_textureAtlas.updateClock(GameTime::getTicks(), m_dimension->id());

	// Debug values only used in DebugOverlay
	u64 time = std::time(nullptr);
	if (time > ClientChunk::chunkUpdateTime) {
		ClientChunk::chunkUpdatesPerSec = ClientChunk::chunkUpdateCounter;
		ClientChunk::chunkUpdateCounter = 0;
		ClientChunk::chunkUpdateTime = time;
	}
	if (time > ClientChunk::chunkDrawTime) {
		ClientChunk::chunksRenderedPerFrame = ClientChunk::chunkDrawCounter / std::max<u64>(1, ClientChunk::frameCounter - ClientChunk::chunkDrawStartFrame);
		ClientChunk::chunkDrawCallPerFrame = ClientChunk::chunkDrawCallCounter / std::max<u64>(1, ClientChunk::frameCounter - ClientChunk::chunkDrawStartFrame);
		ClientChunk::chunkDrawCounter = 0;
		ClientChunk::chunkDrawCallCounter = 0;
		ClientChunk::chunkDrawTime = time;
		ClientChunk::chunkDrawStartFrame = ClientChunk::frameCounter;
	}

	unsigned removed = 0;
	for (auto it = m_chunksToRemove.begin(); it != m_chunksToRemove.end() && removed < 4; ++removed) {
		removeChunk(*it);
		it = m_chunksToRemove.erase(it);
	}

	if (allowWorldReload) {
		if (World::isReloadRequested) {
			for (auto &[pos, chunk] : m_chunks) {
				chunk->setChanged();
				addChunkToUpdate(chunk.get());
			}

			World::isReloadRequested = false;
		}
	}

	requestChunkMeshing();

	m_scene.update();

	m_chunkMeshBuilder.update();
}

void ClientWorld::requestChunkMeshing() {
	for (auto it = m_chunksToMesh.begin() ; it != m_chunksToMesh.end() ; ) {
		ClientChunk *chunk = (ClientChunk *)getChunk(it->second.x, it->second.y, it->second.z);
		if(chunk && !chunk->isReadyForMeshing() && chunk->areAllNeighboursLoaded()) {
			chunk->setReadyForMeshing(true);
			chunk->setChanged();
			addChunkToUpdate(chunk);

			// logDebug() << "Chunk at" << ux << uy << uz << "is ready for meshing";
		}

		it = m_chunksToMesh.erase(it);
	}
}

void ClientWorld::checkPlayerChunk(double playerX, double playerY, double playerZ) {
	int pcx = (int)std::floor(playerX / CHUNK_WIDTH);
	int pcy = (int)std::floor(playerY / CHUNK_DEPTH);
	int pcz = (int)std::floor(playerZ / CHUNK_HEIGHT);

	ClientChunk *chunk = (ClientChunk *)getChunk(pcx, pcy, pcz);
	if (!chunk) {
		m_chunks.emplace(Vector3i{pcx, pcy, pcz}, new ClientChunk(pcx, pcy, pcz, *m_dimension, *this));
	}
}

void ClientWorld::clear() {
	clearUpdateQueues();
	m_chunkMeshBuilder.clear();
	m_chunksToMesh.clear();
	m_chunksToRemove.clear();
	m_chunks.clear();
	m_scene.registry().clear();
	m_scene.particles().clear();
}

void ClientWorld::changeDimension(u16 dimensionID) {
	World::clearUpdateQueues();

	const Dimension &dimension = Registry::getInstance().getDimension(dimensionID);
	m_dimension = &dimension;

	const Sky &sky = Registry::getInstance().getSkyFromStringID(dimension.sky());
	m_sky = &sky;
}

void ClientWorld::receiveChunkData(Network::Packet &packet) {
	if (!m_dimension) return;

	OM_PROFILE_START("ClientWorld::receiveChunkData");

	s32 cx, cy, cz;
	packet >> cx >> cy >> cz;

	// Get the chunk from the map or create it if it doesn't exist
	ClientChunk *chunk = (ClientChunk *)getChunk(cx, cy, cz);
	if (!chunk) {
		auto it = m_chunks.emplace(Vector3i{cx, cy, cz}, new ClientChunk(cx, cy, cz, *m_dimension, *this));
		chunk = it.first->second.get();
	}

	linkChunkNeighbours(chunk);
	chunk->lightmap().clearPendingUpdates();

	// Receive chunk data
	// bool hasUpdatedChunk = false;
	for (u8 z = 0 ; z < CHUNK_HEIGHT ; ++z) {
		for (u8 y = 0 ; y < CHUNK_DEPTH ; ++y) {
			for (u8 x = 0 ; x < CHUNK_WIDTH ; ++x) {
				u32 block;
				u8 light;

				packet >> block >> light;

				chunk->setDataRaw(x, y, z, block);
				chunk->lightmap().setLightDataRaw(x, y, z, light);

				// bool updatedBlock = chunk->setBlockRaw(x, y, z, block & 0xffff);
				// bool updatedData = chunk->setData(x, y, z, block >> 16);
				// bool updatedLight = chunk->lightmap().setLightData(x, y, z, light);
				// if (updatedBlock || updatedData || updatedLight)
				// 	hasUpdatedChunk = true;
			}
		}
	}

	addChunkToUpdate(chunk);

	// if (!hasUpdatedChunk)
	// 	logWarning() << "Received chunk at" << cx << cy << cz << "without any updates";

	++chunk->debugTimesReceived;

	if (!chunk->isInitialized()) {
		auto addSurroundingChunkToUpdate = [this, chunk](u8 i) {
			Chunk *surroundingChunk = chunk->getSurroundingChunk(i);
			if (surroundingChunk) {
				surroundingChunk->setChanged();
				addChunkToUpdate(surroundingChunk);
			}
		};

		addSurroundingChunkToUpdate(0);
		addSurroundingChunkToUpdate(1);
		addSurroundingChunkToUpdate(2);
		addSurroundingChunkToUpdate(3);
		addSurroundingChunkToUpdate(4);
		addSurroundingChunkToUpdate(5);

		chunk->setInitialized(true);
	}

	if (m_eventHandler)
		m_eventHandler->emplaceEvent<ChunkCreatedEvent>(Vector3i{cx, cy, cz}, true);

	// logDebug() << "Chunk at" << cx << cy << cz << "received";

	OM_PROFILE_END("ClientWorld::receiveChunkData");
}

void ClientWorld::removeChunk(const Vector3i &chunkPos) {
	auto it = m_chunks.find(chunkPos);
	if (it == m_chunks.end()) {
		logWarning() << "Tried to remove unloaded chunk at" << chunkPos.x << chunkPos.y << chunkPos.z;
		return;
	}

	ClientChunk *surroundingChunks[6] = {
		(ClientChunk *)it->second->getSurroundingChunk(0),
		(ClientChunk *)it->second->getSurroundingChunk(1),
		(ClientChunk *)it->second->getSurroundingChunk(2),
		(ClientChunk *)it->second->getSurroundingChunk(3),
		(ClientChunk *)it->second->getSurroundingChunk(4),
		(ClientChunk *)it->second->getSurroundingChunk(5)
	};

	if (m_eventHandler)
		m_eventHandler->emplaceEvent<ChunkRemovedEvent>(Vector3i{it->second->x(), it->second->y(), it->second->z()});

	removeChunkFromQueues(it->second.get());
	m_chunkMeshBuilder.remove(chunkPos);
	m_chunks.erase(it);

	for (u8 i = 0 ; i < 6 ; ++i)
		if (surroundingChunks[i])
			surroundingChunks[i]->setSurroundingChunk((i % 2 == 0) ? i + 1 : i - 1, nullptr);

	// logDebug() << "Chunk at" << chunkPos.x << chunkPos.y << chunkPos.z << "removed";
}

Chunk *ClientWorld::getChunk(int cx, int cy, int cz) const {
	auto it = m_chunks.find({cx, cy, cz});
	if (it == m_chunks.end())
		return nullptr;

	return it->second.get();
}

void ClientWorld::linkChunkNeighbours(ClientChunk *chunk) {
	Vector3i surroundingChunks[6] = {
		{chunk->x() - 1, chunk->y(),     chunk->z()},
		{chunk->x() + 1, chunk->y(),     chunk->z()},
		{chunk->x(),     chunk->y() - 1, chunk->z()},
		{chunk->x(),     chunk->y() + 1, chunk->z()},
		{chunk->x(),     chunk->y(),     chunk->z() - 1},
		{chunk->x(),     chunk->y(),     chunk->z() + 1},
	};

	// Create entries in the map for surrounding chunks
	for (u8 i = 0 ; i < 6 ; ++i) {
		const s32 scx = surroundingChunks[i].x;
		const s32 scy = surroundingChunks[i].y;
		const s32 scz = surroundingChunks[i].z;

		ClientChunk *neighbour = (ClientChunk *)getChunk(scx, scy, scz);
		if (neighbour) {
			chunk->setSurroundingChunk(i, neighbour);
			neighbour->setSurroundingChunk((i % 2 == 0) ? i + 1 : i - 1, chunk);
		}
	}
}

void ClientWorld::draw(RenderTarget &target, RenderStates states) const {
	if (!target.getView() || !m_camera) {
		logError() << "Trying to draw world without a camera";
		return;
	}

	OM_PROFILE_START("ClientWorld::draw");

	m_chunkRenderer.draw(target, states, m_chunks, *m_camera, m_sky);

	target.draw(m_scene, states);

	std::unordered_set<Vector3i> visibleSigns;
	const auto camera = m_camera->getDPosition();
	// sign vertices use the same camera-relative coordinates as chunk meshes.
	m_camera->setDPosition(0, 0, 0);
	for (const auto &entry : m_chunks) {
		const auto &chunk = *entry.second;
		if (!chunk.hasBlockMetadata()) continue;
		if (std::abs(chunk.x() * CHUNK_WIDTH - camera.x) > 48
		 || std::abs(chunk.y() * CHUNK_DEPTH - camera.y) > 48
		 || std::abs(chunk.z() * CHUNK_HEIGHT - camera.z) > 48) continue;
		for (const auto &metadata : chunk.blockData()) {
			const auto &local = metadata.first;
			const auto *state = chunk.getBlockState(local.x, local.y, local.z);
			if (!state || !state->block().hasGroup("group:om_sign")) continue;
			Vector3i position{chunk.x() * CHUNK_WIDTH + local.x, chunk.y() * CHUNK_DEPTH + local.y, chunk.z() * CHUNK_HEIGHT + local.z};
			if ((Vector3d{double(position.x), double(position.y), double(position.z)} - camera).length() > 32) continue;
			visibleSigns.insert(position);
			auto &text = m_signText[position];
			auto lines = sign::read(metadata.second->meta);
			u16 rotation = state->block().param().getParam(BlockParam::Rotation, chunk.getData(local.x, local.y, local.z)) & 3;
			for (int i = 0; i < 4; ++i) {
				if (lines[i].empty()) continue;
				if (text[i].string() != lines[i]) {
					text[i].setString(lines[i]);
					text[i].setColor(Color::Black);
					text[i].setShadowEnabled(false);
					text[i].setDepthTestEnabled(true);
				}
				text[i].updateVertexBuffer();
				RenderStates signStates = states;
				signStates.transform.translate(float(position.x - camera.x + 0.5), float(position.y - camera.y + 0.5), float(position.z - camera.z + 0.5));
				signStates.transform.rotateZ(float(rotation) * 90);
				signStates.transform.translate(-0.5f, -0.5f, -0.5f);
				float scale = std::min(0.0095f, 0.9f / std::max(1, text[i].getSize().x));
				signStates.transform.translate(0.5f + text[i].getSize().x * scale / 2, state->id() ? 0.126f : 0.564f, (state->id() ? 0.71f : 0.96f) - i * 0.105f);
				signStates.transform.rotateX(90);
				signStates.transform.scale(-scale, -scale, scale);
				target.draw(text[i], signStates);
			}
		}
	}
	for (auto it = m_signText.begin(); it != m_signText.end();) {
		if (!visibleSigns.count(it->first)) it = m_signText.erase(it);
		else ++it;
	}

	m_camera->setDPosition(camera);
	OM_PROFILE_END("ClientWorld::draw");
}


void ClientWorld::appendCollisionBoxes(const DoubleBox &, const DoubleBox &broadphase, std::vector<DoubleBox> &boxes) const {
	m_scene.registry().view<const PositionComponent, const DoubleBox, const DrawableComponent>().each(
		[&](auto, const auto &position, const auto &localBox, const auto &drawable) {
			const auto *mob = dynamic_cast<const MobModel *>(drawable.drawable());
			if (!mob || !mob->hasCollision() || !m_dimension || position.dimension != m_dimension->id()) return;
			DoubleBox box = localBox;
			box.move(position.x, position.y, position.z);
			if (box.intersects(broadphase)) boxes.push_back(box);
		});
}
