--
-- =====================================================================================
--
--  OpenMiner
--
--  Copyright (C) 2018-2020 Unarelith, Quentin Bazin <openminer@unarelith.net>
--  Copyright (C) 2019-2020 the OpenMiner contributors (see CONTRIBUTORS.md)
--
--  This file is part of OpenMiner.
--
--  OpenMiner is free software; you can redistribute it and/or
--  modify it under the terms of the GNU Lesser General Public
--  License as published by the Free Software Foundation; either
--  version 2.1 of the License, or (at your option) any later version.
--
--  OpenMiner is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
--  Lesser General Public License for more details.
--
--  You should have received a copy of the GNU Lesser General Public License
--  along with OpenMiner; if not, write to the Free Software Foundation, Inc.,
--  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
--
-- =====================================================================================
--
local modpath = mod:path()

local function show_inventory(keyID, client)
	local gui = LuaGUI.new(keyID)

	gui:set_size(176, 166)
	gui:set_centered(true)

	gui:image {
		name = "img_background",
		pos = {x = 0, y = 0},

		texture = modpath .. "/textures/gui/inventory.png",
		size = {x = 176, y = 166},
		border = 5,
	}

	gui:image {
		name = "img_crafting_arrow",
		pos = {x = 129, y = 35},
		texture = modpath .. "/textures/gui/workbench.png",
		clip = {x = 468, y = 158, width = 73, height = 58},
		size = {x = 18, y = 17},
	}

	for slot, piece in ipairs({"helmet", "chestplate", "leggings", "boots"}) do
		gui:inventory {
			name = "armor_" .. piece,
			pos = {x = 7, y = 7 + (slot - 1) * 18},
			inventory = {source = "player", player = "player", inventory_name = "armor", offset = slot - 1, count = 1},
			size = {x = 1, y = 1},
			filter = "group:om_armor_" .. piece,
			shift_destination = "inv_main,inv_hotbar",
		}
	end

	gui:inventory {
		name = "inv_offhand", pos = {x = 76, y = 61},
		inventory = {source = "player", player = "player", inventory_name = "offhand", offset = 0, count = 1},
		size = {x = 1, y = 1}, shift_destination = "inv_main,inv_hotbar",
	}

	gui:inventory {
		name = "inv_main",
		pos = {x = 7, y = 83},

		inventory = {
			source = "player",
			player = "player",
			inventory_name = "main",
			offset = 9,
			count = 9 * 3,
		},

		size = {x = 9, y = 3},

		shift_destination = "armor_helmet,armor_chestplate,armor_leggings,armor_boots,inv_hotbar,inv_main",
	}

	gui:inventory {
		name = "inv_hotbar",
		pos = {x = 7, y = 141},

		inventory = {
			source = "player",
			player = "player",
			inventory_name = "main",
			offset = 0,
			count = 9,
		},

		size = {x = 9, y = 1},

		shift_destination = "armor_helmet,armor_chestplate,armor_leggings,armor_boots,inv_main,inv_hotbar",
	}

	gui:crafting {
		name = "inv_crafting",
		pos = {x = 91, y = 25},
		result_pos = {x = 151, y = 34},

		inventory = {
			source = "temp",
			size = 2,
		},

		shift_destination = "inv_main,inv_hotbar",
	}

	gui:show(client)
end

mod:key {
	id = "inventory", name = "Inventory", default_key = "E",
	callback = function(keyID, client)
		if client.creative then show_creative_inventory(keyID, client)
		else show_inventory(keyID, client) end
	end,
}

mod:key {
	id = "equipment", name = "Inventory and Crafting", default_key = "I",
	callback = show_inventory,
}
