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

function register_furnace(def)
local cook_ticks = def.cook_ticks or 200
local prefix = def.texture_prefix
local states = {}
for frame = 1, (def.active_frames or 1) do
	local suffix = frame == 1 and "" or "_" .. frame
	states[frame] = {
		is_light_source = true,
		tiles = {prefix .. "_top.png", prefix .. "_top.png", prefix .. "_front_on" .. suffix .. ".png", prefix .. "_side.png"},
	}
end

mod:block {
	id = def.id,
	name = def.name,
	tiles = {prefix .. "_top.png", prefix .. "_top.png", prefix .. "_front.png", prefix .. "_side.png"},
	is_rotatable = true,
	groups = {om_material_stone = 1, om_light_level = 13},

	hardness = 3.5,
	harvest_requirements = Capability.Pickaxe,

	states = states,

	on_block_placed = function(pos, world)
		world:add_block_data(pos.x, pos.y, pos.z, 3, 1)
	end,

	on_block_activated = function(pos, block, player, world, client, server)
		local gui = LuaGUI.new()

		gui:set_size(176, 166)
		gui:set_centered(true)

		gui:progress_bar {
			name = "bar_process",
			pos = {x = 80, y = 35},

			type = "item_process",

			block = {x = pos.x, y = pos.y, z = pos.z},

			meta = "item_progress",
			max_value = cook_ticks,

			texture = modpath .. "/textures/gui/furnace.png",
			clip = {x = 176, y = 14, width = 24, height = 17},
		}

		gui:progress_bar {
			name = "bar_burn",
			pos = {x = 57, y = 37},

			type = "burn_process",

			block = {x = pos.x, y = pos.y, z = pos.z},

			meta = "ticks_remaining",
			max_meta = "current_burn_time",

			texture = modpath .. "/textures/gui/furnace_fire_fg.png",
			clip = {x = 0, y = 0, width = 14, height = 14},
		}

		gui:inventory {
			name = "inv_input",
			pos = {x = 55, y = 16},

			inventory = {
				source = "block",
				block = {x = pos.x, y = pos.y, z = pos.z},
				offset = 0,
				count = 1,
			},

			size = {x = 1, y = 1},

			shift_destination = "inv_main,inv_hotbar",
		}

		gui:inventory {
			name = "inv_output",
			pos = {x = 115, y = 34},

			inventory = {
				source = "block",
				block = {x = pos.x, y = pos.y, z = pos.z},
				offset = 1,
				count = 1,
			},

			is_output = true,

			size = {x = 1, y = 1},

			shift_destination = "inv_main,inv_hotbar",
		}

		gui:inventory {
			name = "inv_fuel",
			pos = {x = 55, y = 52},

			inventory = {
				source = "block",
				block = {x = pos.x, y = pos.y, z = pos.z},
				offset = 2,
				count = 1,
			},

			size = {x = 1, y = 1},

			filter = "group:om_fuel",
			shift_destination = "inv_main,inv_hotbar",
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

			shift_destination = "inv_input,inv_fuel",
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

			shift_destination = "inv_input,inv_fuel",
		}

		gui:image {
			name = "img_background",
			pos = {x = 0, y = 0},

			texture = modpath .. "/textures/gui/inventory.png",
			size = {x = 176, y = 166},
			border = 5,
		}

		gui:image {
			name = "img_process_arrow",
			pos = {x = 80, y = 35},
			texture = modpath .. "/textures/gui/workbench.png",
			clip = {x = 468, y = 158, width = 73, height = 58},
			size = {x = 24, y = 17},
		}

		gui:image {
			name = "img_fire",
			pos = {x = 57, y = 37},
			texture = modpath .. "/textures/gui/furnace_fire_bg.png",
			clip = {x = 0, y = 0, width = 14, height = 14},
		}

		gui:show(client)
	end,

	on_tick = function(pos, block, chunk, world)
		local data = world:get_block_data(pos.x, pos.y, pos.z)
		if not data then return end

		local block_param = world:get_data(pos.x, pos.y, pos.z)
		local current_state = block:param():get_param(BlockParamType.State, block_param)

		local input_stack = data.inventory:get_stack(0, 0)
		local output_stack = data.inventory:get_stack(1, 0)
		local fuel_stack = data.inventory:get_stack(2, 0)

		local ticks_remaining = data.meta:get_int("ticks_remaining") or 0
		local current_burn_time = data.meta:get_int("current_burn_time") or 0
		local item_progress = data.meta:get_int("item_progress") or 0
		local input_id = input_stack:item():string_id()
		if data.meta:get_string("input_id") ~= input_id then item_progress = 0 end

		local recipe = opencraft.registry:get_recipe(data.inventory)
		if recipe and recipe:type() ~= "smelt" then
			recipe = nil
		end
		local result
		if def.get_result then result = def.get_result(input_stack, recipe)
		elseif recipe then result = recipe:result() end
		local has_room = result and result:amount() > 0 and input_stack:amount() > 0
			and (output_stack:amount() == 0 or output_stack:item():string_id() == result:item():string_id())
			and output_stack:amount() + result:amount() <= result:item():max_stack_size()

		if ticks_remaining == 0 and has_room and fuel_stack:amount() > 0
			and fuel_stack:item():has_group("group:om_fuel") then
			local fuel_id = fuel_stack:item():string_id()
			local burn_time = math.floor(fuel_stack:item():get_group_value("group:om_fuel") / (def.fuel_factor or 1))
			if fuel_id == "default:bucket_lava" then
				data.inventory:set_stack(2, 0, "default:bucket_empty", 1)
			else
				data.inventory:set_stack(2, 0, fuel_stack:amount() > 1 and fuel_id or "", fuel_stack:amount() - 1)
			end
			ticks_remaining = burn_time
			current_burn_time = burn_time
		end
		if ticks_remaining > 0 then
			ticks_remaining = ticks_remaining - 1
			if has_room then
				item_progress = item_progress + 1
			else
				item_progress = 0
			end
		else
			item_progress = 0
			current_burn_time = 0
		end
		local next_state = 0
		if ticks_remaining > 0 then
			next_state = def.active_frames and math.floor(world:get_ticks() / 320) % def.active_frames + 1 or 1
		end
		if current_state ~= next_state then world:set_block_state(pos.x, pos.y, pos.z, next_state) end

		if item_progress >= cook_ticks and has_room then
			item_progress = 0
			data.inventory:set_stack(0, 0, input_stack:amount() > 1 and input_id or "", input_stack:amount() - 1)
			data.inventory:set_stack(1, 0, result:item():string_id(), output_stack:amount() + result:amount())
		end

		data.meta:set_int("ticks_remaining", ticks_remaining)
		data.meta:set_int("current_burn_time", current_burn_time)
		data.meta:set_int("item_progress", item_progress)
		data.meta:set_string("input_id", input_id)
		world:mark_block_modified(pos.x, pos.y, pos.z)
	end,
}
end

register_furnace {id = "furnace", name = "Furnace", texture_prefix = "furnace"}
