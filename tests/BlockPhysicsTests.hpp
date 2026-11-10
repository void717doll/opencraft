#ifndef BLOCKPHYSICSTESTS_HPP_
#define BLOCKPHYSICSTESTS_HPP_

#include <cxxtest/TestSuite.h>
#include "common/core/Registry.hpp"
#include "common/scene/component/FallingBlockComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/world/BlockPhysics.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerWorld.hpp"

class BlockPhysicsTests : public CxxTest::TestSuite {
	struct Fixture {
		Registry definitions;
		PlayerList players;
		Dimension dimension{0, "test:physics", "physics"};
		std::unique_ptr<ServerWorld> world;
		std::unique_ptr<BlockPhysics> physics;
		u16 stone, water, waterFlow, lava, lavaFlow, sand, gravel, flora;
		std::size_t initialChunkCount = 0;

		Fixture() {
			Registry::setInstance(definitions);
			definitions.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
			definitions.registerItem<Item>({}, BLOCK_AIR, "air");
			definitions.registerItem<Item>({}, "default:stone", "stone");
			stone = definitions.registerBlock<ServerBlock>("default:stone").id();
			water = liquid("default:water", false);
			waterFlow = liquid("default:water_flowing", true);
			lava = liquid("default:lava", false);
			lavaFlow = liquid("default:lava_flowing", true);
			sand = definitions.registerBlock<ServerBlock>("default:sand").id();
			gravel = definitions.registerBlock<ServerBlock>("default:gravel").id();
			definitions.registerBlock<ServerBlock>("default:cobblestone");
			definitions.registerBlock<ServerBlock>("default:obsidian");
			auto &plant = definitions.registerBlock<ServerBlock>("test:flower");
			flora = plant.id();
			plant.getState(0).isCollidable(false);
			plant.addGroup("group:attached_node", 3);
			world = std::make_unique<ServerWorld>(players, dimension, 42);
			physics = std::make_unique<BlockPhysics>(*world);
			addChunk(0, true);
			initialChunkCount = world->chunks().size();
		}

		u16 liquid(const char *name, bool flowing) {
			auto &block = definitions.registerBlock<ServerBlock>(name);
			block.getState(0).isCollidable(false);
			block.getState(0).drawType(BlockDrawType::Liquid);
			if (flowing) block.param().allocateBits(BlockParam::Custom, 4);
			return block.id();
		}

		ServerChunk &addChunk(int x, bool walls = false) {
			auto &chunk = world->getOrCreateChunk(x, 0, 0);
			for (int z = 0; z < CHUNK_HEIGHT; ++z)
				for (int y = 0; y < CHUNK_DEPTH; ++y)
					for (int bx = 0; bx < CHUNK_WIDTH; ++bx)
						if (!z || (walls && (!bx || bx == 15 || !y || y == 15 || z == 31))) chunk.setBlockRaw(bx, y, z, stone);
			chunk.setInitialized(true);
			return chunk;
		}

		void run(int ticks = 100) {
			for (int i = 0; i < ticks; ++i) physics->tick(8192);
		}

		int level(int x, int y, int z) const {
			const auto &block = definitions.getBlock(world->getBlock(x, y, z));
			return block.param().getParam(BlockParam::Custom, world->getData(x, y, z)) & 7;
		}
	};

	public:
		void testWaterPropagatesAndDrainsAfterSourceRemoval() {
			Fixture f;
			f.world->setBlock(8, 8, 2, f.water);
			f.physics->enqueueNeighbors({8, 8, 2});
			f.run();
			TS_ASSERT_EQUALS(f.world->getBlock(8, 8, 1), f.waterFlow);
			TS_ASSERT_EQUALS(f.world->getBlock(10, 8, 1), f.waterFlow);
			TS_ASSERT(f.level(10, 8, 1) < f.level(8, 8, 1));
			f.world->setBlock(8, 8, 2, 0);
			f.physics->enqueueNeighbors({8, 8, 2});
			f.run(200);
			for (int z = 1; z < 4; ++z)
				for (int y = 1; y < 15; ++y)
					for (int x = 1; x < 15; ++x) TS_ASSERT_EQUALS(f.world->getBlock(x, y, z), 0);
		}

		void testWaterRenewsFromTwoSources() {
			Fixture f;
			f.world->setBlock(7, 8, 1, f.water);
			f.world->setBlock(9, 8, 1, f.water);
			f.physics->enqueue({8, 8, 1});
			TS_ASSERT_EQUALS(f.physics->tick(1), 1);
			TS_ASSERT_EQUALS(f.world->getBlock(8, 8, 1), f.water);
		}

		void testLavaViscosityAndWaterInteraction() {
			Fixture f;
			f.world->setBlock(8, 8, 2, f.lavaFlow);
			f.world->setData(8, 8, 2, 1);
			f.physics->enqueue({8, 8, 1});
			f.physics->tick(1);
			TS_ASSERT_EQUALS(f.world->getBlock(8, 8, 1), f.lavaFlow);
			TS_ASSERT_EQUALS(f.level(8, 8, 1), 1);
			f.world->setBlock(4, 4, 1, f.lava);
			f.world->setBlock(5, 4, 1, f.water);
			f.physics->enqueue({4, 4, 1});
			f.run(5);
			TS_ASSERT_EQUALS(f.world->getBlock(4, 4, 1), f.definitions.getBlockFromStringID("default:obsidian").id());
		}

		void testExhaustedFlowDoesNotDestroyNearbyFlowers() {
			Fixture f;
			f.world->setBlock(8, 8, 1, f.flora);
			f.world->setData(8, 8, 1, 13);
			f.world->setBlock(7, 8, 1, f.waterFlow);
			f.world->setData(7, 8, 1, 0);
			f.physics->enqueue({8, 8, 1});
			f.physics->tick(1);
			TS_ASSERT_EQUALS(f.world->getBlock(8, 8, 1), f.flora);
			TS_ASSERT_EQUALS(f.world->getData(8, 8, 1), 13);
		}

		void testLoadedChunkBoundaryWakesFluidWithoutGeneratingTerrain() {
			Fixture f;
			f.world->setBlock(15, 8, 1, f.water);
			f.physics->enqueueNeighbors({15, 8, 1});
			f.run(20);
			TS_ASSERT_EQUALS(f.world->chunks().size(), f.initialChunkCount);
			TS_ASSERT(f.world->getChunk(1, 0, 0));
			TS_ASSERT(!f.world->getChunk(1, 0, 0)->isInitialized());
			auto &neighbor = f.addChunk(1);
			f.physics->seedChunk(neighbor);
			f.run(30);
			std::size_t initialized = 0;
			for (const auto &entry : f.world->chunks()) if (entry.second->isInitialized()) ++initialized;
			TS_ASSERT_EQUALS(initialized, 2);
			TS_ASSERT_EQUALS(f.world->getBlock(16, 8, 1), f.waterFlow);
		}

		void testQueueDeduplicationAndTickBudget() {
			Fixture f;
			for (int x = 1; x <= 10; ++x) {
				f.physics->enqueue({x, 8, 5});
				f.physics->enqueue({x, 8, 5});
			}
			f.physics->enqueue({100, 100, 100});
			TS_ASSERT_EQUALS(f.physics->pendingCount(), 10);
			TS_ASSERT_EQUALS(f.physics->tick(3), 3);
			TS_ASSERT_EQUALS(f.physics->pendingCount(), 7);
			TS_ASSERT_EQUALS(f.physics->tick(0), 0);
			TS_ASSERT_EQUALS(f.physics->pendingCount(), 7);
			TS_ASSERT_EQUALS(f.world->chunks().size(), f.initialChunkCount);
		}

		void testEnclosedSourcesAndSolidBoundariesDoNotFillQueue() {
			Fixture f;
			auto &chunk = *static_cast<ServerChunk *>(f.world->getChunk(0, 0, 0));
			for (int z = 1; z < 16; ++z)
				for (int y = 1; y < 15; ++y)
					for (int x = 1; x < 15; ++x) chunk.setBlockRaw(x, y, z, f.water);
			f.physics->seedChunk(chunk);
			TS_ASSERT_EQUALS(f.physics->pendingCount(), 0);
			chunk.setBlockRaw(8, 8, 0, 0);
			f.physics->seedChunk(chunk);
			TS_ASSERT_EQUALS(f.physics->pendingCount(), 2);
		}

		void testFallingNodeWaitsForLoadedSupport() {
			Fixture f;
			f.world->setBlock(8, 8, 0, f.gravel);
			f.physics->enqueue({8, 8, 0});
			f.run(2);
			TS_ASSERT_EQUALS(f.world->getBlock(8, 8, 0), f.gravel);
			TS_ASSERT_EQUALS(f.world->scene().registry().view<FallingBlockComponent>().size(), 0);
			TS_ASSERT_EQUALS(f.world->chunks().size(), f.initialChunkCount);
			auto &below = f.world->getOrCreateChunk(0, 0, -1);
			for (int x = 0; x < 16; ++x)
				for (int y = 0; y < 16; ++y) below.setBlockRaw(x, y, 0, f.stone);
			below.setInitialized(true);
			f.physics->seedChunk(below);
			f.run(100);
			TS_ASSERT_EQUALS(f.world->getBlock(8, 8, 0), 0);
			TS_ASSERT_EQUALS(f.world->getBlock(8, 8, -31), f.gravel);
		}

		void testFallingSandSettlesWithParametersAndMetadata() {
			Fixture f;
			f.world->setBlock(8, 8, 6, f.sand);
			f.world->setData(8, 8, 6, 0xa53d);
			auto *data = f.world->addBlockData(8, 8, 6, 2, 1);
			data->meta.setString("label", "saved sand");
			data->inventory.setStack(0, 0, "default:stone", 3);
			f.physics->enqueue({8, 8, 6});
			f.physics->tick(1);
			TS_ASSERT_EQUALS(f.world->getBlock(8, 8, 6), 0);
			TS_ASSERT_EQUALS(f.world->scene().registry().view<FallingBlockComponent>().size(), 1);
			f.world->scene().registry().view<FallingBlockComponent, PositionComponent>().each([&](auto, auto &falling, auto &position) {
				TS_ASSERT(position.z < 6 && position.z > 5);
				sf::Packet save;
				save << falling;
				FallingBlockComponent restored;
				save >> restored;
				TS_ASSERT(save);
				TS_ASSERT_EQUALS(restored.block, f.sand);
				TS_ASSERT_EQUALS(restored.data, 0xa53d);
				TS_ASSERT(restored.blockData);
				if (restored.blockData) TS_ASSERT_EQUALS(restored.blockData->meta.get<std::string>("label"), "saved sand");
				falling = restored;
			});
			f.run(60);
			TS_ASSERT_EQUALS(f.world->scene().registry().view<FallingBlockComponent>().size(), 0);
			TS_ASSERT_EQUALS(f.world->getBlock(8, 8, 1), f.sand);
			TS_ASSERT_EQUALS(f.world->getData(8, 8, 1), 0xa53d);
			auto *settled = f.world->getBlockData(8, 8, 1);
			TS_ASSERT(settled);
			if (settled) {
				TS_ASSERT_EQUALS(settled->meta.get<std::string>("label"), "saved sand");
				TS_ASSERT_EQUALS(settled->inventory.getStack(0, 0).amount(), 3);
			}
		}

		void testStackedPlantsDropEveryUnsupportedStalkExactlyOnce() {
			for (const std::string id : {"default:reeds", "default:cactus"}) {
				for (bool middle : {false,true}) {
					Fixture f;
					f.definitions.registerEntity("default:item_drop");
					auto &plant = f.definitions.registerBlock<ServerBlock>(id);
					plant.addGroup("group:attached_node",3);
					f.definitions.registerItem<Item>({},id,id);
					u16 ground = id == "default:cactus" ? f.sand : f.definitions.registerBlock<ServerBlock>("default:dirt").id();
					f.world->setBlock(4,4,1,ground);
					if (id == "default:reeds") f.world->setBlock(5,4,1,f.water);
					for (int z=2;z<=4;++z) f.world->setBlock(4,4,z,plant.id());
					int removed = middle ? 3 : 1;
					f.world->setBlock(4,4,removed,0);
					f.physics->enqueueNeighbors({4,4,removed}); f.run();
					for (int z=removed+1;z<=4;++z) TS_ASSERT_EQUALS(f.world->getBlock(4,4,z),0);
					if (middle) TS_ASSERT_EQUALS(f.world->getBlock(4,4,2),plant.id());
					unsigned drops = 0;
					f.world->scene().registry().view<ItemStack>().each([&](auto,const auto &stack) {
						TS_ASSERT_EQUALS(stack.stringID(),id); drops += stack.amount();
					});
					TS_ASSERT_EQUALS(drops,middle ? 1u : 3u);
				}
			}
		}

		void testPlantSupportUpdatesCrossVerticalChunksWithoutLoadingTerrain() {
			Fixture f;
			f.definitions.registerEntity("default:item_drop");
			auto &plant = f.definitions.registerBlock<ServerBlock>("default:reeds");
			plant.addGroup("group:attached_node",3);
			f.definitions.registerItem<Item>({},"default:reeds","reeds");
			auto &upper = f.world->getOrCreateChunk(0,0,1);
			upper.setInitialized(true);
			for (int z=31;z<=33;++z) f.world->setBlock(4,4,z,plant.id());
			auto chunks = f.world->chunks().size();
			f.world->setBlock(4,4,31,0);
			f.physics->enqueueNeighbors({4,4,31}); f.run();
			TS_ASSERT_EQUALS(f.world->getBlock(4,4,32),0);
			TS_ASSERT_EQUALS(f.world->getBlock(4,4,33),0);
			TS_ASSERT_EQUALS(f.world->chunks().size(),chunks);
			unsigned drops = 0;
			f.world->scene().registry().view<ItemStack>().each([&](auto,const auto &stack) { drops += stack.amount(); });
			TS_ASSERT_EQUALS(drops,2u);
		}

		void testReedWaterLossAndCactusSideObstructionDropPlants() {
			for (const std::string id : {"default:reeds", "default:cactus"}) {
				Fixture f;
				f.definitions.registerEntity("default:item_drop");
				auto &plant = f.definitions.registerBlock<ServerBlock>(id);
				plant.addGroup("group:attached_node",3);
				f.definitions.registerItem<Item>({},id,id);
				u16 ground = id == "default:cactus" ? f.sand : f.definitions.registerBlock<ServerBlock>("default:dirt").id();
				f.world->setBlock(4,4,1,ground);
				if (id == "default:reeds") f.world->setBlock(5,4,1,f.water);
				for (int z=2;z<=4;++z) f.world->setBlock(4,4,z,plant.id());
				Vector3i changed = id == "default:reeds" ? Vector3i{5,4,1} : Vector3i{5,4,2};
				f.world->setBlock(changed.x,changed.y,changed.z,id == "default:reeds" ? 0 : f.stone);
				f.physics->enqueueNeighbors(changed); f.run();
				for (int z=2;z<=4;++z) TS_ASSERT_EQUALS(f.world->getBlock(4,4,z),0);
				unsigned drops = 0;
				f.world->scene().registry().view<ItemStack>().each([&](auto,const auto &stack) { drops += stack.amount(); });
				TS_ASSERT_EQUALS(drops,3u);
			}
		}

		void testUnsupportedPlantsUseLootInsteadOfPlantItems() {
			Fixture f;
			f.definitions.registerEntity("default:item_drop");
			f.definitions.registerItem<Item>({}, "default:seeds", "seeds");
			auto &shears = f.definitions.registerItem<Item>({}, "default:shears", "shears");
			for (const std::string id : {"default:tallgrass", "default:fern", "default:deadbush"}) {
				auto &plant = f.definitions.registerBlock<ServerBlock>(id);
				plant.addGroup("group:attached_node", 3);
				plant.getState(0).isCollidable(false);
				f.definitions.registerItem<Item>({}, id, id);
				if (id != "default:deadbush") {
					auto sheared = plant.getState(0).getItemDrop(&shears);
					TS_ASSERT_EQUALS(sheared.stringID(), id); TS_ASSERT_EQUALS(sheared.amount(), 1);
				}
				for (int trial = 0; trial < 80; ++trial) {
					f.world->setBlock(4, 4, 2, f.stone);
					f.world->setBlock(4, 4, 3, plant.id());
					f.world->setBlock(4, 4, 2, 0);
					f.physics->enqueueNeighbors({4, 4, 2}); f.run(2);
					TS_ASSERT_EQUALS(f.world->getBlock(4, 4, 3), 0);
				}
			}
			unsigned seeds = 0;
			f.world->scene().registry().view<ItemStack>().each([&](auto, auto &stack) {
				TS_ASSERT_EQUALS(stack.stringID(), "default:seeds");
				TS_ASSERT_EQUALS(stack.amount(), 1); ++seeds;
			});
			TS_ASSERT(seeds > 0); TS_ASSERT(seeds < 160);
			ScriptEngine scripts; scripts.init();
			scripts.lua()["test_state"] = &f.definitions.getBlockFromStringID("default:fern").getState(0);
			scripts.lua()["test_shears"] = &shears;
			auto result = scripts.lua().safe_script("assert(test_state:get_item_drop(test_shears):item():string_id() == 'default:fern'); test_state:get_item_drop()", sol::script_pass_on_error);
			TS_ASSERT(result.valid());
			f.definitions.registerItem<Item>({}, "test:flower", "flower");
			auto &state = f.definitions.registerBlock<ServerBlock>("test:custom_drop").getState(0);
			state.itemDrop("default:seeds"); state.itemDropAmount(3);
			TS_ASSERT_EQUALS(state.getItemDrop().amount(), 3);
			TS_ASSERT_EQUALS(state.getItemDrop().stringID(), "default:seeds");
			state.itemDrop(""); TS_ASSERT_EQUALS(state.getItemDrop().amount(), 0);
			state.itemDrop("test:flower"); state.itemDropAmount(0);
			TS_ASSERT_EQUALS(state.getItemDrop().amount(), 0);
		}

		void testUnsupportedGravelAndAttachedFlowers() {
			Fixture f;
			f.world->setBlock(8, 8, 4, f.gravel);
			f.physics->enqueue({8, 8, 4});
			f.run(60);
			TS_ASSERT_EQUALS(f.world->getBlock(8, 8, 1), f.gravel);
			f.world->setBlock(4, 4, 2, f.stone);
			f.world->setBlock(4, 4, 3, f.flora);
			f.physics->enqueue({4, 4, 3});
			f.run(1);
			TS_ASSERT_EQUALS(f.world->getBlock(4, 4, 3), f.flora);
			f.world->setBlock(4, 4, 2, 0);
			f.physics->enqueueNeighbors({4, 4, 2});
			f.run(2);
			TS_ASSERT_EQUALS(f.world->getBlock(4, 4, 3), 0);
		}
};

#endif // BLOCKPHYSICSTESTS_HPP_
