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

function register_tool(name, material, mining_speed, harvest_capability, harvest_level, max_damage, attack_bonus, brightness)
	local attack_damage = 1
	if name == "sword" then
		attack_damage = 4 + harvest_level * 2
	elseif name == "axe" then
		attack_damage = 3 + harvest_level
	elseif name == "pickaxe" then
		attack_damage = 2 + harvest_level
	elseif name == "shovel" then
		attack_damage = 1 + harvest_level
	end

	local tool_def = {
		id = material .. "_" .. name,
		name = material:gsub("^%l", string.upper) .. " " .. name:gsub("^%l", string.upper),
		tiles = material .. "_" .. name .. ".png",
		max_stack_size = 1,
		groups = {
			om_item_brightness = brightness,
			om_max_damage = max_damage,
			om_tool_type = name == "sword" and 1 or name == "hoe" and 3 or 2,
			om_tool_kind = name == "axe" and 1 or name == "pickaxe" and 2 or name == "shovel" and 3 or 0,
			om_harvest_level = harvest_level,
			om_copper_tool = material == "copper" and 1 or 0,
			om_bronze_tool = material == "bronze" and 1 or 0,
			om_attack_damage = attack_damage + (attack_bonus or 0),
		},
	}

	if name ~= "sword" then
		if mining_speed then
			tool_def.mining_speed = mining_speed
		end

		if harvest_capability then
			tool_def.harvest_capability = harvest_capability
		end
	end

	if name == "hoe" then
		tool_def.on_item_activated = function(pos, block, player, world, client, server)
			if block:string_id() == "default:grass" or block:string_id() == "default:dirt" then
				if world:get_block(pos.x, pos.y, pos.z + 1) == 0 then
					world:set_block_from_str(pos.x, pos.y, pos.z, "default:farmland")
				end
			end
		end
	elseif name == "shovel" then
		tool_def.effective_on = {
			"group:om_material_dirt",
			"group:om_material_sand"
		}
	elseif name == "axe" then
		tool_def.effective_on = {
			"group:om_material_wood"
		}
	elseif name == "pickaxe" then
		tool_def.effective_on = {
			"group:om_material_stone"
		}
	end

	mod:item(tool_def)
end

local tools = {axe = Capability.Axe, hoe = 0, pickaxe = Capability.Pickaxe, shovel = Capability.Shovel, sword = 0}
local materials = {
	{"wooden", 2, 0, 59},
	{"stone", 4, 1, 131},
	{"iron", 6, 2, 250},
	{"golden", 12, 0, 32},
	{"diamond", 8, 3, 1561},
	{"steel", 4, 3, 5000, 1, 65},
}

for _, material in ipairs(materials) do
	for k, v in pairs(tools) do
		register_tool(k, material[1], material[2], v, material[3], material[4], material[5], material[6])
	end
end

mod:item {
	id = "shears",
	name = "Shears",
	tiles = "shears.png",
	groups = {om_max_damage = 238, om_tool_type = 4},

	max_stack_size = 1,

}
