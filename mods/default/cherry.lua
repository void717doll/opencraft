mod:block {
	id = "cherry_wood", name = "Cherry Log", hardness = 2,
	tiles = {"refi_cherry_log_top.png", "refi_cherry_log.png"},
	groups = {om_material_wood = 1, om_fuel = 300},
}
mod:block {
	id = "cherry_planks", name = "Cherry Planks", hardness = 2,
	tiles = "refi_cherry_planks.png",
	groups = {om_material_wood = 1, om_fuel = 300, default_planks = 1},
}
mod:block {
	id = "cherry_leaves", name = "Cherry Leaves", hardness = 0.2,
	tiles = "refi_cherry_leaves.png", draw_type = "leaves", custom_param_bits = 1, is_opaque = false,
	item_drop = {id = "default:cherry_sapling", amount = 1},
}
mod:block {
	id = "cherry_sapling", name = "Cherry Sapling", hardness = 0,
	tiles = "refi_cherry_sapling.png", inventory_image = "refi_cherry_sapling.png",
	draw_type = "xshape", is_opaque = false, is_collidable = false,
	groups = {attached_node = 3, om_sapling = 1},
	placement_constraints = {
		[{0, 0, -1}] = {block = "group:om_material_dirt", is_whitelist = true},
	},
	tick_randomly = true, tick_probability = 0.001,
	on_tick = function(pos, block, chunk, world)
		world:grow_tree(pos.x, pos.y, pos.z, "default:cherry")
	end,
}
mod:tree {
	id = "cherry", log_block = "default:cherry_wood", leaves_block = "default:cherry_leaves",
	trunk_height = {min = 4, max = 6},
}
mod:crafting_recipe {
	result = {id = "default:cherry_planks", amount = 4}, pattern = {"L"},
	keys = {L = "default:cherry_wood"},
}
mod:smelting_recipe {
	input = {id = "default:cherry_wood", amount = 1},
	output = {id = "default:charcoal", amount = 1},
}
mod:crafting_recipe {
	result = {id = "default:mulch", amount = 1}, pattern = {"LL", "LL"},
	keys = {L = "default:cherry_leaves"},
}
