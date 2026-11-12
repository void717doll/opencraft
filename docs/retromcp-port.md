# retromcp port

source: the supplied `~/retromcp` checkout configured for beta 1.7.3. server java supplies simulation and client java supplies geometry. oldworld retains c++17, lua, bgfx, registries, and separate rendering/simulation threads.

| source | implementation |
| --- | --- |
| `NoiseGeneratorPerlin`, `NoiseGeneratorOctaves`, `ChunkProviderGenerate` | java random, bulk octave noise, density, interpolation, surface replacement, sand, gravel, soil, sandstone, ice, and bedrock |
| `NoiseGenerator2`, `NoiseGeneratorOctaves2`, `WorldChunkManager`, `BiomeGenBase` | temperature, humidity, climate adjustment, and all beta overworld biome classifications |
| `MapGenBase`, `MapGenCaves`, `MapGenCavesHell`, `ChunkProviderHell` | eight-chunk cave replay, rooms, branching, water avoidance, low lava, grass restoration, and nether density/surface/caves |
| `EntityLiving` and beta mob subclasses | complete registered roster, distinctive simulation, interactions, health, timers, and drops |
| `ModelRenderer`, `TexturedQuad`, beta mob models | cuboids, source UVs, mirrored limbs, overlays, scaled slimes/giants, and projectile models |
| supplied refi directory | built-in block, item, mob, sky, breaking, and heart artwork; [asset inventory and license](refi-assets.md) |

## terrain verification and limits

source vertical y maps to oldworld z. the generator builds canonical 16×16×128 columns before slicing into the engine's 16×16×32 chunks. source initialization order, integer overflow, sine lookup, and noise caching are retained.

comparisons against java compiled from the supplied checkout passed:

- 2,304 climate/biome samples matched exactly.
- 294,912 overworld surface bytes matched exactly before caves.
- 1,966,080 cave output bytes matched across four seeds, three coordinates, and five input patterns.
- 524,288 nether surface bytes and 524,288 nether carved bytes matched, including a 64-bit seed.
- the earlier 1,350 octave samples matched exactly; 3,825 density samples differed by at most 1.14e-13.

the parity claim covers base climate, density, surface, and cave generation. population changes are described below. flora, cross-chunk feature scheduling, portals, and nether fire/mushroom/spring population still differ from the reference. terrain below y=0 and above the source region is air. placement is limited to y=0 through y=255; bedrock keeps the source irregular y=0 through y=4 layers. existing saved chunks remain unchanged; newly explored terrain can meet older terrain at seams. parallel generation, superchunks, and a public terrain-formula api remain unfinished.

## mobs and controls

the engine registers `pig`, `zombie`, `cow`, `sheep`, `chicken`, `skeleton`, `creeper`, `spider`, `slime`, `ghast`, `pig_zombie`, `wolf`, `squid`, and mod-spawned `giant`. all spawns respect the 64-mob cap and loaded space. skeleton arrows and ghast fireballs are separate visible entities.

left click attacks a visible target within three blocks. right click milks cows with an empty bucket, shears sheep, saddles pigs, and tames/heals/orders wolves. chickens lay eggs, spiders climb and leap, slimes hop and split, pig zombies retaliate together, squid swim, and creepers use a 30-tick fuse. wolves preserve their owner and sitting state. fireballs can be reflected by attacks. food heals immediately, including at full health; stew returns a bowl. old oversized food stacks split on load without losing surplus.

movement uses direct pursuit, obstacle jumping, and the shared swept collision solver. beta's full pathfinder, mob vocalizations, riding controls, wolf sheep hunting, and lightning-charged creepers remain absent. natural spawn scheduling uses oldworld's bounded periodic attempts rather than the complete beta spawning algorithm. explosion rays use explicit resistance for common materials and approximate the remainder from hardness. sheep colors are rendered, but wool drops currently use the single white wool item.

model cuboids and UVs follow the supplied client source. limb motion derives from client displacement; separate head rotation, attack swings, wing timing, wolf shaking, slime squishing, ghast charge interpolation, and squid pitch are adapted or absent. refi lacks distinct charging-ghast artwork; that pose uses the normal atlas. these are not claims of identical animation.

## saves and local messages

player health uses the optional `HEAL` save trailer; older saves default to full health. original mob and drawable records retain their byte layouts. new `BetaMob` component 9 stores subtype, size, fuse, anger, egg/jump/attack timers, ownership, color, saddle, and motion state separately. older engines cannot read the new component.

the game is local only. serialized messages connect rendering to the simulation through in-process queues; no socket transport, peer handshake version, or dedicated executable remains. health, attacks, interactions, and entity hitboxes use local messages. drawable type 3 carries scale/appearance while type 2 keeps its existing save layout. gameplay effects remain simulation-owned.

## first-person wield

the first-person hand and selected block/item use a separate 72-degree camera and depth-cleared pass between the scene effect and hud, following the rendering boundaries in `~/lunarchy/src/client/camera.cpp` and `wieldmesh.cpp`. empty slots show the refi player arm; blocks reuse the existing cube mesh and items gain thickness along their opaque pixel edges. plantlike blocks and blocks with an inventory image use that sprite rather than a textured box. partial block offsets scale with the mesh.

held blocks and sprites now use lunarchy's normal right-hand position `(55, -35, 65)` and rotation `(-100, 120, -100)`, with wield units divided by 100 and forward z reflected for bgfx. block width is 0.3; sprites are 0.4 wide and 0.04 thick, matching lunarchy's 30/40 mesh scales and extrusion ratio. the optional mineclonia tool enlargement and saved left-hand settings are not applied. narrow windows move the pose inward.

the 286ms swing follows lunarchy's position curves and quaternion interpolation toward `(80, 30, 100)`. walking adds its horizontal and vertical bob, while switching lowers and raises items over 125ms each. block meshes retain the z-up conversion and empty slots retain the existing refi-skinned arm. arm inertia, tool recharge recoil, and custom mesh wield models remain absent.

inventory tooltips follow beta `GuiContainer.drawScreen`: only the trimmed item name, white text with a dark shadow, a solid 75-percent-opaque black rectangle, three pixels of padding, and a twelve-pixel offset above/right of the cursor. tooltips hide while carrying a stack and move inward near window edges. dropped plants and seeds use their item sprite instead of a thin crop-shaped box.

## survival rules

survival disables flight, noclip, sprint, starter gifts and cheat commands. creative mode restores the item catalog, unlimited placement and flight. player movement now follows beta `EntityLiving`, `Entity`, and `MovementInputFromOptions`, including fixed physics ticks, momentum, friction, gravity, swimming, ladders, stepping, and sneaking. server position checks use the same axis ordering. the [survival inventory](beta-survival.md) records equipment durability, mining rules, loot behavior, save compatibility, verification, and remaining differences.

fall, drowning, lava/burn, cactus and suffocation damage retain beta hurt immunity. food heals immediately. death drops inventory with equipment wear preserved and waits for the player to choose respawn or return to the title menu. common block hardness, tool speeds, effective-block lists, harvest tiers and weapon damage follow the supplied beta source. loot falls to the ground and partial pickups retain the remainder. water/lava, mobs, falling blocks and dropped-item movement retain the [lunarchy world physics port](lunarchy-physics.md).

water/lava now use queued propagation and level parameters; unsupported sand/gravel fall as saved entities. these systems follow lunarchy rather than the beta generator reference.

armor protection, wear and model layers now follow the supplied beta `ItemArmor`, `InventoryPlayer` and `RenderPlayer`. tnt uses the beta fuse and motion with the shared blast propagation. the 99-item limit, unstackable cookies, creative mode and the copper step before iron are requested deviations.

## nether population

new nether terrain includes hanging glowstone from retromcp's `WorldGenGlowStone1` and `WorldGenGlowStone2`: an air block beneath netherrack starts a cluster, then 1,500 growth attempts add air blocks with exactly one glowstone neighbor. both source passes retain their counts and height ranges. clusters cross horizontal and vertical chunk borders and emit block light. existing saved chunks remain unchanged.

population uses a separate seed for each source column and reads undecorated terrain. this makes results independent of chunk loading order, but does not reproduce beta's shared lava/fire/glowstone random stream or interactions between separate columns' clusters.

nether spawn passes use `BiomeGenHell`'s equal ghast and zombie-pigman weights and `SpawnerAnimals`' three groups of four attempts. they check solid support, the full hitbox, liquids, existing mobs, and a 24-block exclusion around players. ghasts have the source one-in-twenty spawn check and one successful spawn per column per pass; pigmen allow four. only loaded columns within eight chunks of a player are considered. passes run once per second under the existing 64-mob cap, rather than beta's per-tick global population budget. existing ghast fireballs and neutral pigman retaliation remain active.

## minecart and rail behavior

cart movement now follows `~/modernmcp`'s `EntityMinecart`: movement on each horizontal axis is limited to 0.4 blocks per tick, slopes change momentum, occupied carts retain more momentum, and powered rails accelerate after drag. powered rails without a signal halve cart speed and stop carts below 0.03 blocks per tick. a solid block behind a stationary cart starts it along a powered rail.

track connections follow `BlockBaseRailLogic`, including curves, slopes, existing neighbor connections, and redstone selection at junctions. powered and detector rails stay straight or sloped. mineclonia's eight-rail power range and detector behavior remain in use. invalid torch and rail placements are rejected before prediction or item consumption; removing valid support still drops an attached block. redstone torches support wall mounting while preserving the lit-state bits in older saves.

this covers the existing rideable cart and three rail types. container carts, furnace carts, activator rails, and automatic mob passengers are not included.

## terrain and content extensions

modernmcp's `MapGenRavine` now carves the canonical lower 128-block columns after beta caves. forty columns matched the source java output byte for byte. abandoned mineshaft candidates and recursive room, corridor, crossing, and stair layouts follow `MapGenMineshaft` and its component classes; twenty-two layouts matched java. population uses separate deterministic piece seeds and clips writes to the current chunk. reverse chunk generation preserved blocks and loot in the integration check.

mineshafts contain timber supports, rails, webs, chests, and spider spawners. chest minecarts are represented by chests; cave spiders are represented by ordinary spiders. these are adaptations, not full modern structure parity. coral reefs populate shallow non-cold oceans. small nether brick ruins populate exposed nether shelves; they are not full fortresses. new structures and trees appear in newly generated terrain.

birch and spruce shapes follow retromcp's `WorldGenForest` and `WorldGenTaiga2`. forest trees have a one-in-five birch choice; taiga trees use spruce. leaves have a one-in-twenty chance of dropping the matching sapling, while shears return leaves. leaves never drop apples. dungeon chests retain one shared one-in-a-million rare roll for an apple, golden apple, or music disc.

regular and redstone torches share mineclonia's floor and wall mesh geometry, converted to the engine's vertical z axis. hand-view depth affects both held items and the empty hand; it is the only hand-view control and lives under graphics. the interface coordinate option displays minecraft-style x/y/z, with vertical height labeled y.

## compressed world folders

modernmcp supplies the one-folder-per-world organization. openminer keeps its own save format inside `saves/<name>/map.sqlite`; this is not an anvil or nbt save and cannot be opened by minecraft. lunarchy's sqlite chunk storage and whole-record zstandard compression are the storage reference. chunk palettes store block names, block parameters, and sparse inventories/metadata. lighting is rebuilt on load. entities, players, game mode, generation revision, bed spawn, health, score, and armor remain in the world-state record.

saves use a sqlite transaction. unchanged chunks are skipped; chunks with inventories or timers are checked on each save. old flat `.dat` saves remain readable and migrate on save, retaining the original as `<name>/legacy.dat`. new worlds do not need that backup. the menu lists both formats, sorts by last-save time, and renames or deletes the whole world folder.

## ores and building blocks

gold, redstone, diamond, and lapis now populate new overworld chunks using retromcp's `WorldGenMinable` ellipsoids and `ChunkProviderGenerate` counts. gold has two size-eight attempts per column at y=0–31; redstone has eight size-seven attempts and diamond one size-seven attempt at y=0–15. lapis has one size-six attempt at `random(16) + random(16)`. these are the starting heights: beta adds two to four blocks to the vein endpoints, and the ellipsoids can extend beyond them. y=12 is in the diamond and redstone range.

neighboring source columns are replayed and writes are clipped to each destination chunk, keeping the new veins independent of chunk loading order. dirt, gravel, coal, iron, copper, and the four deeper ores share this vein population stream. dirt has twenty size-32 attempts below 128, gravel ten size-32 attempts below 128, and coal twenty size-16 attempts below 128. iron has twelve size-eight attempts below 64; copper has eighteen size-eight attempts starting at 8–79 to preserve the requested balance. earlier beta lakes, dungeons, and clay do not advance this stream, so this reproduces the vein algorithm and placement ranges without seed-identical beta ore locations. saved chunks are unchanged. vertical clipping skips work outside the destination chunk while preserving every random draw.

fences now connect to neighboring fences, use beta's post and rail dimensions, and have its 1.5-block-high collision box. placing a fence requires a solid block or fence below it; removing that support afterward does not drop the fence. inventories, held fences, and dropped fences use the same parts.

oak, stone, sandstone, and cobblestone slabs combine into matching double slabs. double slabs drop two halves. the stone slab uses refi's slab top and side textures. beta's wooden slab also requires a pickaxe. mixed slabs remain separate, and merging cannot fill the player's hitbox.

oak and cobblestone stairs use beta's four upright directions and two-part collision shape. six blocks craft four stairs in either mirrored pattern. breaking a stair returns its parent block, following beta's drop behavior; cobblestone stairs require a pickaxe. stair icons, held items, and dropped items use the stair shape.

two neighboring chests share a 54-slot screen with the same slot order from either half. each half keeps its own saved contents, so breaking one drops only its contents and leaves the other as a single chest. placement rejects triples, and a solid block above either lid prevents opening the pair.

mining uses one shared tool-speed calculation. axes speed up chests, logs, planks, and bookshelves; beta's lists omit fences and crafting tables. furnaces require a pickaxe to drop, but do not receive its speed bonus. lit furnaces drop the ordinary furnace. chest, furnace, and crafting-table contents scatter in stacks of up to thirty, preserving item counts and damage. breaking the same position again produces no additional contents.

verification: 300 vein cases matched the supplied java generator, including the next random value. the build and 200 tests passed. local handler checks covered slab consumption, collision, double drops, container contents, lit furnaces, fence support, and reversed ore generation order. native previews checked connected fences, slab textures, and inventory meshes.

chest and stair checks cover four-direction placement, stair walking and item landing, crafting, shift transfers across both chest halves, blocked lids, triple rejection, saving across chunk boundaries, and breaking either half without duplicating contents. native checks exercised chest clicks, reopening from either half, and removal while open; screenshots checked the 54-slot layout, joined chest textures, and stair meshes. stairs retain the engine's existing partial-block lighting.

## beta building and navigation

signs support four lines of fifteen printable characters, with standing and wall mounts and four cardinal orientations. seven nonblank small refi paintings are available; larger paintings are not included. note blocks use twenty-five pitches, five supporting-material instruments, left-click previews, and rising-edge redstone playback.

piston push-line checks follow `BlockPistonBase`: twelve blocks maximum, immovable obsidian and tile storage, six facing directions, sticky single-block retraction, and power checks around the block above the base. block movement is immediate in this engine. entity movement is swept against surrounding blocks; a pinned entity prevents extension rather than being embedded.

map exploration uses persistent paper items and a north-up surface palette, with 128 by 128 pixels. maps opened since version 0.0.17 cover 128 by 128 blocks; earlier maps retain their 1024-block coverage. empty-map activation, paper size, cardinal arrows and off-map dots follow mineclonia’s `mcl_maps`. maps reveal loaded sunlit surfaces while held. the compass follows default world spawn and spins in the nether. chest carts use twenty-seven slots; furnace carts consume coal or charcoal and push other carts. cargo is saved in a separate component so existing passenger cart serialization remains readable.

## underground exploration

new overworld terrain can contain a nine-block-wide mossy cobblestone room, a short entrance passage, a torch, and a supply chest. one in three 256-block cells contains a candidate, placed below its surface. the room is assembled independently in each intersecting chunk and existing chest metadata is preserved.

zombies, skeletons, and spiders spawning below y=56 have a one-in-192 chance of becoming named encounters, with at most two present in a world. they keep the original model and texture, with a faint cool tint, half again their normal health, one extra attack damage, and slightly faster movement. their name appears in chat when visible nearby. names and boosted stats survive saving; the legacy mob records retain their layouts. defeated named monsters drop two iron ingots and a treasure note alongside their normal loot.

dungeon chests can also contain a treasure note. new notes record a nearby buried-cache destination in `saves/<world>/notes/note_<id>.txt`; subsequent readings retain those coordinates, and earlier notes keep their ruin destinations. reading uses minecraft coordinate labels, with vertical height labeled y. notes are not consumed and do not change the rare apple, golden apple, or disc rolls. ore and ruin changes apply to new terrain.

piston presentation follows mineclonia's piston-off side artwork and piston-on/pusher boxes. the head shaft now joins the recessed base instead of leaving a quarter-block gap.

ore generation clips each ellipsoid to all three chunk axes before visiting voxels, preserving the generator's random draws and placement order. a local benchmark over 256 solid chunks averaged about 281 ms before and 125 ms after across three runs without compiler load. this measures the ore pass, not overall frame time.
