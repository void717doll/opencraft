mod:block {
	id = "tin_ore", name = "Tin Ore", tiles = "stone.png^tin_ore.png", hardness = 3,
	harvest_requirements = Capability.Pickaxe,
	groups = {om_material_stone = 1, om_min_harvest_level = 1},
}
mod:item {id = "tin_ingot", name = "Tin Ingot", tiles = "tin_ingot.png"}
mod:item {id = "bronze_ingot", name = "Bronze Ingot", tiles = "bronze_ingot.png"}
mod:smelting_recipe {
	input = {id = "default:tin_ore", amount = 1},
	output = {id = "default:tin_ingot", amount = 1},
}
mod:crafting_recipe {
	result = {id = "default:bronze_ingot", amount = 4}, pattern = {"CC", "CT"},
	keys = {C = "default:copper_ingot", T = "default:tin_ingot"},
}

for _, tool in ipairs({
	{"pickaxe", Capability.Pickaxe, {"BBB", " S ", " S "}},
	{"axe", Capability.Axe, {"BB", "BS", " S"}},
	{"shovel", Capability.Shovel, {"B", "S", "S"}},
	{"hoe", 0, {"BB", " S", " S"}},
	{"sword", 0, {"B", "B", "S"}},
}) do
	register_tool(tool[1], "bronze", 5.5, tool[2], 1, 220, tool[1] == "sword" and 1 or 0)
	mod:crafting_recipe {
		result = {id = "default:bronze_" .. tool[1], amount = 1}, pattern = tool[3],
		keys = {B = "default:bronze_ingot", S = "default:stick"},
	}
end

local points = {3, 8, 6, 3}
local durability = {99, 144, 135, 117}
local patterns = {{"BBB", "B B"}, {"B B", "BBB", "BBB"}, {"BBB", "B B", "B B"}, {"B B", "B B"}}
for slot, piece in ipairs({"helmet", "chestplate", "leggings", "boots"}) do
	local id = piece .. "_bronze"
	local groups = {om_armor_slot = slot, om_armor_points = points[slot], om_max_damage = durability[slot], om_projectile_reduction = 10}
	groups["om_armor_" .. piece] = 1
	mod:item {
		id = id, name = "Bronze " .. piece:gsub("^%l", string.upper), tiles = id .. ".png",
		max_stack_size = 1, groups = groups,
	}
	mod:crafting_recipe {
		result = {id = "default:" .. id, amount = 1}, pattern = patterns[slot],
		keys = {B = "default:bronze_ingot"},
	}
end
