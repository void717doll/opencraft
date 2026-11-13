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

## remaining bottlenecks

8. **saved terrain is loaded eagerly and server chunks stay resident.** `WorldSaveBasicBackend::load` reads every stored chunk and initializes its lighting; `ServerWorld::getOrCreateChunk` retains chunks after the player moves away. larger explored worlds therefore increase startup work and memory use. the next fix needs on-demand database reads and save-before-eviction, with tests that prevent lost blocks, inventories, and entities. this batch does not change world residency.

9. **small block changes still send a full chunk snapshot.** `ServerChunk::process` and `sendData` send roughly 40 kib of block/light cells after a chunk changes. repeated piston or lamp activity can still create substantial copying and remeshing. the optimized writer reduces serialization cost, and identical snapshots now skip client remeshing. pending digs stay removed until acknowledged. a proper follow-up still needs coalesced block and light changes with an initial full snapshot. simply dropping snapshots would leave client lighting stale.

10. **detectors and piston pushes scan entity lists.** watched plates and detectors in `WorldRailPower.cpp` check all relevant items, mobs, and carts; piston pushes in `WorldBetaDevices.cpp` check entities against affected blocks. dense farms can still pay for distant entities. a shared spatial lookup is the next step when a dense-farm profile confirms this cost.

11. **entity drawing has no distance or view-volume rejection.** `RenderingController::draw` submits every drawable entity. dense scenes can remain expensive even when terrain meshing is faster. a follow-up needs bounds-based visibility checks that preserve large models and objects crossing the screen edge.

12. **a single generated chunk can exceed the scheduling budget.** `processSendRequests` checks a three-millisecond deadline between chunks; `generateChunk` runs terrain, feature placement, lighting initialization, and physics seeding synchronously. one expensive chunk can delay a simulation tick. moving work off-thread requires isolated generation inputs and ordered lighting and physics integration; raising the budget would increase stalls.

## checks run

- linux release build of `opencraft` passed.
- all 239 regression tests passed, including piston sleep, indirect power, blocked retries, idle entity packets, moving and stationary carts, light transitions, exact packet bytes, and saved metadata order.
- standalone mesh comparisons matched every vertex byte across 36 layout and lighting combinations.
- headless vertex-buffer checks passed allocation, reuse, growth, and initialized data.
- a fresh isolated native world loaded and rendered, then saved to sqlite and exited with code zero. the screenshot showed terrain and the hud without a visible meshing regression.
- final paired piston and chunk-stream benchmarks ran after compilation and the native game had stopped.

## validation limits

cpu microbenchmarks and behavioral regressions establish the changes above, not a measured whole-game fps gain. linux is the validation platform. macos and windows runtime performance, long exploration sessions, dense farms, and sustained piston-clock frame times require separate runs. no new dependency, save format, packet format, graphics option, or worker-count increase was introduced.
