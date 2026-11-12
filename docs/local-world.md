# local world runtime

the game supports one local player. choose play on the title screen to create or load a world, or run `build/opencraft --world MyWorld`. `-s` opens world selection. world names use letters, digits, and underscores.

rendering and gameplay simulation run in separate threads in the same process. `Network::LocalSession` carries serialized messages through two mutex-protected fifo queues. the existing packet serializer is retained for saves and local messages; the sfml socket module and packet compression transport are no longer compiled. there are no listening ports, remote connections, host/join screens, dedicated executable, peer avatars, or address settings.

the local endpoint accepts one attachment. each message is copied into bytes and reconstructed with a fresh read cursor. closing a world requests simulation shutdown, saves the world and health, queues the completion message, then closes the queues. already queued messages remain readable, so the save screen waits for actual completion before returning to the title screen. opening another world creates fresh queues.

old saves retain their chunk and component layouts. the local player uses the saved matching name when available, otherwise a saved `singleplayer` record or a deterministic legacy record. migration copies the selected record and preserves the originals. health, inventory, position, and mobs survive local save/reload.

survival play has no flight, noclip, sprint toggle, creative inventory, free item cloning, starter tools, or command console. walking, jumping, sneaking and swimming follow the [beta survival rules](beta-survival.md). equipment wears with use; inventory transfers, drops and saves preserve its damage. block interaction reach is four blocks. correct tool tiers control drops; items appear in the world and partial pickups preserve the remainder. crafting includes torches and shears, and wood/planks/sticks can fuel furnaces.

falling, drowning, lava, burning, cactus and suffocation damage run in the simulation with beta damage immunity. death drops inventory into the world and respawns the player. old saves retain their inventory; removed creative controls cannot be re-enabled through old configuration.
