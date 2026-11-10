mod:block {
	id = "sign", name = "Sign", max_stack_size = 1,
	tiles = "refi_sign_board.png", inventory_image = "refi_sign.png",
	draw_type = "boundingbox", bounding_box = {0, 7/16, 0.5, 1, 2/16, 0.5},
	is_opaque = false, is_collidable = false, is_rotatable = true, hardness = 1,
	groups = {om_material_wood = 1, om_sign = 1, attached_node = 3},
	states = {{bounding_box = {0, 0, 0.25, 1, 2/16, 0.5}}},
	on_block_placed = function(pos, world)
		local data = world:add_block_data(pos.x, pos.y, pos.z, 0, 0)
		for line = 0, 3 do data.meta:set_string("sign_line_" .. line, "") end
	end,
	on_block_activated = function() end,
}

local paintings = {}
for i = 1, 6 do paintings[#paintings + 1] = {tiles = "refi_painting_" .. i .. ".png"} end
mod:block {
	id = "painting", name = "Painting", tiles = "refi_painting_0.png",
	inventory_image = "refi_painting.png", draw_type = "boundingbox",
	bounding_box = {0, 0, 0, 1, 1, 1/16},
	is_opaque = false, is_collidable = false, is_rotatable = true, hardness = 0,
	groups = {om_material_wood = 1, om_painting = 1, attached_node = 1},
	states = paintings, item_drop = {id = "default:painting", amount = 1},
	on_block_activated = function(pos, block, player, world)
		world:set_block_state(pos.x, pos.y, pos.z, (world:get_block_state(pos.x, pos.y, pos.z):id() + 1) % 7)
	end,
}

mod:crafting_recipe {
	result = {id = "default:sign", amount = 1}, pattern = {"PPP", "PPP", " S "},
	keys = {P = "group:default_planks", S = "default:stick"},
}
mod:crafting_recipe {
	result = {id = "default:painting", amount = 1}, pattern = {"SSS", "SWS", "SSS"},
	keys = {S = "default:stick", W = "default:wool"},
}
