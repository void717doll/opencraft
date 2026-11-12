for _, material in ipairs({"exposed_copper", "weathered_copper", "oxidized_copper"}) do
	for _, tool in ipairs({{"pickaxe", Capability.Pickaxe}, {"axe", Capability.Axe}, {"shovel", Capability.Shovel}, {"hoe", 0}, {"sword", 0}}) do
		register_tool(tool[1], material, 5, tool[2], 1, 190)
	end
end
