local directions = {
	{0, 0, 1, 0}, {0, 0, -1, 4}, {0, 1, 0, 8},
	{0, -1, 0, 12}, {1, 0, 0, 20}, {-1, 0, 0, 16},
}
local stages = {
	"small_amethyst_bud", "medium_amethyst_bud", "large_amethyst_bud", "amethyst_cluster",
}
local stage_by_id = {}
for stage, id in ipairs(stages) do stage_by_id["default:" .. id] = stage end
local function grow(pos, block, chunk, world)
	local side = directions[math.random(#directions)]
	local x, y, z = pos.x + side[1], pos.y + side[2], pos.z + side[3]
	if world:get_light(x, y, z) < 0 then return end
	local state = world:get_block_state(x, y, z)
	if not state then return end
	local stage = stage_by_id[state:block():string_id()] or 0
	if state:block():id() ~= 0 and (stage == 0 or stage == #stages) then return end
	if stage > 0 then
		local rotation = state:block():param():get_param(BlockParamType.Rotation, world:get_data(x, y, z))
		if rotation ~= side[4] then return end
	end
	world:set_block_from_str(x, y, z, "default:" .. stages[stage + 1])
	local result = world:get_block_state(x, y, z):block()
	world:set_data(x, y, z, result:param():set_param(BlockParamType.Rotation, 0, side[4]))
end

mod:item {id = "amethyst_shard", name = "Amethyst Shard", tiles = "refi_amethyst_shard.png"}
mod:block {
	id = "amethyst_block", name = "Amethyst Block", tiles = "refi_amethyst_block.png",
	hardness = 1.5, harvest_requirements = Capability.Pickaxe,
	groups = {om_material_stone = 1},
}
mod:block {
	id = "budding_amethyst", name = "Budding Amethyst", tiles = "refi_budding_amethyst.png",
	hardness = 1.5, harvest_requirements = Capability.Pickaxe,
	groups = {om_material_stone = 1}, item_drop = {id = "default:budding_amethyst", amount = 0},
	tick_randomly = true, tick_probability = 0.001, on_tick = grow,
}
for stage, id in ipairs(stages) do
	local heights = {4 / 16, 6 / 16, 7 / 16, 12 / 16}
	local widths = {8 / 16, 9 / 16, 9 / 16, 10 / 16}
	local names = {"Small Amethyst Bud", "Medium Amethyst Bud", "Large Amethyst Bud", "Amethyst Cluster"}
	local width = widths[stage]
	mod:block {
		id = id, name = names[stage], tiles = "refi_" .. id .. ".png", inventory_image = "refi_" .. id .. ".png",
		hardness = 1.5, draw_type = "xshape", is_opaque = false, is_collidable = false, is_rotatable = true,
		is_light_source = true, harvest_requirements = Capability.Pickaxe,
		bounding_box = {(1 - width) / 2, (1 - width) / 2, 0, width, width, heights[stage]},
		groups = {attached_node = 1, om_amethyst_cluster = stage, om_material_stone = 1, om_light_level = ({1, 2, 4, 5})[stage]},
		item_drop = {id = "default:amethyst_shard", amount = stage == 4 and 4 or 0},
	}
end
mod:crafting_recipe {
	result = {id = "default:amethyst_block", amount = 1}, pattern = {"AA", "AA"},
	keys = {A = "default:amethyst_shard"},
}
