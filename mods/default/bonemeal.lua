function farming.fertilize(pos, world, strength)
	if lush_caves and lush_caves.fertilize(pos, world) then return true end
	local state = world:get_block_state(pos.x, pos.y, pos.z)
	if state then
		local name = state:block():string_id()
		local tree = name == "default:sapling" and "oak" or name:match("^default:(.+)_sapling$")
		if tree == "oak" or tree == "cherry" or tree == "birch" or tree == "spruce"
		 or tree == "jungle" or tree == "acacia" or tree == "dark_oak" then
			return world:grow_tree(pos.x, pos.y, pos.z, "default:" .. tree)
		end
	end
	if state and state:block():string_id() == "default:farmland" then
		pos = {x = pos.x, y = pos.y, z = pos.z + 1}
	end
	return farming.grow(pos, world, strength * 2 - 1)
end

function farming.use_fertilizer(pos, block, player, world, client, server, strength)
	local stack = player:held_item_stack()
	if stack:amount() == 0 then return end
	if not farming.fertilize(pos, world, strength) then return end
	server:consume_held_item(player, 1, client.id)
	local state = world:get_block_state(pos.x, pos.y, pos.z)
	local z = pos.z + (state and state:block():string_id() == "default:farmland" and 1 or 0)
	world:emit_particles(pos.x, pos.y, z, 0)
end

mod:item {
	id = "mulch", name = "Mulch", tiles = "refi_bonemeal_mulch.png",
	on_item_activated = function(pos, block, player, world, client, server)
		farming.use_fertilizer(pos, block, player, world, client, server, 1)
	end,
}
mod:item {
	id = "fertilizer", name = "Fertilizer", tiles = "refi_bonemeal_fertiliser.png",
	on_item_activated = function(pos, block, player, world, client, server)
		farming.use_fertilizer(pos, block, player, world, client, server, 3)
	end,
}

mod:crafting_recipe {
	result = {id = "default:mulch", amount = 1}, pattern = {"LL", "LL"},
	keys = {L = "default:oak_leaves"},
}
mod:crafting_recipe {
	result = {id = "default:mulch", amount = 1}, pattern = {"WW"},
	keys = {W = "default:wheat"},
}
mod:crafting_recipe {
	result = {id = "default:fertilizer", amount = 1}, pattern = {"BM"},
	keys = {B = "default:bone_meal", M = "default:mulch"},
}

mod:crafting_recipe {
	result = {id = "default:fertilizer", amount = 1}, pattern = {"PM"},
	keys = {P = "default:mycena_powder", M = "default:mulch"},
}
