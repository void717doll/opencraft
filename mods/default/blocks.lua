--
-- =====================================================================================
--
--  OpenMiner
--
--  Copyright (C) 2018-2020 Unarelith, Quentin Bazin <openminer@unarelith.net>
--  Copyright (C) 2019-2020 the OpenMiner contributors (see CONTRIBUTORS.md)
--
--  This file is part of OpenMiner.
--
--  OpenMiner is free software; you can redistribute it and/or
--  modify it under the terms of the GNU Lesser General Public
--  License as published by the Free Software Foundation; either
--  version 2.1 of the License, or (at your option) any later version.
--
--  OpenMiner is distributed in the hope that it will be useful,
--  but WITHOUT ANY WARRANTY; without even the implied warranty of
--  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
--  Lesser General Public License for more details.
--
--  You should have received a copy of the GNU Lesser General Public License
--  along with OpenMiner; if not, write to the Free Software Foundation, Inc.,
--  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
--
-- =====================================================================================
--

mod:block {
	id = "dirt",
	hardness = 0.5,
	name = "Dirt",
	tiles = "dirt.png",
	groups = {
		om_material_dirt = 1,
	},
}

mod:block {
	id = "cobblestone",
	name = "Cobblestone",
	tiles = "cobblestone.png",

	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "grass",
	hardness = 0.6,
	name = "Grass",
	tiles = {"grass_block_top.png", "dirt.png", "grass_block_side.png"},
	color_multiplier = {129, 191, 91, 255},
	groups = {
		om_material_dirt = 1,
	},

	item_drop = {
		id = mod:id()..":dirt",
		amount = 1
	}
}

mod:block {
	id = "oak_leaves",
	name = "Oak Leaves",
	tiles = "oak_leaves.png",
	color_multiplier = {106, 173, 51, 255},
	hardness = 0.2,
	draw_type = "leaves", custom_param_bits = 1,
	-- is_opaque = false, -- FIXME
}

mod:block {
	id = "oak_wood",
	hardness = 2,
	name = "Oak Wood",
	tiles = {"oak_log_top.png", "oak_log.png"},
	groups = {
		om_material_wood = 1,
		om_fuel = 300,
	}
}

mod:block {
	id = "stone",
	name = "Stone",
	tiles = "stone.png",
	hardness = 1.5,
	harvest_requirements = Capability.Pickaxe,
	item_drop = {
		id = mod:id()..":cobblestone",
		amount = 1
	},
}

mod:block {
	id = "sand",
	hardness = 0.5,
	name = "Sand",
	tiles = "sand.png",
	groups = {
		falling_node = 1,
		om_material_sand = 1
	}
}

mod:block {
	id = "water",
	name = "Water",

	tiles = "water.png",
	color_multiplier = {51, 115, 255, 217},

	draw_type = "liquid",
	is_opaque = false,

	fog_depth = 20.0,
	fog_color = {0, 128, 255},
}

mod:block {
	id = "glass",
	hardness = 0.3,
	name = "Glass",
	tiles = "glass.png",
	draw_type = "glass",
	is_opaque = false,
}

mod:block {
	id = "coal_ore",
	name = "Coal Ore",
	tiles = "coal_ore.png",
	hardness = 3,
	harvest_requirements = Capability.Pickaxe,
	item_drop = {
		id = mod:id()..":coal",
		amount = 1
	},
}

mod:block {
	id = "oak_planks",
	name = "Oak Wood Planks",
	tiles = "oak_planks.png",

	hardness = 2,

	groups = {
		om_material_wood = 1,
		om_fuel = 300,
		default_planks = 1
	}
}

mod:block {
	id = "glowstone",
	hardness = 0.3,
	name = "Glowstone",
	tiles = "glowstone.png",
	is_light_source = true,
	groups = {
		om_material_stone = 1
	},
}

mod:block {
	id = "iron_ore",
	name = "Iron Ore",
	tiles = "iron_ore.png",
	hardness = 3,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "dandelion",
	groups = {attached_node = 3, om_soil_plant = 1},
	name = "Dandelion",
	tiles = "dandelion.png",
	hardness = 0,
	draw_type = "xshape",
	bounding_box = {0.25, 0.25, 0.0, 0.5, 0.5, 0.5},
}

mod:block {
	id = "tallgrass",
	groups = {attached_node = 3, om_soil_plant = 1, om_replaceable = 1},
	name = "Grass",
	tiles = "grass.png",
	color_multiplier = {129, 191, 91, 255},
	hardness = 0,
	draw_type = "xshape",
}

mod:reserve_block_id() -- stonebrick

mod:block {
	id = "bricks",
	name = "Bricks",
	tiles = "bricks.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "clay",
	hardness = 0.6,
	name = "Clay",
	tiles = "clay.png",
	item_drop = {
		id = mod:id()..":clay_ball",
		amount = 4
	}
}

mod:block {
	id = "oak_slab",
	name = "Oak Wood Slab",
	tiles = "oak_planks.png",
	-- beta's wooden slab uses the same rock material and tools as stone slabs.
	groups = {om_material_stone = 1, om_slab = 1},
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
	draw_type = "boundingbox",
	is_opaque = false,
	bounding_box = {0, 0, 0, 1, 1, 0.5},
	states = {{
		bounding_box = {0, 0, 0, 1, 1, 1}, is_opaque = true,
		item_drop = {id = "default:oak_slab", amount = 2},
	}},
}

mod:block {
	id = "portal",
	name = "Portal",
	tiles = "portal.png",
	draw_type = "glass",
	is_opaque = false,
	is_collidable = false,
	is_light_source = true,
	groups = {om_light_level = 7},
	bounding_box = {0, 0.375, 0, 1, 0.25, 1},
	states = {{bounding_box = {0.375, 0, 0, 0.25, 1, 1}}},
	item_drop = {id = "default:portal", amount = 0},
	on_tick = function(pos, block, chunk, world)
		world:validate_portal(pos.x, pos.y, pos.z)
	end,
	tick_randomly = true,
	tick_probability = 0.05,
	on_block_activated = function(pos, block, player, world, client, server)
		server:use_portal(client.id, pos.x, pos.y, pos.z)
	end,
}

mod:block {
	id = "netherrack",
	hardness = 0.4,
	name = "Netherrack",
	tiles = "netherrack.png",
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "soul_sand",
	hardness = 0.5,
	name = "Soul Sand",
	tiles = "soul_sand.png",
	groups = {
		om_material_sand = 1
	}
}

mod:block {
	id = "lava",
	name = "Lava",
	tiles = "lava.png",

	draw_type = "liquid",
	is_light_source = true,
	is_opaque = false,

	fog_depth = 10.0,
	fog_color = {255, 128, 0},
}

mod:block {
	id = "cactus",
	name = "Cactus",
	tiles = {"cactus_top.png", "cactus_bottom.png", "cactus_side.png"},
	hardness = 0.4,
	is_opaque = false,

	draw_type = "cactus",
	bounding_box = {1/16, 1/16, 0, 14/16, 14/16, 15/16};
	groups = {attached_node = 3},
}

mod:block {
	id = "deadbush",
	groups = {attached_node = 3},
	name = "Dead Bush",
	tiles = "deadbush.png",
	draw_type = "xshape",
	hardness = 0,
}

mod:reserve_block_id() -- debug

mod:block {
	id = "obsidian",
	name = "Obsidian",
	tiles = "obsidian.png",

	hardness = 10,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "reeds",
	name = "Sugar Canes",
	tiles = "reeds.png",
	inventory_image = "reeds_item.png",

	draw_type = "xshape",
	is_collidable = false,
	hardness = 0,
	groups = {attached_node = 3},
}

mod:reserve_block_id() -- redstone_lamp

mod:block {
	id = "farmland",
	name = "Farmland",
	item_drop = {id = "default:dirt", amount = 1},
	tiles = {"farmland_dry.png", "dirt.png", "dirt.png"},
	is_opaque = false,
	groups = {
		om_material_dirt = 1,
		om_farmland = 1
	},

	states = {
		{ tiles = {"farmland_wet.png", "dirt.png", "dirt.png"} },
		{ tiles = {"farmland_wet.png", "dirt.png", "dirt.png"} },
		{ tiles = {"farmland_wet.png", "dirt.png", "dirt.png"} },
		{ tiles = {"farmland_wet.png", "dirt.png", "dirt.png"} },
		{ tiles = {"farmland_wet.png", "dirt.png", "dirt.png"} },
		{ tiles = {"farmland_wet.png", "dirt.png", "dirt.png"} },
		{ tiles = {"farmland_wet.png", "dirt.png", "dirt.png"} },
	},

	draw_type = "boundingbox",
	bounding_box = {0, 0, 0, 1, 1, 15 / 16},
	tick_randomly = true,
	tick_probability = 0.01,
	on_tick = function(pos, block, chunk, world)
		farming.soil_tick(pos, block, chunk, world)
	end,
}

mod:reserve_block_id() -- grass_path

mod:reserve_block_id() -- nether_wart

mod:block {
	id = "seeds",
	name = "Seeds",
	tiles = "wheat_stage_0.png",
	draw_type = "xshape",
	inventory_image = "seeds_wheat.png",
	hardness = 0,
	groups = {om_crop = 7, attached_node = 3},
	item_drop = {id = "default:seeds", amount = 1},

	placement_constraints = {
		[{0, 0, -1}] = {
			block = "default:farmland",
			is_whitelist = true
		}
	},

	bounding_box = {0, 0, 0, 1, 1, 1.0 / 16.0},
	draw_offset = {0, 0, -1.0 / 16.0},

	states = {
		{ tiles = "wheat_stage_1.png", bounding_box = {0, 0, 0, 1, 1, 3.0 / 16.0}, },  -- 1
		{ tiles = "wheat_stage_2.png", bounding_box = {0, 0, 0, 1, 1, 5.0 / 16.0}, },  -- 2
		{ tiles = "wheat_stage_3.png", bounding_box = {0, 0, 0, 1, 1, 8.0 / 16.0}, },  -- 3
		{ tiles = "wheat_stage_4.png", bounding_box = {0, 0, 0, 1, 1, 10.0 / 16.0}, }, -- 4
		{ tiles = "wheat_stage_5.png", bounding_box = {0, 0, 0, 1, 1, 12.0 / 16.0}, }, -- 5
		{ tiles = "wheat_stage_6.png", bounding_box = {0, 0, 0, 1, 1, 14.0 / 16.0}, }, -- 6
		{ tiles = "wheat_stage_7.png", bounding_box = {0, 0, 0, 1, 1, 1},
			item_drop = {id = "default:wheat", amount = 1} },
	},

	tick_randomly = true,
	tick_probability = 0.01,

	on_tick = function(pos, block, chunk, world)
		farming.grow_tick(pos, block, chunk, world)
	end,
}

mod:block {
	id = "torch",
	groups = {attached_node = 5, om_torch = 1},
	name = "Torch",
	tiles = "torch_on.png",

	is_light_source = true,
	is_collidable = false,
	is_opaque = false,

	inventory_image = "../blocks/torch_on.png",

	draw_type = "torch",
	is_rotatable = true,

	bounding_box = {7 / 16, 7 / 16, 0, 2 / 16, 2 / 16, 10 / 16},

	hardness = 0
}

mod:reserve_block_id() -- stone_andesite

mod:reserve_block_id() -- stone_granite

mod:reserve_block_id() -- stone_diorite

mod:block {
	id = "ice",
	hardness = 0.5,
	groups = {slippery = 3},
	name = "Ice",
	tiles = "ice.png",
	draw_type = "glass",
	is_opaque = false
}

mod:block {
	id = "snow",
	hardness = 0.2,
	name = "Snow",
	tiles = "snow.png",
}

mod:block {
	id = "bookshelf",
	hardness = 1.5,
	name = "Bookshelf",
	tiles = {"oak_planks.png", "oak_planks.png", "bookshelf.png"},
}

mod:reserve_block_id() -- stonebrick_mossy

mod:reserve_block_id() -- stonebrick_cracked

mod:reserve_block_id() -- stonebrick_carved

mod:block {
	id = "cobblestone_mossy",
	name = "Mossy Cobblestone",
	tiles = "cobblestone_mossy.png",
	hardness = 2,
	harvest_requirements = Capability.Pickaxe,
}

mod:reserve_block_id() -- coal_block

mod:block {
	id = "diamond_block",
	name = "Diamond Block",
	tiles = "diamond_block.png",
	hardness = 5,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "gold_block",
	name = "Gold Block",
	tiles = "gold_block.png",
	hardness = 3,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "lapis_block",
	name = "Lapis Lazuli Block",
	tiles = "lapis_block.png",
	hardness = 3,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "iron_block",
	name = "Iron Block",
	tiles = "iron_block.png",
	hardness = 5,
	harvest_requirements = Capability.Pickaxe,
}

mod:reserve_block_id() -- redstone_block

mod:reserve_block_id() -- emerald_block

mod:reserve_block_id() -- emerald_ore

mod:block {
	id = "gold_ore",
	name = "Gold Ore",
	tiles = "gold_ore.png",
	hardness = 3,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "diamond_ore",
	name = "Diamond Ore",
	tiles = "diamond_ore.png",
	hardness = 3,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "redstone_ore",
	name = "Redstone Ore",
	tiles = "redstone_ore.png",
	hardness = 3,
	harvest_requirements = Capability.Pickaxe,
}

mod:block {
	id = "lapis_ore",
	name = "Lapis Lazuli Ore",
	tiles = "lapis_ore.png",
	hardness = 3,
	harvest_requirements = Capability.Pickaxe,
}

mod:reserve_block_id() -- red_sandstone_smooth

mod:reserve_block_id() -- sandstone_smooth

mod:reserve_block_id() -- red_sandstone_carved

mod:reserve_block_id() -- sandstone_carved

mod:reserve_block_id() -- red_sandstone

mod:block {
	id = "sandstone",
	name = "Sandstone",
	tiles = {"sandstone_top.png", "sandstone_bottom.png", "sandstone_normal.png"},
	hardness = 0.8,
	harvest_requirements = Capability.Pickaxe,
}

mod:reserve_block_id() -- red_sand

mod:reserve_block_id() -- slime

mod:reserve_block_id() -- nether_bricks

mod:reserve_block_id() -- red_nether_brick

mod:reserve_block_id() -- end_stone

mod:reserve_block_id() -- end_bricks

mod:reserve_block_id() -- hay_block

dofile("blocks/workbench.lua")
dofile("blocks/furnace.lua")
dofile("blocks/door.lua")

mod:block {
	id = "bedrock",
	name = "Bedrock",
	tiles = "bedrock.png",
	hardness = math.huge,
}

mod:block {
	id = "gravel",
	hardness = 0.6,
	name = "Gravel",
	tiles = "gravel.png",
	groups = {
		falling_node = 1,om_material_dirt = 1},
}

mod:block {
	id = "wool",
	name = "White Wool",
	tiles = "white_wool.png",
	hardness = 0.8,
	groups = {om_material_wool = 1},
}

for _, liquid in ipairs({{"water", "Water", {51, 115, 255, 217}, 20, {0, 128, 255}, 1},
	{"lava", "Lava", {255, 255, 255, 255}, 10, {255, 128, 0}, 3}}) do
	mod:block {
		id = liquid[1] .. "_flowing",
		name = "Flowing " .. liquid[2],
		tiles = liquid[1] .. ".png",
		custom_param_bits = 4,
		draw_type = "liquid",
		is_opaque = false,
		is_collidable = false,
		is_light_source = liquid[1] == "lava",
		color_multiplier = liquid[3],
		fog_depth = liquid[4],
		fog_color = liquid[5],
		groups = {liquid_range = 8, liquid_viscosity = liquid[6]},
	}
end

dofile("blocks/plants.lua")
