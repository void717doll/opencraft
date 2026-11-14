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
	m_uploads.clear();
	m_active.clear();
	m_urgent.clear();
	for (auto &future : m_futures) { auto *job = future.get(); delete job; }
	m_futures.clear();
	m_pending.clear();
	m_pendingSet.clear();
	m_generations.clear();
}

void ChunkMeshBuilder::remove(const Vector3i &position) {
	m_generations.erase(position);
	m_urgent.erase(position);
	m_pendingSet.erase(position);
	m_pending.erase(std::remove(m_pending.begin(), m_pending.end(), position), m_pending.end());
}

void ChunkMeshBuilder::addMeshBuildingJob(const Chunk &chunk, const TextureAtlas &, bool urgent) {
	Vector3i position{chunk.x(), chunk.y(), chunk.z()};
	m_generations.emplace(position,++m_nextGeneration);
	if (urgent) {
		m_urgent.emplace(position);
		for (auto it=m_uploads.begin(); it!=m_uploads.end();) {
			const auto &job=*(*it)->job;
			if (!job.urgent && Vector3i{job.chunkData.x,job.chunkData.y,job.chunkData.z} == position) {
				m_active.erase(position); it=m_uploads.erase(it);
			} else ++it;
		}
		m_pending.erase(std::remove(m_pending.begin(),m_pending.end(),position),m_pending.end());
		m_pendingSet.emplace(position);
		m_pending.push_front(position);
	} else if (m_pendingSet.emplace(position).second) m_pending.push_back(position);
}

void ChunkMeshBuilder::update() {
	using Clock = std::chrono::steady_clock;
	auto deadline = Clock::now() + std::chrono::milliseconds(2);
	unsigned completed = 0;
	for (auto it = m_futures.begin(); it != m_futures.end() && completed < 2 && Clock::now() < deadline;) {
		if (it->future().wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) { ++it; continue; }
		std::unique_ptr<ChunkMeshBuildingJob> job{it->get()};
		Vector3i position{job->chunkData.x,job->chunkData.y,job->chunkData.z};
		if (m_urgent.count(position) && !job->urgent) {
			m_active.erase(position); it=m_futures.erase(it); ++completed; continue;
		}
		auto upload = std::make_unique<Upload>();
		upload->job = std::move(job);
		if (upload->job->urgent) m_uploads.push_front(std::move(upload));
		else m_uploads.push_back(std::move(upload));
		it = m_futures.erase(it);
		++completed;
	}
	std::size_t bytes = 0;
	std::size_t byteBudget = !m_uploads.empty() && m_uploads.front()->job->urgent ? 256*1024 : 64*1024;
	while (!m_uploads.empty() && bytes < byteBudget && Clock::now() < deadline) {
		auto &upload = *m_uploads.front();
		auto &job = *upload.job;
		Vector3i position{job.chunkData.x,job.chunkData.y,job.chunkData.z};
		auto generation = m_generations.find(position);
		auto *chunk = static_cast<ClientChunk *>(m_world.getChunk(position.x,position.y,position.z));
		if (!chunk || generation == m_generations.end() || generation->second != job.generation
		 || (m_urgent.count(position) && !job.urgent)) {
			m_active.erase(position); m_uploads.pop_front(); continue;
		}
		// finish this snapshot even if newer edits arrive, so busy chunks keep progressing.
		if (!upload.initialized) {
			upload.buffer.setupDefaultLayout();
			if (job.totalVertexCount) upload.buffer.init(nullptr,u32(job.totalVertexCount*sizeof(Vertex)));
			upload.initialized = true;
		}
		while (upload.layer < ChunkMeshLayer::Count && bytes < byteBudget && Clock::now() < deadline) {
			const auto &vertices = job.vertices[upload.layer];
			u32 count = std::min<u32>(u32(vertices.size())-upload.vertex, u32((byteBudget-bytes)/sizeof(Vertex)));
			if (count) {
				upload.buffer.update(vertices.data()+upload.vertex,count*sizeof(Vertex),upload.offset*sizeof(Vertex));
				upload.vertex += count; upload.offset += count; bytes += count*sizeof(Vertex);
			}
			if (upload.vertex == vertices.size()) { ++upload.layer; upload.vertex = 0; }
			else if (!count) break;
		}
		if (upload.layer < ChunkMeshLayer::Count) break;
		chunk->getVertexBuffer() = std::move(upload.buffer);
		for (u8 layer = 0; layer < ChunkMeshLayer::Count; ++layer) chunk->setVerticesCount(layer,job.vertices[layer].size());
		m_active.erase(position);
		m_uploads.pop_front();
	}
	std::size_t examined = m_pending.size();
	unsigned submitted = 0;
	while (examined-- && !m_pending.empty() && m_futures.size() + m_uploads.size() < (m_urgent.empty() ? 4u : 5u) && submitted < 2 && Clock::now() < deadline) {
		Vector3i position = m_pending.front();
		m_pending.pop_front();
		if (m_active.count(position)) {
			if (m_urgent.count(position)) { m_pending.push_front(position); break; }
			m_pending.push_back(position); continue;
		}
		m_pendingSet.erase(position);
		const Chunk *chunk = m_world.getChunk(position.x, position.y, position.z);
		auto generation = m_generations.find(position);
		if (!chunk || generation == m_generations.end()) continue;
		auto job = std::make_unique<ChunkMeshBuildingJob>();
		job->generation = generation->second;
		job->urgent = m_urgent.erase(position) != 0;
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
