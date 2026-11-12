local blocks, items, recipes = {}, {}, {}
Capability = {Pickaxe = 1, Axe = 4, Shovel = 2}
BlockParamType = {State = 1}
local gui_shown, gui_slots, bucket_returns = 0, {}, 0
LuaGUI = {new = function()
	return {set_size = function() end, set_centered = function() end, image = function() end,
		progress_bar = function() end,
		inventory = function(_, def) gui_slots[def.name] = def end,
		show = function() gui_shown = gui_shown + 1 end}
end}
mod = {
	path = function() return "mods/default" end,
	block = function(_, def) blocks["default:" .. def.id] = def end,
	item = function(_, def) items["default:" .. def.id] = def end,
	crafting_recipe = function(_, def) recipes[#recipes + 1] = def end,
	give_item_stack = function(_, player, stack)
		assert(stack:item():string_id() == "default:bucket_empty" and stack:amount() == 1)
		bucket_returns = bucket_returns + 1
	end,
}
local function item(id)
	local def = items[id] or {}
	return {
		string_id = function() return id end,
		max_stack_size = function() return def.max_stack_size or 99 end,
		has_group = function(_, name) return def.groups and (def.groups[name:sub(7)] or 0) > 0 end,
		get_group_value = function(_, name) return def.groups and def.groups[name:sub(7)] or 0 end,
	}
end
ItemStack = {new = function(id, amount)
	local stack = {id = id or "", count = amount or 0}
	function stack:amount() return self.count end
	function stack:item() return item(self.id) end
	return stack
end}
local smelts = {['default:iron_ore'] = 'default:iron_ingot', ['default:potato'] = 'default:baked_potato',
	['default:porkchop_raw'] = 'default:porkchop_cooked'}
opencraft = {registry = {get_recipe = function(_, inventory)
	local input = inventory:get_stack(0, 0)
	local result = smelts[input.id]
	if result and input.count > 0 then
		return {type = function() return "smelt" end, result = function() return ItemStack.new(result, 1) end}
	end
end}}
items['default:coal'] = {groups = {om_fuel = 1600}}
items['default:bucket_lava'] = {max_stack_size = 1, groups = {om_fuel = 20000}}
items['default:flint_and_steel'] = {groups = {om_igniter = 1}}
for _, id in ipairs({'porkchop_raw', 'porkchop_cooked', 'fish_cod_raw', 'fish_cod_cooked', 'potato', 'baked_potato'}) do
	items['default:' .. id] = {max_stack_size = 1}
end
dofile("mods/default/tools.lua")
dofile("mods/default/blocks/furnace.lua")
dofile("mods/default/steel.lua")
dofile("mods/default/campfires.lua")

local function inventory(size)
	local stacks = {}
	for slot = 0, size - 1 do stacks[slot] = ItemStack.new('', 0) end
	return {
		get_stack = function(_, x) return stacks[x] end,
		set_stack = function(_, x, y, id, count) stacks[x].id = id stacks[x].count = count end,
	}
end
local function metadata()
	local values = {}
	return {get_int = function(_, key) return values[key] end, set_int = function(_, key, value) values[key] = value end,
		get_string = function(_, key) return values[key] end, set_string = function(_, key, value) values[key] = value end}
end
local pos = {x = 0, y = 0, z = 1}
local function fixture(id, size)
	local data = {inventory = inventory(size), meta = metadata()}
	local current, ticks, modified, particles, water = 0, 0, 0, 0, false
	local state = {id = function() return current end}
	local block = {param = function() return {get_param = function(_, _, value) return value end} end}
	local world = {
		get_data = function() return current end,
		get_ticks = function() return ticks end,
		get_block_data = function() return data end,
		add_block_data = function() return data end,
		set_block_state = function(_, x, y, z, value) current = value end,
		mark_block_modified = function() modified = modified + 1 end,
		emit_particles = function() particles = particles + 1 end,
		get_block_state = function(_, x, y, z)
			if x == 0 and y == 0 and z == 1 then return state end
			return {block = function() return {string_id = function() return water and "default:water" or "air" end} end}
		end,
	}
	return {data = data, world = world, block = block,
		tick = function(count) for _ = 1, count do ticks = ticks + 1 blocks[id].on_tick(pos, block, nil, world) end end,
		state = function() return current end, modified = function() return modified end,
		particles = function() return particles end, water = function(value) water = value end}
end
local function slot(f, x, id, amount) f.data.inventory:set_stack(x, 0, id, amount or 1) end
local function is_slot(f, x, id, amount)
	local stack = f.data.inventory:get_stack(x, 0)
	assert(stack.id == id and stack.count == amount, stack.id .. ':' .. stack.count .. ' expected ' .. id .. ':' .. amount)
end

for _, name in ipairs({'axe', 'pickaxe', 'shovel', 'hoe', 'sword'}) do
	local steel, diamond = items['default:steel_' .. name], items['default:diamond_' .. name]
	assert(steel.max_stack_size == 1 and steel.groups.om_max_damage == 5000 and steel.groups.om_item_brightness == 65)
	assert(steel.groups.om_harvest_level == 3 and steel.groups.om_attack_damage == diamond.groups.om_attack_damage + 1)
	if name ~= 'sword' then assert(steel.mining_speed == items['default:stone_' .. name].mining_speed) end
end
assert(item('default:steel_ingot'):max_stack_size() == 99 and item('default:molten_iron'):max_stack_size() == 99)
local steel_recipe
for _, recipe in ipairs(recipes) do if recipe.result.id == 'default:steel_ingot' then steel_recipe = recipe end end
assert(steel_recipe.pattern[1] == 'CMO' and steel_recipe.keys.O == 'default:obsidian' and steel_recipe.keys.M == 'default:molten_iron')

local f = fixture('default:blast_furnace', 3)
slot(f, 0, 'default:iron_ingot', 2) slot(f, 2, 'default:coal')
f.tick(99) is_slot(f, 1, '', 0)
f.tick(1) is_slot(f, 1, 'default:molten_iron', 1) is_slot(f, 0, 'default:iron_ingot', 1)
assert(f.data.meta:get_int('current_burn_time') == 800 and f.data.meta:get_int('ticks_remaining') == 700)
f.tick(100) is_slot(f, 1, 'default:molten_iron', 2) is_slot(f, 0, '', 0)
assert(f.modified() == 200 and f.state() > 0)
local normal = fixture('default:furnace', 3)
slot(normal, 0, 'default:iron_ingot') slot(normal, 2, 'default:coal') normal.tick(220)
is_slot(normal, 0, 'default:iron_ingot', 1) is_slot(normal, 1, '', 0) is_slot(normal, 2, 'default:coal', 1)
slot(normal, 0, 'default:iron_ore') normal.tick(200)
is_slot(normal, 1, 'default:iron_ingot', 1)
assert(normal.data.meta:get_int('current_burn_time') == 1600 and normal.data.meta:get_int('ticks_remaining') == 1400)

f = fixture('default:blast_furnace', 3)
slot(f, 0, 'default:iron_ingot') slot(f, 1, 'default:molten_iron', 99) slot(f, 2, 'default:coal') f.tick(100)
is_slot(f, 0, 'default:iron_ingot', 1) is_slot(f, 1, 'default:molten_iron', 99) is_slot(f, 2, 'default:coal', 1)
slot(f, 1, 'default:iron_ingot') f.tick(100) is_slot(f, 2, 'default:coal', 1)
slot(f, 1, '', 0) slot(f, 0, 'default:potato') f.tick(100) is_slot(f, 2, 'default:coal', 1)
slot(f, 0, 'default:iron_ore') f.tick(50)
slot(f, 0, 'default:iron_ingot') f.tick(50) is_slot(f, 1, '', 0)
f.tick(50) is_slot(f, 1, 'default:molten_iron', 1)

normal = fixture('default:furnace', 3)
slot(normal, 0, 'default:potato') slot(normal, 1, 'default:baked_potato') slot(normal, 2, 'default:bucket_lava') normal.tick(200)
is_slot(normal, 0, 'default:potato', 1) is_slot(normal, 1, 'default:baked_potato', 1) is_slot(normal, 2, 'default:bucket_lava', 1)
slot(normal, 1, '', 0) normal.tick(200)
is_slot(normal, 1, 'default:baked_potato', 1) is_slot(normal, 2, 'default:bucket_empty', 1)
assert(normal.data.meta:get_int('current_burn_time') == 20000)

local camp = fixture('default:campfire', 4)
local held = ItemStack.new('', 0)
local player = {held_item_stack = function() return held end}
local client = {id = 1, creative = false}
local worn, consumed = 0, 0
local server = {
	consume_held_item = function(_, _, amount, id)
		assert(id == client.id and held.count >= amount)
		if not client.creative then held.count = held.count - amount consumed = consumed + amount end
		return true
	end,
	wear_held_item = function(_, _, amount) if not client.creative then worn = worn + amount end end,
}
local function activate(id, amount)
	held = ItemStack.new(id, amount or 1)
	blocks['default:campfire'].on_block_activated(pos, camp.block, player, camp.world, client, server)
end
activate('default:porkchop_raw') activate('default:fish_cod_raw') activate('default:potato') activate('default:porkchop_raw')
assert(consumed == 4)
activate('default:potato') assert(consumed == 4 and gui_shown == 1)
for slot_index = 0, 3 do assert(gui_slots['food_' .. slot_index].is_read_only) end
camp.tick(300)
activate('default:steel_shovel') assert(camp.state() == 1 and worn == 1)
camp.tick(600) is_slot(camp, 0, 'default:porkchop_raw', 1)
assert(camp.data.meta:get_int('cook_0') == 300)
activate('default:flint_and_steel') assert(camp.state() == 0 and worn == 2)
camp.tick(299) is_slot(camp, 0, 'default:porkchop_raw', 1)
camp.tick(1)
is_slot(camp, 0, 'default:porkchop_cooked', 1) is_slot(camp, 1, 'default:fish_cod_cooked', 1)
is_slot(camp, 2, 'default:baked_potato', 1) is_slot(camp, 3, 'default:porkchop_cooked', 1)
camp.tick(100) assert(camp.data.meta:get_int('cook_0') == 600)
activate('default:bucket_water') assert(camp.state() == 1 and consumed == 5 and bucket_returns == 1)
client.creative = true
activate('default:flint_and_steel') assert(camp.state() == 0 and worn == 2)
activate('default:bucket_water') assert(camp.state() == 1 and consumed == 5 and bucket_returns == 1)
slot(camp, 0, '', 0) activate('default:potato') assert(held.count == 1 and consumed == 5)
camp.tick(1) assert(camp.data.meta:get_int('cook_0') == 0)
activate('default:flint_and_steel') camp.water(true) camp.tick(1)
assert(camp.state() == 1 and camp.particles() == 4)
assert(blocks['default:campfire'].item_drop.id == 'default:charcoal' and blocks['default:campfire'].item_drop.amount == 2)
print('steel progression, blast-only melting, fuel/capacity, campfire cooking, quenching, relighting, and creative checks passed')
