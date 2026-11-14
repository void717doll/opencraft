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
#ifndef PLAYER_HPP_
#define PLAYER_HPP_

#include "common/world/PlayerStatistics.hpp"

#include "common/core/Box.hpp"
#include "common/core/ISerializable.hpp"
#include "common/inventory/Inventory.hpp"

class Player : public ISerializable {
	public:
		Player();

		const std::string &name() const { return m_name; }

		// West, East, South, North
		// Same order as enum BlockFace in TilesDef.hpp
		u8 getDirection() const;
		u8 getOppositeDirection() const;

		Vector3i getCurrentChunk() const;

		void serialize(sf::Packet &packet) const override;
		void deserialize(sf::Packet &packet) override;

		double x() const { return m_x; }
		double y() const { return m_y; }
		double z() const { return m_z; }

		u16 dimension() const { return m_dimension; }

		float cameraYaw()   const { return m_viewAngleH; }
		float cameraPitch() const { return m_viewAngleV; }
		float cameraRoll()  const { return m_viewAngleRoll; }

		u16 clientID() const { return m_clientID; }

		Inventory &inventory() { return m_inventory; }
		Inventory &offhand() { return m_offhand; }
		const Inventory &offhand() const { return m_offhand; }
		Inventory &heldInventory() { return m_usingOffhand ? m_offhand : m_inventory; }
		u16 activeItemSlot() const { return m_usingOffhand ? 0 : u16(m_heldItemSlot); }
		bool shouldUseOffhand() const;
		const ItemStack &useItemStack() const;
		void setUsingOffhand(bool value) { m_usingOffhand = value; }
		bool swapHands();
		Inventory &armor() { return m_armor; }
		const Inventory &armor() const { return m_armor; }
		u16 armorValue() const;
		u16 absorbDamage(u16 amount, const std::string &cause = "");

		void setName(const std::string &name) { m_name = name; }
		void setPosition(double x, double y, double z) { m_x = x; m_y = y; m_z = z; }
		void setRotation(float yaw, float pitch) { m_viewAngleH = yaw; m_viewAngleV = pitch; }
		void setDimension(u16 dimension) { m_dimension = dimension; }
		void setClientID(u16 clientID) { m_clientID = clientID; }

		const FloatBox &hitbox() const { return m_hitbox; }

		const ItemStack &heldItemStack() { return heldInventory().getStack(activeItemSlot(), 0); }
		s8 heldItemSlot() const { return m_heldItemSlot; }
		void setHeldItemSlot(u8 heldItemSlot) { m_heldItemSlot = heldItemSlot; }
		bool pickItem(const std::string &itemID);

		PlayerStatistics &statistics() { return m_statistics; }
		const PlayerStatistics &statistics() const { return m_statistics; }

		bool isCreative() const { return m_creative; }
		void setCreative(bool creative) { m_creative = creative; if (creative) m_health = 20; else m_noclip = false; }
		bool isNoclip() const { return m_creative && m_noclip; }
		void setNoclip(bool enabled) { m_noclip = m_creative && enabled; }

		u16 health() const { return m_health; }
		s32 score() const { return m_score; }
		void setScore(s32 score) { m_score = score; }
		void setHealth(u16 health) { m_health = std::min<u16>(health, 20); }
		void heal(u16 amount) { setHealth(u16(std::min<unsigned>(20, m_health + amount))); }
		void damage(u16 amount) { if (m_creative) return; m_health = amount >= m_health ? 0 : m_health - amount; }

		static void initUsertype(sol::state &lua);

	protected:
		PlayerStatistics m_statistics;
		bool m_creative = false;
		bool m_noclip = false;
		u16 m_health = 20;
		s32 m_score = 0;
		std::string m_name;

		double m_x = 0;
		double m_y = 0;
		double m_z = 0;

		u16 m_dimension = 0;

		float m_viewAngleH = -90;
		float m_viewAngleV = 0;
		float m_viewAngleRoll = 0;

		u16 m_clientID = 0;

		Inventory m_inventory{9, 4};
		Inventory m_armor{1, 4, "armor"};
		Inventory m_offhand{1, 1, "offhand"};
		bool m_usingOffhand = false;
		u16 m_armorRemainder = 0;

		FloatBox m_hitbox;

		s8 m_heldItemSlot = -1;
};

#endif // PLAYER_HPP_
