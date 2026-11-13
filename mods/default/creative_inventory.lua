function show_creative_inventory(key_id, client)
	if not client.creative then return end
	local gui = LuaGUI.new(key_id)
	gui:set_size(195, 136)
	gui:set_centered(true)
	gui:image {
		name = "img_background", pos = {x = 0, y = 0},
		texture = mod:path() .. "/textures/gui/creative_window.png",
		clip = {x = 0, y = 0, width = 195, height = 136},
	}
	gui:text_input {
		name = "search", pos = {x = 82, y = 6}, width = 86, height = 8,
		placeholder = "Search...", placeholder_color = {192, 192, 192}, inventory = "inv_catalog",
	}
	gui:inventory {
		name = "inv_catalog", pos = {x = 8, y = 17},
		inventory = {source = "catalog", count = 45}, size = {x = 9, y = 5},
		is_read_only = true, shift_destination = "inv_hotbar",
	}
	gui:scroll_bar {
		name = "scroll_bar", pos = {x = 175, y = 18},
		texture = mod:path() .. "/textures/gui/tabs.png",
		clip = {x = 232, y = 0, width = 12, height = 15},
		clip_selected = {x = 244, y = 0, width = 12, height = 15},
		widget = "inv_catalog", min_y = 0, max_y = 75,
	}
	gui:inventory {
		name = "inv_hotbar", pos = {x = 8, y = 111},
		inventory = {source = "player", player = "player", inventory_name = "main", offset = 0, count = 9},
		size = {x = 9, y = 1},
	}
	gui:show(client)
end

mod:key {
	id = "creative_inventory", name = "Creative Inventory", default_key = "H",
	callback = function(key_id, client) show_creative_inventory(key_id, client) end,
}
