mod:item {id = "climbing_rope", name = "Climbing Rope", tiles = "refi_rope.png", groups = {om_rope = 1}}
mod:block {
	id = "rope", name = "Climbing Rope", tiles = "refi_rope_strand.png",
	hardness = 0.2, draw_type = "boundingbox", is_opaque = false, is_collidable = false,
	bounding_box = {0.4375, 0.4375, 0, 0.125, 0.125, 1},
	groups = {climbable = 1, attached_node = 4, om_hanging_chain = 1, om_rope = 1, om_creative_hidden = 1, ci_ignore = 1},
	item_drop = {id = "default:climbing_rope", amount = 1},
}
mod:crafting_recipe {
	result = {id = "default:climbing_rope", amount = 8}, pattern = {"S", "S", "S"},
	keys = {S = "default:string"},
}
