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
#ifndef CLIENTCOMMANDHANDLER_HPP_
#define CLIENTCOMMANDHANDLER_HPP_

#include <unordered_map>

#include <entt/entt.hpp>

#include "common/core/IntTypes.hpp"
#include "common/network/Network.hpp"
#include <glm/glm.hpp>

class Inventory;
class ItemStack;

class Camera;
class Client;
class ClientPlayer;
class ClientWorld;

class ClientCommandHandler {
	public:
		void sendPlayerNoclip(bool enabled);
		void requestStatistics();
		void sendCraftedItem(const std::string &name, u16 amount);
		ClientCommandHandler(Client &client, ClientWorld &world, ClientPlayer &player)
			: m_client(client), m_world(world), m_player(player) {}

		void sendPlayerInvUpdate();
		void sendPlayerPosUpdate() const;
		void sendPlayerRotUpdate();
		void sendPlayerDigBlock(const glm::ivec4 &selectedBlock);
		void sendNotePreview(const glm::ivec4 &selectedBlock);
		void sendPlayerPlaceBlock(s32 x, s32 y, s32 z, u32 block);
		void sendPlayerSwapHands();
		void sendPlayerHeldItemChanged(u8 hotbarSlot, u16 itemID);
		void sendPlayerReady();
		void sendPlayerAttack();
		void sendPlayerInteract();
		void sendPlayerDismount() const;
		void sendPlayerDropItem();
		void sendPlayerRespawn();
		void sendPlayerPause(bool paused);
		void sendPlayerDropStack(const ItemStack &stack);
		void sendPlayerChunkPosUpdate() const;
		void sendBlockActivated(const glm::ivec4 &selectedBlock);
		void sendBlockInvUpdate(Inventory &inventory);
		void sendMinecartInvUpdate(Inventory &inventory);
		bool minecartInReach(u32 id) const;
		void setMinecartInventory(Inventory *inventory) { m_minecartInventory = inventory; }
		void clearMinecartInventory(const Inventory *inventory) { if (m_minecartInventory == inventory) m_minecartInventory = nullptr; }
		void sendItemActivated(const glm::ivec4 &selectedBlock);
		void sendKeyPressed(u16 keyID);

		void setupCallbacks();

		Client &client() { return m_client; }

		bool isRegistryInitialized() const { return m_isRegistryInitialized; }


		using EntityMap = std::unordered_map<entt::entity, entt::entity>;

	private:
		Client &m_client;
		ClientWorld &m_world;
		ClientPlayer &m_player;

		EntityMap m_entityMap;
		Inventory *m_minecartInventory = nullptr;

		bool m_isRegistryInitialized = false;

};

#endif // CLIENTCOMMANDHANDLER_HPP_
