for _, biome in ipairs({
	{"jungle", "Jungle", "jungle", 0.07, "fern", 0.2},
	{"acacia_savanna", "Acacia Savanna", "acacia", 0.025, "tallgrass", 0.15},
	{"birch_forest", "Birch Forest", "birch", 0.035, "tallgrass", 0.12},
	{"dark_forest", "Dark Forest", "dark_oak", 0.065, "fern", 0.15},
	{"cherry_grove", "Cherry Grove", "cherry", 0.0035, "rose", 0.06},
}) do
	mod:biome {
		id = biome[1], name = biome[2],
		params = {temperature = 0.5, precipitation = 0.5},
		blocks = {
			top = "default:grass", ground = "default:dirt", deep = "default:stone",
			beach = "default:sand", liquid = "default:water", portal = "default:portal", portal_frame = "default:obsidian",
		},
		trees = {{type = "default:" .. biome[3], probability = biome[4]}},
		flora = {{block = "default:" .. biome[5], spawns_on = "default:grass", probability = biome[6]}},
	}
end
