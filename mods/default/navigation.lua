local frames = {}
for frame = 0, 31 do frames[#frames + 1] = string.format("refi_compass_%02d.png", frame) end
mod:item { id = "compass", name = "Compass", tiles = frames, groups = {om_compass = 1} }
mod:crafting_recipe {
	result = {id = "default:compass", amount = 1}, pattern = {" I ", "IRI", " I "},
	keys = {I = "default:iron_ingot", R = "default:redstone_dust"},
}
mod:item { id = "map", name = "Map", tiles = "refi_map.png", max_stack_size = 1, groups = {om_map = 1, om_creative_hidden = 1} }
mod:item {id = "empty_map", name = "Empty Map", tiles = "refi_empty_map.png", groups = {om_map = 1}}
mod:crafting_recipe {
	result = {id = "default:empty_map", amount = 1}, pattern = {"PPP", "PCP", "PPP"},
	keys = {P = "default:paper", C = "default:compass"},
}
