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
#ifndef SERVERCHUNK_HPP_
#define SERVERCHUNK_HPP_

#include <random.hpp>
#include <set>

#include "common/core/IntTypes.hpp"
#include "common/world/Chunk.hpp"

using Random_t = effolkronium::random_local;

class ClientInfo;
class ServerBlock;
class ServerPlayer;
class ServerWorld;

class ServerChunk : public Chunk {
	public:
		ServerChunk(s32 x, s32 y, s32 z, ServerWorld &world);

		void update() final;
		void process() final;

		void onBlockChanged(int x, int y, int z) override;
		void onBlockPlaced(int x, int y, int z, const Block &block) override;
		void onBlockDestroyed(int x, int y, int z, const Block &block) override;
		void onBlockDataRemoved(int x, int y, int z, const BlockData &data) override;

		void tick(unsigned budget = 64);
		void initializeTickingBlocks();
		Random_t &random() { return m_random; }

		void sendData(const ClientInfo &client);
		void sendChanges(const ClientInfo &client);
		void sendMetadata(const ClientInfo &client);

		bool hasBeenModified() const { return m_hasBeenModified; }
		void setModified(bool hasBeenModified) { m_hasBeenModified = hasBeenModified; if (hasBeenModified) m_needsSave = true; }
		bool needsSave() const { return m_needsSave; }
		void setSaved() { m_needsSave = false; }

		void setReadyToSend() { m_isReadyToSend = true; m_sentCells.clear(); }

		void addTickingBlock(int x, int y, int z, const ServerBlock &) { m_tickingBlocks.emplace((z * depth + y) * width + x); }

	private:
		ServerWorld &m_world;

		std::vector<u64> m_sentCells;
		bool m_hasBeenModified = false;
		bool m_needsSave = true;
		bool m_isReadyToSend = false;

		Random_t m_random;

		std::set<u16> m_tickingBlocks;
		u16 m_tickCursor = 0;
};

#endif // SERVERCHUNK_HPP_
