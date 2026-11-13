local materials = {
	{"birch", "Birch", "birch_planks", "refi_birch_planks.png", "wood"},
	{"spruce", "Spruce", "spruce_planks", "refi_spruce_planks.png", "wood"},
	{"cherry", "Cherry", "cherry_planks", "refi_cherry_planks.png", "wood"},
	{"jungle", "Jungle", "jungle_planks", "refi_jungle_planks.png", "wood"},
	{"acacia", "Acacia", "acacia_planks", "refi_acacia_planks.png", "wood"},
	{"dark_oak", "Dark Oak", "dark_oak_planks", "refi_dark_oak_planks.png", "wood"},
	{"amethyst", "Amethyst", "amethyst_block", "refi_amethyst_block.png", "stone"},
}
for _, material in ipairs(materials) do
	local id, name, parent, texture, kind = material[1], material[2], material[3], material[4], material[5]
	local slab, stairs = id .. "_slab", id .. "_stairs"
	local hardness = kind == "wood" and 2 or 1.5
	local harvest = kind == "wood" and 0 or Capability.Pickaxe
	local slab_groups = {om_slab = 1}
	local stair_groups = {om_stairs = 1}
	slab_groups["om_material_" .. kind] = 1
	stair_groups["om_material_" .. kind] = 1
	if kind == "wood" then slab_groups.om_fuel = 150; stair_groups.om_fuel = 300 end
	mod:block {
		id = slab, name = name .. " Slab", tiles = texture,
		hardness = hardness, harvest_requirements = harvest, groups = slab_groups,
		draw_type = "boundingbox", is_opaque = false,
		bounding_box = {0, 0, 0, 1, 1, 0.5},
		states = {{bounding_box = {0, 0, 0, 1, 1, 1}, is_opaque = true,
			item_drop = {id = "default:" .. slab, amount = 2}}},
	}
	mod:block {
		id = stairs, name = name .. " Stairs", tiles = texture,
		hardness = hardness, harvest_requirements = harvest, groups = stair_groups,
		draw_type = "boundingbox", is_opaque = false, is_rotatable = true,
		bounding_box = {0, 0, 0, 1, 1, 1},
		item_drop = {id = "default:" .. parent, amount = 1},
	}
	mod:crafting_recipe {
		result = {id = "default:" .. slab, amount = 3},
		pattern = {"SSS"}, keys = {S = "default:" .. parent},
	}
	for _, pattern in ipairs({{"S  ", "SS ", "SSS"}, {"  S", " SS", "SSS"}}) do
		mod:crafting_recipe {
			result = {id = "default:" .. stairs, amount = 4},
			pattern = pattern, keys = {S = "default:" .. parent},
		}
	end
end
