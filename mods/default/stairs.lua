local stairs = {
	{"oak_stairs", "Oak Wood Stairs", "oak_planks", "default:oak_planks", "wood", 0},
	{"cobblestone_stairs", "Cobblestone Stairs", "cobblestone", "default:cobblestone", "stone", Capability.Pickaxe},
}

for _, stair in ipairs(stairs) do
	local groups = {om_stairs = 1}
	groups["om_material_" .. stair[5]] = 1
	if stair[5] == "wood" then groups.om_fuel = 300 end
	mod:block {
		id = stair[1], name = stair[2], tiles = stair[3] .. ".png",
		hardness = 2, harvest_requirements = stair[6], groups = groups,
		draw_type = "boundingbox", is_opaque = false, is_rotatable = true,
		bounding_box = {0, 0, 0, 1, 1, 1},
		-- beta stairs drop their parent block rather than the stair item.
		item_drop = {id = "default:" .. stair[3], amount = 1},
	}
	for _, pattern in ipairs({{"S  ", "SS ", "SSS"}, {"  S", " SS", "SSS"}}) do
		mod:crafting_recipe {
			result = {id = "default:" .. stair[1], amount = 4},
			pattern = pattern, keys = {S = stair[4]},
		}
	end
end
