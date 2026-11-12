# openminer

openminer is a small, free and libre block-building game inspired by minecraft beta 1.7.3. gather materials, make tools, build a home, and explore. it runs offline and saves worlds locally.

choose survival to mine, craft, farm, and fight mobs, or creative to build with unlimited blocks. food restores health directly. tools wear out, armor provides protection, and caves lead to ores and underground lakes. obsidian portals connect the overworld to the nether.

## changes in this version

- terrain, caves, biomes, mining, movement, and survival rules use minecraft beta 1.7.3 as their reference.
- refi textures replace the old block, item, mob, and interface artwork. the inventory and hud use compact hearts, an armor bar, item names above the hotbar, and optional durability text.
- held items use the normal right-hand pose. dropped items have ground collision, pickup delays, rotation, and bobbing.
- ordinary blocks and items stack to 99. food, tools, and armor do not stack. food heals directly, without a hunger bar.
- tools wear out, correct tool tiers control drops, and equipment keeps its damage through inventory moves and saves. copper comes between stone and iron. dark steel tools have 5,000 durability and stone-tool mining speed.
- armor appears on the player model. leather adds fall protection, gold adds fire protection, and chainmail adds general damage protection.
- material sounds cover footsteps, digging, breaking, placement, and item pickups.

## additions

- survival and creative world creation, saved game modes, automatic names for unnamed worlds, and a searchable creative inventory.
- copper ore and tools, craftable tnt, ferns, three-dimensional armor, and held-item tooltips.
- natural diamond, redstone, lapis, and gold veins; connected fences, merging slabs, oak and cobblestone stairs, and double chests with shared storage.
- nether portals, ghasts, zombie pigmen, glowstone clusters, basalt, quartz, and nether masonry.
- cave moss, fungi, crystals, lush cave plants, cherry trees, and a spyglass.
- potatoes, tomatoes, barley, extra food recipes, bone meal, mulch, fertilizer, beehives, honey, and wax.
- ocelots and bats, working buckets, beds, clocks, minecarts, powered rails, and detector rails.
- dungeon rooms with loot chests and mob spawners, plus jukeboxes with eight mineclonia music tracks.
- ravines, abandoned mineshafts, coral reefs, nether brick ruins, and birch and spruce trees.
- connected beta-style ore pockets, including larger coal veins and underground dirt and gravel. copper remains more common than iron.
- rare named cave monsters and treasure notes leading to small mossy underground ruins. right-click a note to read its coordinates; notes are saved per world. new ore pockets and ruins appear in newly generated terrain.
- editable standing and wall signs, seven small refi paintings, tunable note blocks, and regular and sticky pistons.
- paper maps that fill while exploring, compasses pointing to world spawn, chest minecarts, and coal-powered furnace minecarts.
- building from y=0 through y=255, a hand-view depth slider in graphics settings, and optional coordinates on the hud.

## removals

- multiplayer joining, hosting, dedicated-server startup, and address settings.
- shields, offhand slots, fireworks, and the block-information overlay.
- texture-pack selection and overrides; refi artwork is bundled with the game.
- starter gifts, cheat commands, sprint, and survival flight. creative keeps its own inventory and flight controls.

## play

choose **play** to create or open a world. new worlds have a seed and a survival or creative setting. leaving the name empty creates `World 1`, `World 2`, and so on.

| control | action |
| --- | --- |
| wasd | move |
| mouse | look |
| space | jump |
| shift | sneak |
| left click | break blocks or attack |
| right click | place blocks or use items |
| middle click | pick the pointed block; creates a stack only in creative |
| e | inventory, or the creative catalogue in creative worlds |
| h | creative inventory |
| i | inventory, crafting, and armor |
| 1–9 or mouse wheel | select a hotbar slot |
| q | drop one item |
| v | toggle noclip in creative |
| escape | pause; statistics for the current world |
| f11 | fullscreen |

controls, sound volume, and hud options can be changed in settings. gamma and hand-view depth are under graphics; always-visible coordinates are under interface. pause and choose statistics to view time played, movement, combat, mining, building, crafting, and item use for this world. older worlds start counting these statistics after this update.

right-click a sign to edit its four lines, or a painting to choose another picture. note blocks play on left click, tune on right click, and play once when redstone power turns on; the block below selects the instrument. pistons move up to twelve blocks, and sticky pistons pull one block back. containers and obsidian stay in place.

craft an empty map and right-click to open it. new maps cover 128 by 128 blocks and show loaded surface terrain while held, with a direction arrow that becomes a dot outside the map. older maps keep their original coverage. map files stay with their world in its `maps` folder. compasses point to world spawn. right-click a chest minecart to open its storage, or a furnace minecart with coal or charcoal to power it.

worlds are stored in `saves/<world name>/map.sqlite`, with compressed chunks and player data in one database. maps and treasure-note records use subfolders beside it. the world list puts the most recently saved world first. old `.dat` worlds migrate when saved; their original file is retained as `legacy.dat` inside the world folder. that backup still occupies its original space.

## build

requires git, cmake, pkg-config, a c++17 compiler, sqlite3 and zstd development libraries, and the platform development libraries needed by sdl and bgfx. run from the project directory:

```sh
git submodule update --init --recursive
cmake -S . -B build -DOM_BUILD_TESTS=OFF
cmake --build build --target openminer -j4
./build/openminer
```

launch a source build from the project directory so it can find its files.

## packages

the `packages` github actions workflow builds on pushes to `master` or `main`, version tags, pull requests, and manual runs. download the packages from that run's artifacts:

- linux x64: deb, rpm, and AppImage.
- macos 14 or newer: separate intel and apple silicon dmg files containing `OpenMiner.app`.
- windows: separate 64-bit and 32-bit zip files. extract the whole zip and launch `openminer.cmd`.

each artifact includes sha256 checksums. package versions come from `source/common/core/EngineConfig.hpp`; a version tag must match `v` followed by that version. the workflow packages files without publishing a release. mac builds are ad-hoc signed, without apple notarization.

installed linux packages and AppImages keep saves in `${XDG_DATA_HOME:-~/.local/share}/openminer`; mac apps use `~/Library/Application Support/OpenMiner`. set `OPENMINER_DATA_DIR` to choose a different folder. windows zip builds keep saves beside the executable. existing source-build saves are kept in place; copy the `saves` folder to the package's data folder to continue them.

runner architectures follow [github's supported runner labels](https://docs.github.com/en/actions/reference/runners/github-hosted-runners). the AppImage uses [linuxdeploy](https://github.com/linuxdeploy/linuxdeploy), and mac library bundling uses [dylibbundler](https://github.com/auriamg/macdylibbundler). the mac app uses a native launcher and follows [apple’s bundle signing layout](https://developer.apple.com/library/archive/technotes/tn2206/).

## credits

- **OpenMiner**, by Unarelith, Quentin Bazin, and its contributors: the original project and engine.
- **Luanti** and **Lunarchy**: references for movement, block physics, held items, and sound playback.
- **RetroMCP**: the beta 1.7.3 source reference for terrain, survival rules, mobs, and models. [port notes](docs/retromcp-port.md).
- **ModernMCP**: references for ravines, mineshaft layouts, world height, minecarts, and save-folder organization.
- **Mineclonia**: gameplay references, models, and sounds. [sound credits](docs/sound-assets.md).
- **MysticTempest's REFI_Textures**: the block, item, mob, and interface textures. [texture credits](docs/refi-assets.md) and [asset inventory](docs/refi-assets.json).
- the authors of the libraries in `external/`, including SDL, bgfx, EnTT, sol2, GLM, SFML, tinyxml2, and zlib.

## licenses

OpenMiner's license is retained in [LICENSE](LICENSE). refi textures are licensed under [CC BY-SA 4.0](docs/REFI-LICENSE.txt). imported models and sounds retain their upstream licenses and credits alongside the files.
