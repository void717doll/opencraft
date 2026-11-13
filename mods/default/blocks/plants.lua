for _, plant in ipairs({{"fern", "Fern"}, {"rose", "Rose"}, {"mushroom_brown", "Brown Mushroom"}, {"mushroom_red", "Red Mushroom"}}) do
	mod:block {
		id = plant[1],
		name = plant[2],
		tiles = plant[1] .. ".png",
		inventory_image = plant[1] == "fern" and "fern_item.png" or nil,
		hardness = 0,
		draw_type = "xshape",
		is_opaque = false,
		is_collidable = false,
		groups = {attached_node = 3, om_soil_plant = (plant[1] == "fern" or plant[1] == "rose") and 1 or nil, om_replaceable = plant[1] == "fern" and 1 or nil},
	}
end

mod:crafting_recipe {
	result = {id = "default:mushroom_stew", amount = 1},
	pattern = {"R", "M", "B"},
	keys = {R = "default:mushroom_red", M = "default:mushroom_brown", B = "default:bowl"},
}
