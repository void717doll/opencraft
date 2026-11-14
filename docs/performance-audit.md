# performance audit

reviewed for one offline player, a render distance of eight chunks, and a 20-tick-per-second simulation. four agents reviewed redstone and pistons, chunk meshing, entity synchronization, and streaming and saves. lighting, audio playback, world scheduling, and the connecting calls were checked alongside them.

measurements below are synthetic cpu workloads, not whole-game fps results. native gameplay checks cover loading and drawing; they do not establish smooth play on every computer.

## fixed

1. **idle pistons repeatedly searched for power.** each watched piston queried its surroundings every tick, including indirect inputs through solid blocks. settled pistons now sleep until a relevant block changes; blocked pushes keep retrying. the regression includes indirect power three blocks away. the 64-piston test over 10,000 ticks dropped from 28,800,000 chunk lookups to zero after settling. see `WorldBetaDevices.cpp`, `WorldRailPower.cpp`, `WorldRedstone.cpp`, and `PistonWorkTests.hpp`.

2. **hidden blocks built geometry before being rejected.** a chunk rebuild constructed solid cubes and checked special groups even for blocks surrounded by opaque neighbors. those cubes now stop before that work. all emitted vertex bytes matched in 36 layout and lighting combinations. 100 rebuilds took 976 to 243 ms for buried solid chunks, 274 to 147 ms for half-solid chunks, and 2,533 to 1,914 ms for mixed air and stone. a sparse single-block chunk was effectively unchanged. see `BlockMesher.cpp`.

3. **unchanged entities sent hitboxes every tick.** the server also used runtime type lookup to serialize every component before discovering there was nothing to send. direct dirty checks now send changed components, and cached hitboxes suppress repeats. a real local-session test checks six initial packets, zero packets over 100 idle ticks, two packets for movement and resizing, and six on reconnect. unchanged mob headings and stationary cart positions also keep their update flags clear. see `NetworkController.cpp`, `NetworkComponent.hpp`, and `EntityNetworkPerformanceTests.hpp`.

4. **chunk packets performed thousands of tiny appends and metadata searches.** each 8,192-cell snapshot made 16,384 scalar packet writes and 8,192 metadata lookups, even when the chunk had no metadata. the writer now fills one contiguous block/light buffer and visits existing metadata directly. the packet format stays unchanged; regression checks compare every byte and verify metadata at negative world coordinates. three paired runs averaged 1,615 to 264 ms for 5,000 sends, about 84% less serialization time; both writers sent exactly 204,870,000 bytes. see `ServerChunk.cpp` and `ChunkStreamingPerformanceTests.hpp`.

5. **equivalent block replacements restarted lighting.** replacing one ordinary opaque block with another removed and rebuilt light despite having identical lighting behavior. replacements now compare opacity, light emission, and sunlight transmission first. tests retain relighting when a wall opens, when emission changes, or when water changes sunlight attenuation. see `Chunk.cpp` and `ExpansionEngineTests.hpp`.

6. **new mesh buffers uploaded bytes before receiving the real mesh.** empty dynamic vertex buffers now use bgfx's count-based allocation instead of allocating an uninitialized cpu buffer and uploading it. standalone headless checks cover allocation, reuse, growth, and buffers initialized with actual vertices. see `VertexBuffer.cpp`.

7. **saving scanned every cell twice for sparse metadata.** saved chunks now count and visit their metadata map directly, removing 16,384 metadata lookups per chunk. the save schema remains unchanged. existing world-save round trips exercise the reader and writer. see `WorldSaveBasicBackend.cpp`.

## follow-up fixes

8. **saved terrain now loads on demand.** indexed database reads restore chunks as they enter the streaming area. distant chunks save modified blocks and container metadata before their pointers, physics checks, and terrain caches are released. entities remain in the saved scene and resume when their chunks load. legacy saves are converted once with their original backup retained.

9. **small changes now send block and light deltas.** the first subscription still receives a complete snapshot. later updates contain changed cell indices, block words, and light bytes; large changes fall back to a snapshot. a single changed cell uses 23 bytes instead of roughly 40 kib. ordered delivery preserves pending-dig handling and metadata updates.

10. **detectors use nearby contacts.** each device update builds spatial buckets once and checks contacts near each plate or detector. unloaded distant mobs pause their simulation; live mobs also stop simulating outside the player's active range.

11. **entity drawing rejects distant and offscreen bounds.** conservative model bounds retain objects crossing the screen edge. terrain generation yields between rows and feature passes. completed meshes upload in bounded batches into a replacement buffer, leaving the previous mesh visible until the replacement is ready. ordinary uploads use 64 kib per update; direct block edits take priority and can use 256 kib within the same two-millisecond budget. predicted digs remove collision immediately and wait for authoritative light changes instead of rebuilding light twice. vertex-buffer upload offsets now convert bytes to bgfx vertex indices.

12. **dimension entity traffic is isolated.** spawns, changed components, hitboxes, damage, and despawns go only to players in that dimension. entering another dimension sends its complete entity state after the client clears its old scene.

13. **full item stacks skip merge searches.** partial stacks retain merging, gravity, pickup delay, and expiry. the audit measured 30.34 ms of server cpu per tick for 1,024 overlapping full stacks before the fix and 4.55 ms afterward. measurements use process cpu time over 200 ticks, not whole-game frame rates.

14. **the local message queue is bounded.** replaceable entity state is coalesced between reliable events. inventory and lifecycle events retain their order. a slow consumer blocks its producer at 4,096 messages or 16 mib, and closing the session wakes blocked producers. disconnect also cancels blocked world updates while retaining the final save acknowledgement.

15. **leaf checks wait for missing chunks.** blocked leaves park until the missing chunk is seeded, rather than retrying every tick. eviction removes abandoned checks. shared world-clock values now use atomics.

## earlier checks

- linux release build of `opencraft` passed.
- all 239 regression tests passed, including piston sleep, indirect power, blocked retries, idle entity packets, moving and stationary carts, light transitions, exact packet bytes, and saved metadata order.
- standalone mesh comparisons matched every vertex byte across 36 layout and lighting combinations.
- headless vertex-buffer checks passed allocation, reuse, growth, and initialized data.
- a fresh isolated native world loaded and rendered, then saved to sqlite and exited with code zero. the screenshot showed terrain and the hud without a visible meshing regression.
- final paired piston and chunk-stream benchmarks ran after compilation and the native game had stopped.

## follow-up checks

- linux release builds of `opencraft` and `opencraft_tests` passed; all 265 regression cases passed.
- new cases cover dimension isolation, ordered state coalescing, disconnect with a full queue, single-cell deltas, initial snapshots on resubscription, lazy loading, eviction in all three axes and container metadata, full item stacks, leaf wake-up, and staged terrain generation against three snapshots from the previous commit.
- an isolated native creative world accepted rapid breaking through three stone blocks, displayed the resulting tunnel, saved through the quit menu, and reopened with those blocks still removed. an independent reader checked the saved cells. the second automated desktop cycle was interrupted after reopening.
- the 1,024-full-stack workload fell from 30.34 to 4.55 ms of server cpu per tick. 256 leaves beside unloaded chunks stopped retrying and left zero active queue entries after 2,000 ticks.

## validation limits

cpu microbenchmarks and behavioral regressions establish the changes above, not a measured whole-game fps gain. linux is the validation platform. macos and windows runtime performance, long exploration sessions, dense farms, and sustained piston-clock frame times require separate runs. no new dependency, save format, graphics option, or worker-count increase was introduced. local chunk-delta messages are new; complete snapshots and persisted component ids retain their existing formats.

base terrain calculations, individual feature passes, sqlite writes, and buffer allocations remain synchronous. deadlines are checked between stages; individual operations can still exceed them on slow hardware.
