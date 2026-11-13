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
if not mods then mods = {} end

mods["default"] = opencraft.mod_loader:register_mod("default")
mod = mods["default"]

dofile("blocks.lua")
dofile("items.lua")
dofile("tools.lua")
dofile("recipes.lua")
dofile("sky.lua")
dofile("trees.lua")
dofile("biomes.lua")
dofile("dimensions.lua")

dofile("listeners.lua")
dofile("creative_inventory.lua")
dofile("inventory.lua")

dofile("item_drop.lua")


dofile("mobs.lua")
dofile("tnt.lua")
dofile("armor.lua")
dofile("copper.lua")

-- append expansion registrations so existing numeric block ids stay stable.
dofile("farming.lua")
dofile("nether.lua")
dofile("caverealms.lua")
dofile("bonemeal.lua")
dofile("bees.lua")
dofile("beds.lua")
dofile("clock.lua")
dofile("minecarts.lua")
dofile("cherry.lua")
dofile("spyglass.lua")
dofile("lush_caves.lua")
dofile("dungeons.lua")
dofile("steel.lua")
dofile("campfires.lua")
dofile("fauna.lua")
dofile("mineshafts.lua")
dofile("music_discs.lua")
dofile("beta_woods.lua")
dofile("ocean.lua")
dofile("slabs.lua")
dofile("stairs.lua")

dofile("beta_mechanics.lua")
dofile("navigation.lua")
dofile("decorations.lua")
dofile("exploration.lua")
dofile("bronze.lua")
dofile("rope.lua")
dofile("copper_oxidation.lua")

dofile("tree_variants.lua")
dofile("amethyst.lua")
dofile("tree_biomes.lua")
