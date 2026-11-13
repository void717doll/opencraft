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
#include <random.hpp>

#include "common/core/Registry.hpp"
#include "common/world/BlockState.hpp"
#include "common/world/BetaMining.hpp"

bool BlockState::canHarvest(const Item &item) const {
	return retro::canHarvest(*this, item);
}

ItemStack BlockState::getItemDrop(const Item *tool) const {
	const auto drops = getItemDrops(tool);
	return drops.empty() ? ItemStack::Empty : drops.front();
}

std::vector<ItemStack> BlockState::getItemDrops(const Item *tool) const {
	using Random = effolkronium::random_static;
	const std::string &id = block().stringID();
	u16 cropAge = block().getGroupValue("group:om_crop");
	if (cropAge) {
		std::vector<ItemStack> drops;
		if (m_id >= cropAge && itemDropAmount() && Registry::getInstance().hasItem(itemDrop()))
			drops.emplace_back(itemDrop(), itemDropAmount());
		u16 seeds = 0;
		for (int attempt = 0; attempt < 3; ++attempt)
			if (Random::get(0, 14) <= m_id) ++seeds;
		const std::string &seed = block().getState(0).itemDrop();
		if (seeds && Registry::getInstance().hasItem(seed)) drops.emplace_back(seed, seeds);
		return drops;
	}
	std::string drop = itemDrop();
	u16 amount = itemDropAmount();
	bool grass = id == "default:tallgrass" || id == "default:fern";
	if (tool && tool->stringID() == "default:shears" && (grass || drawType() == BlockDrawType::Leaves)) {
		drop = id; amount = 1;
	}
	else if (id == "default:diamond_ore") { drop = "default:diamond"; amount = 1; }
	else if (id == "default:redstone_ore") { drop = "default:redstone_dust"; amount = u16(Random::get(4, 5)); }
	else if (id == "default:lapis_ore") { drop = "default:lapis_lazuli"; amount = u16(Random::get(4, 8)); }
	else if (grass) {
		if (Random::get(1, 8) != 1) return {};
		drop = "default:seeds"; amount = 1;
	}
	else if (id == "default:oak_leaves") {
		if (Random::get(1, 20) != 1) return {};
		drop = "default:sapling"; amount = 1;
	}
	else if (id == "default:cherry_leaves" || id == "default:birch_leaves" || id == "default:spruce_leaves"
	 || id == "default:jungle_leaves" || id == "default:acacia_leaves" || id == "default:dark_oak_leaves") {
		if (Random::get(1, id == "default:jungle_leaves" ? 40 : 20) != 1) return {};
		drop = id.substr(0, id.size() - 7) + "_sapling"; amount = 1;
	}
	else if (id == "default:deadbush") return {};
	if (drop.empty() || !amount || !Registry::getInstance().hasItem(drop)) return {};
	return {{drop, amount}};
}

void BlockState::serialize(sf::Packet &packet) const {
	packet << m_id << m_label << m_tiles
		<< m_itemDrop << m_itemDropAmount
		<< m_hardness << m_harvestRequirements
		<< m_boundingBox << u8(m_drawType) << m_colorMultiplier
		<< m_isOpaque << m_isLightSource
		<< m_inventoryImage << m_fogDepth << m_fogColor
		<< m_drawOffset << m_isCollidable << m_attrs;
}

void BlockState::deserialize(sf::Packet &packet) {
	u8 drawType;

	packet >> m_id >> m_label >> m_tiles
		>> m_itemDrop >> m_itemDropAmount
		>> m_hardness >> m_harvestRequirements
		>> m_boundingBox >> drawType >> m_colorMultiplier
		>> m_isOpaque >> m_isLightSource
		>> m_inventoryImage >> m_fogDepth >> m_fogColor
		>> m_drawOffset >> m_isCollidable >> m_attrs;

	m_drawType = BlockDrawType(drawType);
}

bool BlockState::isOpaque() const {
	bool isOpaque = (!m_defaultState || m_attrs & BlockAttribute::attr_isOpaque) ? m_isOpaque : m_defaultState->m_isOpaque;
	return m_block && m_block->id() != 0 && isOpaque && drawType() != BlockDrawType::XShape;
}

// Please update 'docs/lua-api-cpp.md' if you change this
void BlockState::initUsertype(sol::state &lua) {
	lua.new_usertype<BlockState>("BlockState",
		"id", &BlockState::id,
		"block", &BlockState::block,
		"label", (const std::string &(BlockState::*)() const)&BlockState::label,
		"is_opaque", [](const BlockState &state) { return state.isOpaque(); },
		"is_collidable", (const bool &(BlockState::*)() const)&BlockState::isCollidable,
		"get_item_drop", sol::overload(
			[](const BlockState &state) { return state.getItemDrop(); },
			[](const BlockState &state, const Item &tool) { return state.getItemDrop(&tool); }),
		"get_item_drops", sol::overload(
			[](const BlockState &state) { return sol::as_table(state.getItemDrops()); },
			[](const BlockState &state, const Item &tool) { return sol::as_table(state.getItemDrops(&tool)); }),
		"can_harvest", &BlockState::canHarvest
	);
}

