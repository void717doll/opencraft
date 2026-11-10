local states = {}
for shape = 1, 9 do
	states[shape] = {
		tiles = shape >= 6 and "refi_rail_curved.png" or "refi_rail.png",
		bounding_box = {0, 0, 0, 1, 1, shape >= 2 and shape <= 5 and 1 or 1 / 16},
	}
end

mod:block {
	id = "rail",
	name = "Rail",
	tiles = "refi_rail.png",
	inventory_image = "refi_rail.png",
	draw_type = "rail",
	hardness = 0.7,
	is_opaque = false,
	is_collidable = false,
	bounding_box = {0, 0, 0, 1, 1, 1 / 16},
	groups = {attached_node = 3, om_rail = 1, om_material_metal = 1},
	states = states,
}

mod:item {
	id = "minecart",
	name = "Minecart",
	tiles = "refi_minecart.png",
	max_stack_size = 1,
	groups = {om_minecart = 1},
}

mod:crafting_recipe {
	result = {id = "default:rail", amount = 16},
	pattern = {"I I", "ISI", "I I"},
	keys = {I = "default:iron_ingot", S = "default:stick"},
}
mod:crafting_recipe {
	result = {id = "default:minecart", amount = 1},
	pattern = {"I I", "III"},
	keys = {I = "default:iron_ingot"},
}

local function powered_rail(id, name, texture, kind)
	local shapes = {}
	for state = 1, 15 do
		local shape = state % 8
		shapes[state] = {
			tiles = texture .. (state >= 8 and "_on.png" or ".png"),
			bounding_box = {0, 0, 0, 1, 1, shape >= 2 and shape <= 5 and 1 or 1 / 16},
		}
	end
	mod:block {
		id = id, name = name,
		tiles = texture .. ".png", inventory_image = texture .. ".png",
		draw_type = "rail", hardness = 0.7,
		is_opaque = false, is_collidable = false,
		bounding_box = {0, 0, 0, 1, 1, 1 / 16},
		groups = {attached_node = 3, om_rail = 1, om_rail_type = kind, om_material_metal = 1},
		states = shapes,
	}
end
powered_rail("powered_rail", "Powered Rail", "refi_powered_rail", 2)
powered_rail("detector_rail", "Detector Rail", "refi_detector_rail", 3)

mod:block {
	id = "redstone_torch", name = "Redstone Torch",
	tiles = "refi_redstone_torch.png", inventory_image = "refi_redstone_torch_inv.png",
	draw_type = "torch", hardness = 0,
	is_opaque = false, is_collidable = false, is_light_source = true,
	bounding_box = {0.4375, 0.4375, 0, 0.125, 0.125, 0.625},
	is_rotatable = true,
	groups = {attached_node = 5, om_torch = 1, om_power_source = 1, om_light_level = 7, om_material_wood = 1},
	states = {{tiles = "refi_redstone_torch_off.png", is_light_source = false}},
}
mod:block {
	id = "lever", name = "Lever",
	tiles = {"oak_planks.png", "cobblestone.png"}, inventory_image = "refi_lever.png",
	draw_type = "lever", hardness = 0.5,
	is_opaque = false, is_collidable = false,
	bounding_box = {0.25, 0.25, 0, 0.5, 0.625, 0.5},
	groups = {attached_node = 3, om_power_source = 2, om_material_stone = 1},
	states = {{}},
	on_block_activated = function(pos, block, player, world, client, server)
		local state = world:get_block_state(pos.x, pos.y, pos.z)
		world:set_block_state(pos.x, pos.y, pos.z, state:id() == 0 and 1 or 0)
	end,
}
mod:block {
	id = "stone_pressure_plate", name = "Stone Pressure Plate",
	tiles = "refi_stone_plate.png", inventory_image = "refi_stone_plate_inv.png",
	draw_type = "boundingbox", hardness = 0.5,
	is_opaque = false, is_collidable = false,
	bounding_box = {0.0625, 0.0625, 0, 0.875, 0.875, 0.0625},
	groups = {attached_node = 3, om_power_source = 3, om_material_stone = 1},
	states = {{tiles = "refi_stone_plate_on.png", bounding_box = {0.0625, 0.0625, 0, 0.875, 0.875, 0.03125}}},
}
mod:crafting_recipe {
	result = {id = "default:powered_rail", amount = 6},
	pattern = {"G G", "GSG", "GRG"},
	keys = {G = "default:gold_ingot", S = "default:stick", R = "default:redstone_dust"},
}
mod:crafting_recipe {
	result = {id = "default:detector_rail", amount = 6},
	pattern = {"I I", "IPI", "IRI"},
	keys = {I = "default:iron_ingot", P = "default:stone_pressure_plate", R = "default:redstone_dust"},
}
mod:crafting_recipe {
	result = {id = "default:redstone_torch", amount = 1},
	pattern = {"R", "S"}, keys = {R = "default:redstone_dust", S = "default:stick"},
}
mod:crafting_recipe {
	result = {id = "default:lever", amount = 1},
	pattern = {"S", "C"}, keys = {S = "default:stick", C = "default:cobblestone"},
}
mod:crafting_recipe {
	result = {id = "default:stone_pressure_plate", amount = 1},
	pattern = {"SS"}, keys = {S = "default:stone"},
}

for _, cart in ipairs({
	{"chest_minecart", "Minecart with Chest", "chest", 2},
	{"furnace_minecart", "Minecart with Furnace", "furnace", 3},
}) do
	mod:item {
		id = cart[1], name = cart[2], tiles = "refi_" .. cart[1] .. ".png", max_stack_size = 1,
		groups = {om_minecart = cart[4]},
	}
	mod:crafting_recipe {
		result = {id = "default:" .. cart[1], amount = 1}, pattern = {"B", "C"},
		keys = {B = "default:" .. cart[3], C = "default:minecart"},
	}
end
