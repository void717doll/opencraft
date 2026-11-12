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
#include "common/core/Registry.hpp"
#include "common/network/Network.hpp"
#include "common/world/Player.hpp"

Player::Player() {
	m_hitbox = FloatBox{-0.3f, -0.3f, 0, 0.6f, 0.6f, 1.8f};
}

bool Player::pickItem(const std::string &itemID) {
	auto &registry = Registry::getInstance();
	if (!registry.hasItem(itemID) || !registry.getItemFromStringID(itemID).id()
	 || !m_inventory.width() || !m_inventory.height()) return false;
	u16 width = m_inventory.width();
	for (u16 slot = 0; slot < width; ++slot) {
		const auto &stack = m_inventory.getStack(slot, 0);
		if (stack.amount() && stack.stringID() == itemID) {
			setHeldItemSlot(u8(slot));
			return true;
		}
	}
	u16 target = m_heldItemSlot >= 0 && m_heldItemSlot < width ? u16(m_heldItemSlot) : 0;
	if (m_inventory.getStack(target, 0).amount()) {
		for (u16 slot = 0; slot < width; ++slot) {
			if (!m_inventory.getStack(slot, 0).amount()) { target = slot; break; }
		}
	}
	for (u16 y = 1; y < m_inventory.height(); ++y) {
		for (u16 x = 0; x < width; ++x) {
			ItemStack owned = m_inventory.getStack(x, y);
			if (!owned.amount() || owned.stringID() != itemID) continue;
			m_inventory.setItemStack(x, y, m_inventory.getStack(target, 0));
			m_inventory.setItemStack(target, 0, owned);
			setHeldItemSlot(u8(target));
			return true;
		}
	}
	if (!isCreative()) return false;
	m_inventory.setStack(target, 0, itemID, registry.getItemFromStringID(itemID).maxStackSize());
	setHeldItemSlot(u8(target));
	return true;
}

u16 Player::armorValue() const {
	unsigned protection = 0, maximum = 0, remaining = 0;
	for (const auto &stack : m_armor.items()) {
		if (!stack.amount()) continue;
		u16 durability = stack.maxDamage();
		protection += stack.item().getGroupValue("group:om_armor_points");
		maximum += durability;
		remaining += durability > stack.damage() ? durability - stack.damage() : 0;
	}
	return maximum && protection ? u16((protection - 1) * remaining / maximum + 1) : 0;
}

u16 Player::absorbDamage(u16 amount, const std::string &cause) {
	unsigned bonus = 0;
	bool projectile = cause == "arrow" || cause.compare(0, 6, "arrow:") == 0
		|| cause == "projectile" || cause.compare(0, 11, "projectile:") == 0;
	for (const auto &stack : m_armor.items()) {
		if (!stack.amount() || !stack.maxDamage() || stack.damage() >= stack.maxDamage()) continue;
		bonus += stack.item().getGroupValue("group:om_damage_reduction");
		if (projectile) bonus += stack.item().getGroupValue("group:om_projectile_reduction");
		if (cause == "fall") bonus += stack.item().getGroupValue("group:om_fall_reduction");
		if (cause == "fire" || cause == "lava") bonus += stack.item().getGroupValue("group:om_fire_reduction");
	}
	unsigned reduced = unsigned(amount) * (25 - std::min<u16>(20, armorValue()))
		* (100 - std::min(80u, bonus)) + m_armorRemainder;
	for (u16 slot = 0; slot < 4; ++slot) {
		if (m_armor.getStackRef(0, slot).wear(amount)) m_armor.setChanged(true);
	}
	m_armorRemainder = u16(reduced % 2500);
	return u16(reduced / 2500);
}

// Note: This function returns an angle4
u8 Player::getDirection() const {
	return int(floorf(m_viewAngleH / 90.f + 0.5f)) & 3;
}

// Note: This function returns an angle4
u8 Player::getOppositeDirection() const {
	return getDirection() ^ 2;
}

Vector3i Player::getCurrentChunk() const {
	return {
		(static_cast<s32>(std::floor(m_x)) & -CHUNK_WIDTH)  / CHUNK_WIDTH,
		(static_cast<s32>(std::floor(m_y)) & -CHUNK_DEPTH)  / CHUNK_DEPTH,
		(static_cast<s32>(std::floor(m_z)) & -CHUNK_HEIGHT) / CHUNK_HEIGHT
	};
}

void Player::serialize(sf::Packet &packet) const {
	packet << m_x << m_y << m_z << m_dimension << m_viewAngleH << m_viewAngleV << m_viewAngleRoll << m_inventory << m_heldItemSlot;
}

void Player::deserialize(sf::Packet &packet) {
	packet >> m_x >> m_y >> m_z >> m_dimension >> m_viewAngleH >> m_viewAngleV >> m_viewAngleRoll >> m_inventory >> m_heldItemSlot;
}

// Please update 'docs/lua-api-cpp.md' if you change this
void Player::initUsertype(sol::state &lua) {
	lua.new_usertype<Player>("Player",
		"name", &Player::name,
		"health", &Player::health,
		"heal", &Player::heal,
		"damage", &Player::damage,

		"x", &Player::x,
		"y", &Player::y,
		"z", &Player::z,
		"set_position", &Player::setPosition,

		"dimension", &Player::dimension,
		"set_dimension", &Player::setDimension,

		"client_id", &Player::clientID,

		"inventory", &Player::inventory,

		"held_item_stack", &Player::heldItemStack
	);
}
