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
#include <chrono>
#include <memory>

#include "client/core/ClientProfiler.hpp"
#include "client/graphics/TextureAtlas.hpp"
#include "client/world/ChunkMeshBuilder.hpp"
#include "client/world/ClientWorld.hpp"

#include "client/world/mesh/BlockMesher.hpp"
#include "client/world/mesh/CampfireMesher.hpp"
#include "client/world/mesh/XShapeMesher.hpp"

// NOTE: TextureAtlas and Registry are accessed from different threads.
//
//       This is not a problem currently since both shouldn’t change during the execution,
//       but if they do, a lot of problems will occur.
//
//       Also, Registry is accessed from server too when running a singleplayer game,
//       so if it changes during the execution, that would cause problems there too.
//
ChunkMeshBuilder::~ChunkMeshBuilder() {
	for (auto &future : m_futures) delete future.get();
}

void ChunkMeshBuilder::clear() {
	m_pending.clear();
	m_pendingSet.clear();
	m_revisions.clear();
}

void ChunkMeshBuilder::remove(const Vector3i &position) {
	m_revisions.erase(position);
	m_pendingSet.erase(position);
	m_pending.erase(std::remove(m_pending.begin(), m_pending.end(), position), m_pending.end());
}

void ChunkMeshBuilder::addMeshBuildingJob(const Chunk &chunk, const TextureAtlas &) {
	Vector3i position{chunk.x(), chunk.y(), chunk.z()};
	m_revisions[position] = ++m_nextRevision;
	if (m_pendingSet.emplace(position).second) m_pending.push_back(position);
}

void ChunkMeshBuilder::update() {
	using Clock = std::chrono::steady_clock;
	auto deadline = Clock::now() + std::chrono::milliseconds(2);
	unsigned completed = 0;
	for (auto it = m_futures.begin(); it != m_futures.end() && completed < 2 && Clock::now() < deadline;) {
		if (it->future().wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) { ++it; continue; }
		std::unique_ptr<ChunkMeshBuildingJob> job{it->get()};
		Vector3i position{job->chunkData.x, job->chunkData.y, job->chunkData.z};
		m_active.erase(position);
		auto revision = m_revisions.find(position);
		ClientChunk *chunk = static_cast<ClientChunk *>(m_world.getChunk(position.x, position.y, position.z));
		if (chunk && revision != m_revisions.end() && revision->second == job->revision) {
			VertexBuffer &vbo = chunk->getVertexBuffer();
			if (job->totalVertexCount) vbo.init(nullptr, u32(job->totalVertexCount * sizeof(Vertex)));
			else vbo.free();
			u32 offset = 0;
			for (u8 layer = 0; layer < ChunkMeshLayer::Count; ++layer) {
				u32 count = u32(job->vertices[layer].size());
				chunk->setVerticesCount(layer, count);
				if (count) vbo.update(job->vertices[layer].data(), count * sizeof(Vertex), offset);
				offset += count;
			}
		}
		it = m_futures.erase(it);
		++completed;
	}
	std::size_t examined = m_pending.size();
	unsigned submitted = 0;
	while (examined-- && !m_pending.empty() && m_futures.size() < 4 && submitted < 2 && Clock::now() < deadline) {
		Vector3i position = m_pending.front();
		m_pending.pop_front();
		if (m_active.count(position)) { m_pending.push_back(position); continue; }
		m_pendingSet.erase(position);
		const Chunk *chunk = m_world.getChunk(position.x, position.y, position.z);
		auto revision = m_revisions.find(position);
		if (!chunk || revision == m_revisions.end()) continue;
		auto job = std::make_unique<ChunkMeshBuildingJob>();
		job->revision = revision->second;
		job->textureAtlas = &m_world.textureAtlas();
		job->chunkData.loadFromChunk(*chunk);
		m_futures.emplace_back(m_workers.submit(&ChunkMeshBuilder::buildChunkMesh, job.get()));
		job.release();
		m_active.emplace(position);
		++submitted;
	}
}

ChunkMeshBuildingJob *ChunkMeshBuilder::buildChunkMesh(ChunkMeshBuildingJob *job) {
	// For each block, generate its vertices and add them to the list
	for (s8f z = 0 ; z < CHUNK_HEIGHT ; z++) {
		for (s8f y = 0 ; y < CHUNK_DEPTH ; y++) {
			for (s8f x = 0 ; x < CHUNK_WIDTH ; x++) {
				u16 blockID = job->chunkData.getBlockID(x, y, z);
				if (!blockID) continue;

				u16 blockParam = job->chunkData.getBlockParam(x, y, z);
				const BlockState &blockState = ChunkData::getBlockState(blockID, blockParam);

				if (blockState.drawType() == BlockDrawType::XShape)
					XShapeMesher::addCross(x, y, z, *job, blockState, blockParam);
				else if (blockState.drawType() == BlockDrawType::Campfire)
					CampfireMesher::addBlock(x, y, z, *job, blockState);
				else
					BlockMesher::addBlock(x, y, z, *job, blockState, blockParam);
			}
		}
	}

	return job;
}
