#include "common/world/NavigationMap.hpp"
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
#include "common/core/Debug.hpp"
#include "common/world/Chunk.hpp"
#include "common/world/Player.hpp"
#include "common/world/World.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerItem.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerPlayer.hpp"
#include "server/world/ServerWorld.hpp"
#include "server/world/BucketInteraction.hpp"

bool ServerItem::onItemActivated(const glm::ivec3 &pos, Block &block, Player &player, World &world, ClientInfo &client, ServerCommandHandler &server, s8 face) const {
	if (block.stringID() == "default:campfire")
		return static_cast<const ServerBlock &>(block).onBlockActivated(pos, static_cast<ServerPlayer &>(player), static_cast<ServerWorld &>(world), client, server);
	if (hasGroup("group:om_map")) {
		ItemStack held = player.heldItemStack();
		if (held.stringID() == "default:map" && held.damage()) return true;
		u16 id = navigation::create(server.worldName(),player.x(),player.y(),player.dimension());
		if (!id) return false;
		ItemStack filled("default:map",1); filled.setDamage(id);
		Inventory inventory = player.inventory();
		if (held.amount() == 1) inventory.setItemStack(u16(player.heldItemSlot()),0,filled);
		else {
			held.setAmount(held.amount()-1);
			inventory.setItemStack(u16(player.heldItemSlot()),0,held);
			auto remainder = inventory.addItemStack(filled);
			if (remainder.amount() && !static_cast<ServerWorld &>(world).dropWorldItem({player.x(),player.y(),player.z()+1},"default:map",1,id,false)) return false;
		}
		player.inventory() = std::move(inventory);
		server.sendPlayerInvUpdate(client.id);
		return true;
	}
	if (hasGroup("group:om_minecart")) return static_cast<ServerWorld &>(world).placeMinecart(player, {pos.x, pos.y, pos.z});
	if (hasGroup("group:om_bucket")) return useBucket(*this, pos, face, player, static_cast<ServerWorld &>(world), server);
	if (u16 slot = getGroupValue("group:om_armor_slot")) {
		if (slot > 4) return false;
		ItemStack previous = player.armor().getStack(0, slot - 1);
		player.armor().setItemStack(0, slot - 1, player.heldItemStack());
		player.inventory().setItemStack(u16(player.heldItemSlot()), 0, previous);
		server.sendPlayerInvUpdate(client.id);
		return true;
	}
	if (hasGroup("group:om_igniter")) {
		auto &serverWorld = static_cast<ServerWorld &>(world);
		if (!serverWorld.primeTnt({pos.x, pos.y, pos.z}) && !serverWorld.ignitePortal({pos.x, pos.y, pos.z})) return false;
		server.wearHeldItem(player, 1);
		return true;
	}
	if (m_healAmount > 0) {
		if (player.isCreative()) return true;
		ItemStack stack = player.heldItemStack();
		if (stack.amount() == 0) return false;
		u16 remaining = stack.amount() - 1;
		std::string result = remaining ? stack.item().stringID() : "";
		if (!m_container.empty()) {
			if (remaining) {
				// preserve oversized food stacks loaded from older saves.
				if (player.inventory().addStack(m_container, 1).amount()) return false;
			}
			else {
				result = m_container;
				remaining = 1;
			}
		}
		player.inventory().setStack(player.heldItemSlot(), 0, result, remaining);
		player.heal(m_healAmount);
		server.sendPlayerInvUpdate(client.id);
		server.sendPlayerHealth(client.id);
		return true;
	}
	try {
		if (m_onItemActivated) {
			u16 previous = world.getBlock(pos.x, pos.y, pos.z);
			ItemStack held = player.heldItemStack();
			m_onItemActivated(pos, block, player, world, client, server);
			if (player.isCreative()) {
				player.inventory().setItemStack(u16(player.heldItemSlot()), 0, held);
				server.sendPlayerInvUpdate(client.id);
			}
			u16 type = getGroupValue("group:om_tool_type");
			if (type == 3 && previous != world.getBlock(pos.x, pos.y, pos.z)) {
				server.wearHeldItem(player, 1);
			}
			return true;
		}
	}
	catch (const sol::error &e) {
		logError() << e.what();
	}

	return false;
}

void ServerItem::setOnItemActivated(const sol::protected_function &function) {
	m_onItemActivated = function;
	m_canBeActivated = m_onItemActivated.valid();
}
