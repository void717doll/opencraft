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
#ifndef SERVERCOMMANDHANDLER_HPP_
#define SERVERCOMMANDHANDLER_HPP_

#include <unordered_map>

#include <entt/entt.hpp>
#include <sol/sol.hpp>

#include "common/core/Vector3.hpp"

struct BlockData;

struct AnimationComponent;
class ClientInfo;
class DrawableDef;
class Inventory;
class PlayerList;
class Player;
class Registry;
class ScriptEngine;
class Server;
class ServerPlayer;
class ServerWorld;
class WorldController;

//=================================================================================================
// IMPORTANT: ServerCommandHandler is the only class that should be creating and sending packets.
// The only exceptions to this rule are ServerWorld::sendRequestedChunks and LuaGUI::show
//=================================================================================================

class ServerCommandHandler {
	public:
		ServerCommandHandler(ScriptEngine &scriptEngine, Server &server, WorldController &worldController, PlayerList &players, Registry &registry)
			: m_scriptEngine(scriptEngine), m_server(server), m_worldController(worldController), m_players(players), m_registry(registry) {}

		void sendServerClosed(const std::string &message, ClientInfo *client = nullptr) const;
		void sendChunkUnload(s32 chunkX, s32 chunkY, s32 chunkZ, const ClientInfo *client = nullptr) const;
		void sendBlockDataUpdate(s32 x, s32 y, s32 z, const BlockData *blockData, const ClientInfo *client = nullptr) const;
		void sendBlockInvUpdate(s32 x, s32 y, s32 z, const Inventory &inventory, const ClientInfo *client = nullptr) const;
		void sendPlayerPosUpdate(u16 clientID, bool isTeleportation = false, const ClientInfo *client = nullptr) const;
		const std::string &worldName() const;
		void sendPlayerInvUpdate(u16 clientID, const ClientInfo *client = nullptr) const;
		void sendPlayerChangeDimension(u16 clientID, s32 x, s32 y, s32 z, u16 dimension, const ClientInfo *client = nullptr) const;
		void sendEntitySpawn(entt::entity entityID, const ClientInfo *client = nullptr) const;
		void sendEntityDespawn(entt::entity entityID, const ClientInfo *client = nullptr) const;
		void sendSound(const std::string &name, const Vector3d &position, u16 dimension, float gain = 1, float pitch = 1, const ClientInfo *client = nullptr) const;

		void sendPlayerHealth(u16 clientID) const;
		void sendParticles(Vector3d position, u16 dimension, u8 effect) const;
		bool usePortal(u16 clientID, s32 x, s32 y, s32 z);
		void useBed(u16 clientID, s32 x, s32 y, s32 z);
		void sendPlayerMessage(u16 clientID, const std::string &message) const;
		bool consumeHeldItem(Player &player, u16 amount, u16 clientID);
		void wearHeldItem(Player &player, u16 amount);
		void damagePlayer(u16 clientID, u16 amount, const std::string &cause = "damage");
		void sendEntityHurt(entt::entity entity, u16 amount, double x, double y, double z) const;
		void respawnPlayer(u16 clientID);
		void sendPlayerScore(u16 clientID) const;

		void setupCallbacks();

		void setPlayerPosition(u16 clientID, s32 x, s32 y, s32 z);


		const Server &server() const { return m_server; }

		static void initUsertype(sol::state &lua);

	private:
		ServerWorld &getWorldForClient(u16 clientID);

		ScriptEngine &m_scriptEngine;

		Server &m_server;
		WorldController &m_worldController;
		PlayerList &m_players;

		Registry &m_registry;

};

#endif // SERVERCOMMANDHANDLER_HPP_
