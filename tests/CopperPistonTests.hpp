#ifndef COPPERPISTONTESTS_HPP_
#define COPPERPISTONTESTS_HPP_
#include <cxxtest/TestSuite.h>
#include <filesystem>
#include "common/core/GameClock.hpp"
#include "common/core/Registry.hpp"
#include "common/core/MaterialPalette.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/world/CopperOxidation.hpp"
#include "common/world/Dimension.hpp"
#include "common/world/BetaPiston.hpp"
#include "common/world/WorldCollision.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/lua/ServerModLoader.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/WorldController.hpp"

class CopperPistonTests : public CxxTest::TestSuite {
public:
	void setUp() override {
		static LoggerHandler logger; LoggerHandler::setInstance(logger);
		static GameClock clock; GameClock::setInstance(clock);
	}
	void testAllCopperToolsOxidizeWithoutLosingWearAndSurviveSaving() {
		struct Restore {std::filesystem::path original=std::filesystem::current_path(); ~Restore(){std::filesystem::current_path(original);}} restore;
		std::filesystem::current_path(OM_TEST_SOURCE_DIR);
		ScriptEngine scripts; Registry registry; Registry::setInstance(registry);
		registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
		registry.registerItem<Item>({},BLOCK_AIR,"air").setIsBlock(true);
		WorldController worlds(registry); ServerModLoader loader(scripts,worlds); loader.loadMods();
		for(const char *tool:{"pickaxe","axe","shovel","hoe","sword"}) {
			ItemStack stack{std::string("default:copper_")+tool,1,37};
			TS_ASSERT(!oxidizeCopperTool(stack,19));
			for(unsigned stage=1;stage<=3;++stage) {
				TS_ASSERT(oxidizeCopperTool(stack,0));
				TS_ASSERT_EQUALS(stack.item().getGroupValue("group:om_copper_stage"),stage);
				TS_ASSERT_EQUALS(stack.damage(),37); TS_ASSERT_EQUALS(stack.maxDamage(),190);
				TS_ASSERT_EQUALS(stack.item().getGroupValue("group:om_copper_tool"),1);
				sf::Packet packet; packet<<stack<<u32(123); ItemStack restored; u32 marker=0; packet>>restored>>marker;
				TS_ASSERT(restored.sameItem(stack)); TS_ASSERT_EQUALS(marker,123u);
			}
			TS_ASSERT(!oxidizeCopperTool(stack,0));
		}
		ItemStack bronze{"default:bronze_pickaxe",1,20}; TS_ASSERT(!oxidizeCopperTool(bronze,0));
		ItemStack ingot{"default:copper_ingot",8}; TS_ASSERT(!oxidizeCopperTool(ingot,0));
		const auto &piston=registry.getBlockFromStringID("default:piston");
		TS_ASSERT_EQUALS(piston.getState(0).tiles().getTextureForFace(BlockFace::Top),"refi_piston_top.png");
		TS_ASSERT_EQUALS(piston.getState(0).tiles().getTextureForFace(BlockFace::West),"refi_piston_side.png");
	}
	void testOxidationUsesGameTimeAndUpdatesCarriedStoredAndDroppedTools() {
		Registry registry; Registry::setInstance(registry);
		registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
		registry.registerItem<Item>({},BLOCK_AIR,"air");
		const char *names[]={"copper_pickaxe","exposed_copper_pickaxe","weathered_copper_pickaxe","oxidized_copper_pickaxe"};
		for(unsigned stage=0;stage<4;++stage) {
			auto &item=registry.registerItem<Item>({},std::string("default:")+names[stage],names[stage]);
			item.addGroup("group:om_copper_tool",1); item.addGroup("group:om_copper_stage",u16(stage)); item.addGroup("group:om_max_damage",190);
		}
		PlayerList players; Dimension dimension{0,"test:world","world"}; ServerWorld world(players,dimension,42);
		auto &player=players.addPlayer("tester",true); ClientInfo client{1,{}}; player.setClient(&client); player.setReady(true);
		player.inventory().setStack(0,0,"default:copper_pickaxe",1); player.inventory().getStackRef(0,0).setDamage(37);
		auto &chunk=world.getOrCreateChunk(0,0,0); chunk.setInitialized(true);
		auto *data=chunk.addBlockData(8,8,8,9,3); data->inventory.setStack(0,0,"default:copper_pickaxe",1);
		auto entity=world.scene().registry().create();
		world.scene().registry().emplace<ItemStack>(entity,"default:copper_pickaxe",1,37);
		world.scene().registry().emplace<DrawableDef>(entity).addInventoryCube();
		for(unsigned tick=0;tick<1199;++tick) world.updateCopperOxidation();
		TS_ASSERT_EQUALS(player.inventory().getStack(0,0).stringID(),"default:copper_pickaxe");
		for(unsigned tick=0;tick<1200*600;++tick) world.updateCopperOxidation();
		TS_ASSERT_EQUALS(player.inventory().getStack(0,0).stringID(),"default:oxidized_copper_pickaxe");
		TS_ASSERT_EQUALS(player.inventory().getStack(0,0).damage(),37);
		TS_ASSERT_EQUALS(data->inventory.getStack(0,0).stringID(),"default:oxidized_copper_pickaxe");
		TS_ASSERT_EQUALS(world.scene().registry().get<ItemStack>(entity).stringID(),"default:oxidized_copper_pickaxe");
		TS_ASSERT_EQUALS(world.scene().registry().get<DrawableDef>(entity).getInventoryCubeDef().blockID,"default:oxidized_copper_pickaxe");
		TS_ASSERT(chunk.hasBeenModified());
	}
	void testPatinaCoverageIncreasesAndWoodHandlesStayBrown() {
		using namespace materialPalette;
		unsigned exposed=0,weathered=0,oxidized=0;
		for(int x=0;x<16;++x) for(int y=0;y<16;++y) {
			auto first=color(Kind::ExposedCopper,130,121,38,x,y);
			auto middle=color(Kind::WeatheredCopper,130,121,38,x,y);
			auto last=color(Kind::OxidizedCopper,130,121,38,x,y);
			exposed+=first[1]>first[0]; weathered+=middle[1]>middle[0]; oxidized+=last[1]>last[0];
		}
		TS_ASSERT(exposed<weathered); TS_ASSERT(weathered<oxidized); TS_ASSERT_EQUALS(oxidized,256);
		for(auto kind:{Kind::ExposedCopper,Kind::WeatheredCopper,Kind::OxidizedCopper}) {
			auto wood=color(kind,104,87,42,8,8); TS_ASSERT_EQUALS(wood[0],104); TS_ASSERT_EQUALS(wood[1],87);
		}
	}
	void testPistonShaftJoinsItsBaseInEveryOrientation() {
		Registry registry; Registry::setInstance(registry);
		auto &head=registry.registerBlock<ServerBlock>("default:piston_head"); head.addGroup("group:om_piston_head",1);
		head.setRotatable(true); head.param().allocateBits(BlockParam::Rotation,5);
		head.getState(0).boundingBox({0,0,0.75f,1,1,0.25f});
		BlockGeometry::initOrientation();
		for(u16 rotation=0;rotation<24;rotation+=4) {
			glm::vec3 point=BlockGeometry::orientMatrices[rotation]*(glm::vec3{0.5f,0.5f,-0.125f}-glm::vec3{0.5f})+glm::vec3{0.5f};
			DoubleBox contact{point.x-0.04,point.y-0.04,point.z-0.04,0.08,0.08,0.08};
			u16 param=head.param().setParam(BlockParam::Rotation,0,rotation);
			TS_ASSERT(collision::intersectsBlock(head.getState(0),param,0,0,0,contact));
		}
	}
};
#endif
