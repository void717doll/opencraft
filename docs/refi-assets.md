# built-in refi artwork

world artwork is imported from the supplied REFI_Textures distribution by MysticTempest, licensed under CC BY-SA 4.0. source: https://github.com/MysticTempest/REFI_Textures. the complete supplied license is retained in [REFI-LICENSE.txt](REFI-LICENSE.txt).

[refi-assets.json](refi-assets.json) records every imported source path, destination, source/output dimensions, operation, and sha256 digest. destinations keep their existing names so saved registry ids and lua definitions remain stable.

coverage: 320 png files under the default mod and engine texture directories, with 314 manifest entries. unused legacy artwork and engine exceptions remain recorded; registered beta content and the requested copper tier are available in play.

## adaptations

- inventory, workbench, and furnace screens share refi's `mcl_base_textures_background9.png` panel and `mcl_formspec_itemslot.png` slots. the original images are copied unchanged and sized at runtime; the crafting arrow is clipped from the supplied workbench sheet. the inventory has four working armor slots, a 3d character preview, storage and crafting; there is no shield or offhand slot.
- furnace fire uses the supplied foreground/background sprites. the white furnace progress arrow remains an engine ui asset. slot highlights draw behind items and stack counts. hud hearts retain their compact nine-pixel size, with a four-pixel margin below the hotbar.
- block and item art is 16px. ore/material aliases use the corresponding refi texture, with no fallback to the removed texturepacks directory.
- wheat seeds use the supplied loose-seed sprite, `farming_wheat_seed.png`, in place of the seed-packet icon. planted wheat keeps its separate growth-stage textures.
- grass side overlays are composited over the supplied dirt texture. oak leaves use the supplied desaturated oak variant because the existing engine applies foliage tint.
- zombie and zombie pigman skins use the upper 64x32 section of their supplied 64x64 atlases. slime uses nearest-neighbor reduction from 128x64 to beta's 64x32 coordinates.
- pig saddle, sheep fur, spider eyes, and wolf variants retain their supplied 64x32 atlases. refi supplies one ghast atlas, so `ghast_shooting.png` is an explicit alias of that atlas; shooting animation changes the model rather than inventing a second skin.
- the arrow projectile retains the supplied 64x64 sheet; fireballs retain their 16x16 sprite. projectile models must use those layouts.
- the supplied moon sheet is reduced with nearest-neighbor sampling to preserve the engine's existing 32px phase rectangles. the sun and player skin use the supplied files directly.
- the five vertical breaking-overlay frames are arranged horizontally, with each repeated twice to fit the existing ten-frame progression. the menu background uses the supplied dirt texture.
- full and empty hearts are direct copies of `textures/default_mcl_core/gui_hud/heart.png` and `heart_gone.png`. half hearts use clipping of the full-heart artwork at runtime. the saddle item uses the supplied `mcl_mobitems_saddle.png`.

menu buttons and slider tracks use refi's `mcl_inventory_button9.png`, stretched with a nine-part layout that preserves the border. the hotbar and selection frame use the supplied 182x22 and 24x24 images directly. saved worlds use the supplied filled-map icon. the title screen has a plain dark background; other menus retain the refi dirt background.

font, recipe, toast, and the default mod's white furnace progress arrow remain engine ui assets. the old title panorama and widget atlas remain unused files and are no longer loaded. the six labeled debug block faces retain their original engine artwork, resized from 32px to 16px with nearest-neighbor sampling. the manifest accounts for these separately from refi artwork and lists six unchanged ui files, including the two unused files.

## validation

all 316 manifest entries were checked for readable png data, destination coverage, expected dimensions, and matching source/output digests where a supplied source exists. grass-side composition pixels match the supplied dirt and overlay layers. all block/item images are 16x16. default lua texture paths resolve, including generated tool and armor names, and both sky resource keys resolve through the texture configuration. the retained license matches the supplied file byte for byte. native rendering remains an integration check.

armor uses the supplied leather, chain, iron, gold, and diamond icons and 64x32 worn atlases unchanged. tnt and gravel use `alternate_textures/texture_alts`; ferns, roses, and mushrooms use the main pack.

copper ore uses the complete `high_vis_ores/mcl_copper_ore_highvis_alt.png` alternate; the main ore sprite is an overlay. its ingot and five tools use the supplied `default_mcl_core` copper textures unchanged.

the armor hud uses unchanged `textures/HUD/hbarmor_icon.png` and `hbarmor_bgicon.png`, displayed at the same compact size as hearts. breaking cracks use alpha blending to preserve the transparent areas of the supplied overlay.

## exploration and farming assets

`mcimport_mods/nether` and `mcimport_mods/quartz` supply basalt, masonry, quartz, and ore artwork. the current red netherrack stays in place; the transparent quartz ore overlay is composited onto it. `minetest_game_mods/caverealms` supplies cave coverings, fungi, crystals, and glowworms; transparent coverings are composed with the existing stone tile. `mcimport_mods/crops`, `skyfactory/farming`, and `skyfactory/bonemeal` supply the selected crop stages, food, seeds, and fertilizers. these use the existing 16px atlas. exact sources, compositions, dimensions, and hashes are recorded in `refi-assets.json` under MysticTempest's existing cc by-sa 4.0 attribution.

`minetest_game_mods/bees` supplies the hive, comb, honey, wax, smoker, and small bee sprite textures. these are unchanged copies; the bee is rendered as a small billboard around active hives.

additional bundled textures cover birch and spruce wood, leaves and saplings; five live/dead coral sets; cobwebs; and mineclonia music discs. their source paths and checksums are recorded in `refi-assets.json`.

stone slabs use the unchanged top and side textures from `textures/stairs_mcl_stairs_mcl_walls`. oak, sandstone, and cobblestone slabs reuse their existing block textures.

double chests use the unchanged front halves, back half, and top half from `textures/mcl_chests`. runtime texture coordinates mirror the back and top onto their matching halves; the front has one central latch.

## tin and bronze

tin ore, tin and bronze ingots, and four bronze tools use unchanged `textures/default_mcl_core` artwork. the hoe comes from `textures/farming_mcl_farming`; all four armor icons and worn layers come from `minetest_game_mods/3d_armor/3d_armor`. the supplied pack is now located at `~/Desktop/oldold/refi_textures`. all sixteen copies and hashes are recorded in the manifest.

tin's transparent mineral sprite is layered over the existing stone tile while building the atlas, using `stone.png^tin_ore.png`. the source png stays unchanged, and the combined tile is used for terrain, held blocks, drops, and inventory cubes.

climbing rope uses an unchanged `skyfactory/farming/farming_hemp_rope.png` inventory sprite and the braid-like `mcimport_mods/basic_materials/basic_materials_chain_brass.png` tile on a thin vertical strand. the two supplied refi files and their hashes are recorded in the manifest.

copper, bronze, and rope receive material palettes when their textures load: reddish orange copper, warm brown bronze, and natural tan rope. the same mapping covers inventory and held items, dropped items, copper ore, and worn bronze armor. stone-gray ore backgrounds, wooden handles, shading, and transparency are preserved; the copied source png files remain unchanged.

steel tool textures use a charcoal palette on their gray metal pixels. wooden handles and dark outlines retain their source colors. this replaces the separate mesh-brightness adjustment so inventory, held, and dropped tools share the darker material.

copper oxidation variants reuse unchanged copper-tool sprite copies with separate exposed, weathered, and oxidized material palettes. only the metal pixels gain green patina patches; handles and transparent pixels retain their original appearance.

piston sides now use refi's `mesecons_piston_bottom.png`, matching mineclonia's forward-facing wood band. the head uses the supplied pusher back and left textures; its quarter-block plate and shaft extending into the shortened base follow mineclonia's node boxes, converted to the engine's vertical z axis.

the cyan interior marker in refi's piston-on-front tile is mapped to dark metal at load time so it does not form a bright rim around the shaft.

15 unchanged refi tree textures cover jungle, acacia, and dark oak logs, planks, leaves, and saplings from `textures/default_mcl_core/trees`. seven unchanged amethyst textures come from `textures/mcl_amethyst`. all 22 source paths, dimensions, and sha256 hashes are recorded in the manifest under MysticTempest's cc by-sa 4.0 credit. mineclonia's tree registrations, plant support rules, and amethyst growth and attachment behavior were used as references; the existing beta terrain generator remains in use.

the experience bar and background are unchanged copies from `textures/HUD`, rotated when drawn. the source paths and checksums are in the manifest. slabs and stairs reuse their parent block textures.
