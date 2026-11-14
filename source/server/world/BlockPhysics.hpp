#ifndef BLOCKPHYSICS_HPP_
#define BLOCKPHYSICS_HPP_

#include <deque>
#include <unordered_set>
#include <unordered_map>

#include "common/core/Vector3.hpp"

class Block;
class ServerChunk;
class ServerWorld;

class BlockPhysics {
	public:
		explicit BlockPhysics(ServerWorld &world) : m_world(world) {}

		void enqueue(const Vector3i &position);
		void enqueueNeighbors(const Vector3i &position);
		void markLeaves(const Vector3i &position, int radius);
		void seedChunk(ServerChunk &chunk);
		void forgetChunk(const Vector3i &position);
		std::size_t tick(std::size_t budget = 256);
		std::size_t pendingCount() const { return m_pending.size(); }

	private:
		bool loaded(const Vector3i &position) const;
		bool replaceable(const Vector3i &position) const;
		void write(const Vector3i &position, u16 block, u16 data = 0);
		void seedNode(const Vector3i &position);
		void update(const Vector3i &position);
		void updateLeaves(const Vector3i &position);
		void updateFluid(const Vector3i &position);
		void startFalling(const Vector3i &position);
		void updateFalling();
		void dropAttached(const Vector3i &position);

		ServerWorld &m_world;
		std::deque<Vector3i> m_queue;
		std::unordered_set<Vector3i> m_pending;
		std::unordered_map<Vector3i, std::unordered_set<Vector3i>> m_waitingLeaves;
};

#endif // BLOCKPHYSICS_HPP_
