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
#ifndef SERVERWORLD_HPP_
#define SERVERWORLD_HPP_

#include <deque>
#include <unordered_map>
#include <unordered_set>

#include "common/world/World.hpp"
#include "server/scene/ServerScene.hpp"
#include "server/world/BlockPhysics.hpp"
#include "server/world/ServerChunk.hpp"
#include "server/world/TerrainGenerator.hpp"

class Dimension;
class PlayerList;
class ServerCommandHandler;
class ServerPlayer;
class Player;

class ServerWorld : public World {
	using ChunkMap = std::unordered_map<Vector3i, std::unique_ptr<ServerChunk>>;

	public:
		ServerWorld(PlayerList &players, const Dimension &dimension, s32 seed);

		void update(bool doTick);
		bool isCreative() const { return m_creative; }
		void setCreative(bool creative) { m_creative = creative; }
		BlockPhysics &blockPhysics() { return m_blockPhysics; }
		void updateMobs();
		void updateCopperOxidation();
		void tickSpawner(int x, int y, int z);
		void useJukebox(int x, int y, int z, ServerPlayer &player);
		void stopJukebox(int x, int y, int z);
		void initializeChest(int x, int y, int z);
		std::vector<glm::ivec3> prepareChest(int x, int y, int z, ServerPlayer &player);
		bool growTree(int x, int y, int z, const std::string &treeID);
		void updateItems();
		void updateTnt();
		void updateMinecarts();
		void refreshRail(const Vector3i &point);
		void refreshPoweredRail(const Vector3i &point);
		bool railDirectlyPowered(const Vector3i &point) const;
		void watchRailDevice(const Vector3i &point);
		void updateRailDevices();
		unsigned redstonePower(const Vector3i &point, bool includeWire = true) const;
		void queueRedstone(const Vector3i &point);
		void notifyRedstone(const Vector3i &point);
		void refreshRedstone(const Vector3i &point);
		void scheduleRedstone(const Vector3i &point, u16 state, u16 delay);
		void updateRedstoneCircuits();
		void refreshBetaDevice(const Vector3i &point);
		void playNote(int x, int y, int z, bool tune);
		void removePiston(int x, int y, int z);
		void breakPistonHead(int x, int y, int z);
		bool activateRedstone(const Vector3i &point);
		bool placeRedstoneDust(Player &player, const Vector3i &point, s8 face);
		bool placeMinecart(Player &player, const Vector3i &point);
		bool interactMinecart(ServerPlayer &player);
		bool attackMinecart(ServerPlayer &player);
		bool dismountMinecart(ServerPlayer &player);
		bool ignitePortal(const Vector3i &clicked);
		bool portalFrameAt(Vector3i point, Vector3i &origin, Vector3i &axis) const;
		void validatePortal(int x, int y, int z);
		bool findPortalDestination(Vector3i requested, Vector3i &arrival);
		bool primeTnt(const Vector3i &position, u16 fuse = 80);
		bool placeBed(ServerPlayer &player, Vector3i foot, u16 &data);
		void removeBedPartner(int x, int y, int z);
		void validateBed(int x, int y, int z);
		bool bedRespawn(Vector3i bed, Vector3d &arrival, bool loadChunks = false);
		std::string sleepInBed(ServerPlayer &player, Vector3i position);
		void explodeAt(const Vector3d &position, float strength, u32 seed, const std::string &source);
		bool dropWorldItem(Vector3d position, const std::string &id, u16 count = 1, u16 damage = 0, bool limited = true);
		void dropContainerItems(Vector3i position, const Inventory &inventory);
		void updatePlayerSurvival();
		bool moveSurvivalPlayer(ServerPlayer &player, double x, double y, double z);
		bool spawnMob(const std::string &id, double x, double y, double z, u8 slimeSize = 0, entt::entity *spawned = nullptr);
		bool spawnProjectile(const std::string &id, Vector3d position, Vector3d direction);
		bool tryCreateSnowGolem(const Vector3i &pumpkin);
		bool attackMob(ServerPlayer &player);
		void dropPlayerItems(ServerPlayer &player);
		bool dropHeldItem(ServerPlayer &player);
		bool dropPlayerStack(ServerPlayer &player, const ItemStack &stack);
		bool interactMob(ServerPlayer &player);
		entt::entity findMobTarget(ServerPlayer &player, double reach);

		void updatePlayerChunks(ServerPlayer &player, s32 cx, s32 cy, s32 cz);
		void createChunkNeighbours(ServerChunk &chunk);

		ServerChunk &getOrCreateChunk(s32 cx, s32 cy, s32 cz);

		void appendCollisionBoxes(const DoubleBox &moving, const DoubleBox &broadphase, std::vector<DoubleBox> &boxes) const override;
		Chunk *getChunk(int cx, int cy, int cz) const override;

		bool generateChunk(ServerChunk &chunk);
		void initializeChunkLighting(ServerChunk &chunk);
		void tickActiveChunks();

		const PlayerList &players() const { return m_players; }
		const Dimension &dimension() const { return m_dimension; }
		const ChunkMap &chunks() const { return m_chunks; }

		Heightmap &heightmap() { return m_heightmap; }
		TerrainGenerator &terrainGenerator() { return m_terrainGenerator; }
		ServerScene &scene() { return m_scene; }

		ServerCommandHandler *server() const { return m_server; }
		void setServer(ServerCommandHandler *server) { m_server = server; m_scene.setServer(server); }

		s32 seed() const { return m_seed; }
		void setSeed(s32 seed) { m_seed = seed; m_terrainGenerator.setSeed(seed); m_heightmap.setSeed(seed); }
		u16 generationRevision() const { return m_terrainGenerator.generationRevision(); }
		void setGenerationRevision(u16 revision) { m_terrainGenerator.setGenerationRevision(revision); }

		static void initUsertype(sol::state &lua);

	private:
		struct RedstoneTimer { u16 block; u16 state; u64 due; };
		std::deque<Vector3i> m_redstoneQueue, m_redstoneTimerQueue;
		std::unordered_set<Vector3i> m_redstonePending;
		std::unordered_map<Vector3i,RedstoneTimer> m_redstoneTimers;
		u64 m_redstoneTicks = 0;
		std::unordered_set<Vector3i> m_railDevices;
		std::deque<Vector3i> m_railDeviceQueue;
		std::unordered_map<Vector3i,std::deque<u64>> m_torchSwitches;
		u64 m_railPowerTicks = 0;
		bool m_creative = false;
		void processSendRequests();
		void spawnNetherMobs();
		void spawnCaveMobs();

		PlayerList &m_players;

		const Dimension &m_dimension;

		ChunkMap m_chunks;
		std::unordered_map<Vector2i, std::vector<ServerChunk *>> m_chunkColumns;

		Heightmap m_heightmap;
		TerrainGenerator m_terrainGenerator;

		ServerCommandHandler *m_server = nullptr;

		ServerScene m_scene;
		BlockPhysics m_blockPhysics{*this};
		bool m_movingPiston = false;

		s32 m_seed = 0;
		u32 m_mobTick = 0;
		u16 m_copperAgeTicks = 0;
		u32 m_copperRandom = 0;
		std::size_t m_activeChunkCursor = 0;

		struct ChunkSendRequest {
			ChunkSendRequest(const Vector3i &chunkPos_, ServerPlayer &player_, u64 timestamp_)
				: chunkPos(chunkPos_), player(player_), timestamp(timestamp_) {}

			Vector3i chunkPos;
			ServerPlayer &player;
			u64 timestamp;
		};

		std::queue<ChunkSendRequest> m_chunkSendRequestQueue;
		std::unordered_set<Vector3i> m_queuedChunks;
};

#endif // SERVERWORLD_HPP_
