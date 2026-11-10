mod:block {
	id = "note_block", name = "Note Block", tiles = "refi_note_block.png",
	hardness = 0.8, groups = {om_note_block = 1, om_redstone_device = 1, om_material_wood = 1},
	on_block_activated = function(pos, block, player, world)
		world:play_note(pos.x, pos.y, pos.z, true)
	end,
}
mod:crafting_recipe {
	result = {id = "default:note_block", amount = 1}, pattern = {"WWW", "WRW", "WWW"},
	keys = {W = "group:default_planks", R = "default:redstone_dust"},
}

for _, piston in ipairs({{"piston", "Piston", 1, "refi_piston_top.png"},
	{"sticky_piston", "Sticky Piston", 2, "refi_piston_sticky_top.png"}}) do
	mod:block {
		id = piston[1], name = piston[2],
		tiles = {piston[4], "refi_piston_bottom.png", "refi_piston_side.png"},
		hardness = 0.5, is_rotatable = true,
		groups = {om_piston = piston[3], om_redstone_device = 1, om_material_stone = 1},
		states = {{tiles = {"refi_piston_extended.png", "refi_piston_bottom.png", "refi_piston_side.png"}, is_opaque = false, bounding_box = {0,0,0,1,1,0.75}}},
		on_block_destroyed = function(pos, world) world:remove_piston(pos.x, pos.y, pos.z) end,
	}
end
mod:block {
	id = "piston_head", name = "Piston Head", tiles = {"refi_piston_top.png", "refi_piston_extended.png", "refi_piston_side.png"},
	hardness = 0.5, is_rotatable = true, is_opaque = false,
	states = {{tiles = {"refi_piston_sticky_top.png", "refi_piston_extended.png", "refi_piston_side.png"}}},
	draw_type = "boundingbox", bounding_box = {0, 0, 0.75, 1, 1, 0.25},
	groups = {om_piston_head = 1, om_redstone_device = 1, om_creative_hidden = 1, ci_ignore = 1, om_material_wood = 1},
	item_drop = {id = "default:piston", amount = 0},
	on_block_destroyed = function(pos, world) world:break_piston_head(pos.x, pos.y, pos.z) end,
}
mod:crafting_recipe {
	result = {id = "default:piston", amount = 1}, pattern = {"WWW", "CIC", "CRC"},
	keys = {W = "group:default_planks", C = "default:cobblestone", I = "default:iron_ingot", R = "default:redstone_dust"},
}
mod:crafting_recipe {
	result = {id = "default:sticky_piston", amount = 1}, pattern = {"S", "P"},
	keys = {S = "default:slimeball", P = "default:piston"},
}
