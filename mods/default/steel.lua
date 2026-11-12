mod:item {id = "molten_iron", name = "Molten Iron", tiles = "refi_molten_iron.png"}
mod:item {id = "steel_ingot", name = "Steel Ingot", tiles = "refi_carbon_steel.png"}

local blast_ores = {
	["default:iron_ore"] = true, ["default:gold_ore"] = true,
	["default:tin_ore"] = true, ["default:copper_ore"] = true, ["default:coal_ore"] = true,
	["default:redstone_ore"] = true, ["default:lapis_ore"] = true,
}

register_furnace {
	id = "blast_furnace", name = "Blast Furnace", texture_prefix = "refi_blast_furnace",
	cook_ticks = 100, fuel_factor = 2, active_frames = 3,
	get_result = function(input, recipe)
		if input:amount() == 0 then return nil end
		local id = input:item():string_id()
		if id == "default:iron_ingot" then return ItemStack.new("default:molten_iron", 1) end
		if blast_ores[id] and recipe then return recipe:result() end
	end,
}

mod:crafting_recipe {
	result = {id = "default:blast_furnace", amount = 1}, pattern = {"III", "IFI", "SSS"},
	keys = {I = "default:iron_ingot", F = "default:furnace", S = "default:stone"},
}
mod:crafting_recipe {
	result = {id = "default:steel_ingot", amount = 1}, pattern = {"CMO"},
	keys = {C = "default:coal", M = "default:molten_iron", O = "default:obsidian"},
}

for _, tool in ipairs({
	{"pickaxe", Capability.Pickaxe, {"III", " S ", " S "}},
	{"axe", Capability.Axe, {"II", "IS", " S"}},
	{"shovel", Capability.Shovel, {"I", "S", "S"}},
	{"hoe", 0, {"II", " S", " S"}},
	{"sword", 0, {"I", "I", "S"}},
}) do
	mod:crafting_recipe {
		result = {id = "default:steel_" .. tool[1], amount = 1}, pattern = tool[3],
		keys = {I = "default:steel_ingot", S = "default:stick"},
	}
end
