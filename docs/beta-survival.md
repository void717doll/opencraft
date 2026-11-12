# beta survival

reference: the supplied beta 1.7.3 sources in `~/retromcp/minecraft/src/net/minecraft/src`. gameplay stays in the existing c++ and lua engine.

## movement

`EntityLiving.moveEntityWithHeading`, `Entity.moveFlying`, `Entity.moveEntity`, and `MovementInputFromOptions` supply the player rules. physics runs at 20 ticks per second, with interpolation for rendering. motion uses blocks per tick internally. walking retains momentum, ground friction depends on slipperiness, ice is slippery, sneaking scales input by 0.3, and jumping applies the source's 0.42 impulse. gravity, airborne acceleration, swimming, lava drag, ladder descent/climbing, half-block steps, and sneak edge protection follow those source paths. the body is 0.6 by 0.6 by 1.8 blocks. vertical source y maps to engine z; movement input is rotated into the engine's horizontal axes.

player collision clips vertical movement before the two horizontal axes. server position checks use that same ordering, including ascending slab steps. mobs, falling blocks, ground items, and fluid propagation retain the existing lunarchy-based simulation. loaded chunks bound all movement.

## mining and equipment

number keys 1 through 9 select hotbar slots; the mouse wheel cycles through them. q drops one item from the selected slot, preserving equipment damage. these bindings can be changed in settings. drops leave the player's eye area in the look direction and wait 40 simulation ticks before pickup, following beta `EntityPlayer.dropCurrentItem`. thrown-item movement still uses the engine's luanti physics, without beta's random scatter. dropping into an unloaded chunk or when the item-entity cap is full keeps the inventory unchanged.

block interaction reach remains four blocks. common beta blocks use the source hardness values. default pickaxes, axes, and shovels use the source effective-block lists, including their omissions: axes do not speed up workbenches, and pickaxes do not speed up obsidian or redstone ore. harvest tiers still determine whether a broken block drops loot. swords mine at 1.5 strength; shears speed up leaves and wool. mining takes five times as long when the player's head is underwater, and another five times as long while airborne. the wrong-tool branch retains the source's hardness-only penalty. continuous mining waits 250 milliseconds after a completed block.

| equipment | maximum damage |
| --- | ---: |
| wooden tools | 59 |
| stone tools | 131 |
| iron tools | 250 |
| golden tools | 32 |
| diamond tools | 1561 |
| shears | 238 |

as in beta `ItemStack.damageItem`, equipment breaks when accumulated damage exceeds its maximum. digging costs one durability for ordinary tools and two for swords. hitting a mob costs two for ordinary tools and one for swords. hoes wear only when tilling succeeds. shears wear on leaves and sheep, and keep the source's wool speed. unused swings and unsuccessful interactions cost nothing. worn equipment shows its remaining durability in the inventory and hotbar.

inventory swaps, splitting, shift transfers, ground-item pickups, and death drops retain damage. worn tools cannot merge with differently worn tools. existing oversized stacks are preserved; incoming items do not shrink them to modern stack limits. old stacks start undamaged.

## loot, health, and saves

nearby items collect within one block of the player body horizontally after their pickup delay. pickup merges compatible stacks across the whole inventory before filling empty slots, including backpack space when the hotbar is full. full inventories leave the drop on the ground; partial pickups keep the remainder.

dropped blocks and items rotate at a fixed 90 degrees per second with a small visual bob. sprites use the same opaque-edge mesh as held items, with enough ground clearance to stay above the floor. rendering interpolates the 20hz physics positions without changing collision or pickup positions.

mob deaths put loot on the ground for pickup, including environmental deaths. sheep shearing also puts wool on the ground. partial pickups keep the remainder. existing health, immediate food healing, container returns, fall/drowning/lava/fire/cactus/suffocation damage, and hurt immunity remain active. there is no hunger bar.

accepted damage sends a separate hurt event, including fatal damage. the client plays the hurt cue and briefly tilts the camera; hearts blink between the previous and current amounts after a hit. health synchronization and healing do not trigger hurt sounds. equipment breakage also has its own event, so moving or dropping a tool does not play a break sound.

undamaged stacks retain their original serialized bytes. a damaged stack writes the reserved `@oldworld:stack_damage` marker followed by the item id, amount, and damage; readers accept both forms in player inventories, containers, and item entities. older saves remain readable; older executables cannot read newly saved damaged stacks.

death drops the inventory once and leaves health at zero. the death screen shows `you died!`, the score, and respawn/title-menu buttons. respawning returns to the overworld with full health and a new life score. dead players cannot move, attack, or collect drops. closing an inventory returns carried items and temporary crafting ingredients; overflow and items carried at death drop into the world. player-owned items bypass the ambient-loot cap so the cap cannot silently destroy them.

the score appears at the top left as a white label and yellow value, and the death screen shows the same value. beta `EntityPlayer` starts at zero; all bundled `EntityLiving` subclasses keep `scoreValue = 0`, so the reference does not award nonzero points for mining, pickups, or ordinary mob kills. the counter follows that behavior. an optional `SCOR` save trailer preserves the current value; older saves start at zero.

the pause menu stops world ticks and player movement. inventory and death screens leave the world running, matching the beta screen behavior. movement positions are sent after each physics tick: combining several ticks into one displacement could make the server reject valid paths off ledges and around corners.

mob attacks send the actual health removed and the hit position. the client displays a short red model flash, a rising damage number, and a hit sound. fatal player damage adds a chat message naming the attacking mob, projectile owner, explosion source, or environmental hazard. the last five system messages remain visible for fifteen seconds.

the temporary random portal generator is removed. newly generated terrain contains no automatic portals; existing saved structures remain intact.

## verification and limits

800 samples from the original java movement methods matched the c++ calculations exactly for cardinal-facing walking, sneaking, diagonal input, ice, repeated jumping, water, lava, and ladders. the oracle uses a flat floor and controlled material contacts; it does not prove parity for complete worlds or arbitrary view angles. regression coverage exercises axis ordering, slab ascent accepted by the server, sneak edges, equipment breakage, legacy oversized stacks, and serialized inventory/stack boundaries.

this is a survival rules port, not complete beta feature parity. beds, sapling regrowth, weather, fire spread, liquid currents, the full beta mob pathfinder/spawner, knockback, and air/fire hud indicators remain absent or adapted. crafting and inventory messages retain the existing local-client ownership. mining animation and timing use the engine clock rather than the complete beta controller state machine. stepping camera smoothing and source-exact random drops are not ported. material footsteps and player action effects now use the [luanti sound assets](sound-assets.md). automated checks cover movement acceptance, fatal damage, death drops, score persistence, respawning, hit messages, and hazard attribution. full live gameplay and the latest native visuals still need validation.

## item limits

ordinary items and blocks stack to 99; food, tools, and armor stack to one. the old server default of 64 migrates to 99. all food includes cookies, which deliberately differ from beta's eight-cookie stacks.

later food, materials, and block items have been removed using the supplied beta 1.7.3 item list. their recipes are removed too. reserved air slots preserve old numeric block ids, so removing a block does not change surrounding terrain. removed items disappear on loading; oversized surviving stacks split into available inventory slots, with excess dropped nearby.

## armor and tnt

right click an armor piece to equip or swap it, or use the four filtered inventory slots. shift click also equips a matching empty slot. leather, chain, iron, gold and diamond use beta protection, durability scaling, and damage remainder. armor wears when absorbing damage, drops on death with its wear intact, and saves in an optional `ARMR` trailer. the inventory renders the equipped pieces in 3d; an equipped chestplate also covers the empty first-person hand. chain armor has no survival recipe, matching the unavailable fire ingredient in this engine. shields are absent.

tnt is crafted from five gunpowder and four sand. right click it with flint and steel to start the four-second fuse. blasts damage nearby players and mobs, destroy exposed blocks, drop some debris, and ignite nearby tnt with shorter fuses. terrain outside loaded chunks is untouched. the fuse and motion survive saving.

## world modes

blank world names use the next available `World 1`, `World 2`, and so on. new worlds default to survival. the mode button on world creation switches to creative. the choice is saved in the world file and survives renaming and reloading; existing worlds stay survival.

creative grants instant block breaking, unlimited placement and item use, no tool wear, and no health damage. open the inventory to search or scroll all available items; click to copy a stack or shift-click to fill the hotbar. armor slots remain available. double-tap jump to toggle flight, hold jump to rise, and hold sneak to descend. flight still collides with terrain.

## copper progression

stone pickaxes mine copper ore, with vein starting heights from 8 through 79 in newly generated overworld stone. smelt the ore into copper ingots and use the usual tool patterns to make a pickaxe, axe, shovel, hoe or sword. copper tools have 190 durability and mining speed 5, between stone and iron.

iron ore and iron blocks require a copper, iron or diamond pickaxe. stone and golden pickaxes cannot harvest iron. copper cannot harvest gold, redstone, diamond or obsidian. copper is an explicit addition to the beta item set; iron tool recipes continue to use iron ingots.

## hotbar item labels

selecting an item shows its name above the hotbar for three seconds, fading during the last second. enable `show durability` under options > interface to keep the held tool or armor's remaining and maximum durability visible. the setting is saved; food, blocks and empty slots have no durability label.

armor protection appears above the right side of the hotbar, opposite the hearts. ten icons fill left to right, with a half icon for an odd protection value. the bar follows armor wear in survival and creative, and is hidden without armor.

unsupported plants and plants displaced by liquids or falling blocks use the same loot rules as mining without a tool. tall grass and ferns have a one-in-eight seed drop; dead bushes drop nothing. shears still collect grass, ferns and leaves when used directly. empty drops stay empty.

## farming and exploration expansion

new worlds add small basalt deposits and quartz ore to the existing red nether. stone pickaxes or better harvest basalt, quartz ore, and cave crystals. netherrack smelts into nether bricks; further firing produces cracked bricks. quartz crafts into blocks, pillars, and chiseled blocks. cave stone can carry sparse moss or lichen, with small fungi, dim mycena, hanging glowworms, and rare deep crystals. four crystals and glass make a brighter lamp.

wild potatoes, tomatoes, and barley provide planting material in suitable surface biomes. plant on tilled soil; water within four blocks hydrates farmland. crops need light level 9 or more, and rows grow faster than crowded squares. break mature crops to collect food and seeds, then replant. potatoes can be baked, barley makes bread, and potatoes with tomatoes make bowl-returning stew. edible items stack to one and heal immediately.

bone meal now advances crops by three stages. mulch advances one stage; fertilizer advances five. four leaves or two wheat make mulch. combine mulch with bone meal or mycena powder for fertilizer. unsuccessful uses consume nothing; creative uses preserve the item.

build a classic obsidian frame with a two-wide, three-high opening and ignite it with flint and steel. corner blocks are optional. right-click the portal to travel. destinations reuse an intact nearby frame or build a frame in a checked clear space with a small landing platform. travel uses an eight-to-one horizontal scale and saves a return link. a broken frame extinguishes once its surrounding chunks are loaded.

new block registrations are appended so existing saves retain their numeric ids. new saves record generation revision 1; saves without that trailer retain revision 0 and do not gain the new natural decorations. existing saved blocks are preserved. lighting is rebuilt on load, so enclosed caves and the nether no longer inherit daylight from unloaded chunk boundaries. crops and hives pause away from players; active chunk and block ticking has a fixed work limit.

these additions adapt the supplied refi assets to oldworld's existing systems. they are deliberate extensions to beta, with no hunger, shields, fireworks, or extra equipment tier after diamond. asset mappings and transformations are recorded in `refi-assets.json`.

wild hives appear occasionally in forests. a comb surrounded by planks makes a wooden hive. hives need daylight, light level 12, and two nearby flowers; they produce one comb after roughly four minutes of active checks and hold up to three. right-click to harvest. a smoker made from four iron ingots and coal calms bees for 45 seconds and lasts 64 uses. harvesting without smoke can cause one heart of damage through normal armor and hurt immunity. comb makes honey or wax; wax and a stick make torches. each hive can advance one crop every two minutes, and overlapping hives share the same per-crop cooldown. nearby bee sprites are cosmetic and capped for rendering.

## tin and bronze

tin veins start at y=16–63, with eight size-six attempts per column. stone pickaxes can mine tin ore; smelting it produces one tin ingot. the blast furnace also accepts tin ore. existing generated chunks are unchanged.

three copper ingots and one tin ingot in a two-by-two crafting square make four bronze ingots. bronze uses the usual five tool recipes, with 220 durability and mining speed 5.5 between copper and iron. its pickaxe can harvest iron, while gold, redstone, and diamond ores still require iron-tier tools. bronze swords deal seven damage.

the usual armor patterns make a bronze helmet, chestplate, leggings, and boots. they have 99, 144, 135, and 117 durability, between chain and iron, with beta's durability-dependent armor protection. each usable bronze piece adds ten percent resistance to direct projectiles, including skeleton arrows. a full set reduces the damage remaining after normal armor by forty percent. the bonus does not apply to melee, explosions, falls, fire, or lava. armor still wears by the original incoming damage, and a broken piece loses its bonus.

## climbing rope and buried caches

three string in a vertical recipe make eight climbing-rope sections. right-click the underside of a solid opaque block to lower up to eight sections, using one item per block in survival. right-click any attached section to extend its line. each line is limited to 32 blocks and stops at terrain, liquids, the world boundary, or an unloaded chunk. creative placement keeps its items and follows the same length limit.

hold space while touching rope to climb, release it to descend, or hold sneak to stay in place. climbing clears accumulated fall distance. breaking a section releases the sections below it; breaking the anchor releases the whole line. each removed section drops one reusable rope item.

treasure notes without an assigned destination choose a buried cache in new overworld terrain. caches lie four blocks below dry ground and contain coal, iron, torches, and bread. each cache has a one-in-64 chance of a music disc; other caches have a one-in-three chance of worn iron or bronze tools or armor. worn equipment starts with 50–85 percent of its durability spent. caches do not contain apples or golden apples. their inventories persist and never refill during generation. earlier notes retain their saved ruin destinations.
