for _, wood in ipairs({
	{"jungle", "Jungle", 7, 11}, {"acacia", "Acacia", 4, 6}, {"dark_oak", "Dark Oak", 5, 7},
}) do
	local id, name = wood[1], wood[2]
	mod:block {
		id = id .. "_wood", name = name .. " Log", hardness = 2,
		tiles = {"refi_" .. id .. "_log_top.png", "refi_" .. id .. "_log.png"},
		groups = {om_material_wood = 1, om_fuel = 300},
	}
	mod:block {
		id = id .. "_planks", name = name .. " Planks", hardness = 2,
		tiles = "refi_" .. id .. "_planks.png",
		groups = {om_material_wood = 1, om_fuel = 300, default_planks = 1},
	}
	mod:block {
		id = id .. "_leaves", name = name .. " Leaves", hardness = 0.2,
		tiles = "refi_" .. id .. "_leaves.png", draw_type = "leaves", custom_param_bits = 1, is_opaque = false,
		item_drop = {id = "default:" .. id .. "_sapling", amount = 1},
	}
	mod:block {
		id = id .. "_sapling", name = name .. " Sapling", hardness = 0,
		tiles = "refi_" .. id .. "_sapling.png", inventory_image = "refi_" .. id .. "_sapling.png",
		draw_type = "xshape", is_opaque = false, is_collidable = false,
		groups = {attached_node = 3, om_sapling = 1},
		placement_constraints = { [{0, 0, -1}] = {block = "group:om_material_dirt", is_whitelist = true} },
		tick_randomly = true, tick_probability = 0.001,
		on_tick = function(pos, block, chunk, world) world:grow_tree(pos.x, pos.y, pos.z, "default:" .. id) end,
	}
	mod:tree {
		id = id, log_block = "default:" .. id .. "_wood", leaves_block = "default:" .. id .. "_leaves",
		trunk_height = {min = wood[3], max = wood[4]},
	}
	mod:crafting_recipe {
		result = {id = "default:" .. id .. "_planks", amount = 4}, pattern = {"L"}, keys = {L = "default:" .. id .. "_wood"},
	}
	mod:smelting_recipe {input = {id = "default:" .. id .. "_wood", amount = 1}, output = {id = "default:charcoal", amount = 1}}
end
