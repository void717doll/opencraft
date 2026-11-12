#ifndef BRONZETESTS_HPP_
#define BRONZETESTS_HPP_

#include <cxxtest/TestSuite.h>
#include <filesystem>
#include "common/core/GameClock.hpp"
#include "common/core/Registry.hpp"
#include "common/world/BetaMining.hpp"
#include "common/world/Dimension.hpp"
#include "common/inventory/Recipe.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/lua/ServerModLoader.hpp"
#include "server/world/OreTerrain.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/WorldController.hpp"

class BronzeTests : public CxxTest::TestSuite {
public:
	void setUp() override {
		static LoggerHandler logger; LoggerHandler::setInstance(logger);
		static GameClock clock; GameClock::setInstance(clock);
	}

	void testTinSmeltingBronzeAlloyToolsAndArmorRecipes() {
		struct Restore {std::filesystem::path original=std::filesystem::current_path(); ~Restore(){std::filesystem::current_path(original);}} restore;
		std::filesystem::current_path(OM_TEST_SOURCE_DIR);
		ScriptEngine scripts; Registry registry; Registry::setInstance(registry);
		registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
		registry.registerItem<Item>({},BLOCK_AIR,"air").setIsBlock(true);
		WorldController worlds(registry); ServerModLoader loader(scripts,worlds);
		loader.loadMods(); scripts.luaCore().setRegistry(&registry);
		Inventory furnace(1,1); furnace.setStack(0,0,"default:tin_ore",1);
		const Recipe *smelt=registry.getRecipe(furnace);
		TS_ASSERT(smelt); if(smelt) TS_ASSERT_EQUALS(smelt->result().stringID(),"default:tin_ingot");
		Inventory alloy(3,3);
		alloy.setStack(0,0,"default:copper_ingot",1); alloy.setStack(1,0,"default:copper_ingot",1);
		alloy.setStack(0,1,"default:copper_ingot",1); alloy.setStack(1,1,"default:tin_ingot",1);
		const Recipe *recipe=registry.getRecipe(alloy);
		TS_ASSERT(recipe);
		if(recipe) { TS_ASSERT_EQUALS(recipe->result().stringID(),"default:bronze_ingot"); TS_ASSERT_EQUALS(recipe->result().amount(),4); }
		alloy.clearStack(1,1); TS_ASSERT(!registry.getRecipe(alloy));
		for(const char *tool:{"pickaxe","axe","shovel","hoe","sword"}) {
			const auto &bronze=registry.getItemFromStringID(std::string("default:bronze_")+tool);
			TS_ASSERT_EQUALS(bronze.maxStackSize(),1);
			TS_ASSERT_EQUALS(bronze.getGroupValue("group:om_max_damage"),220);
			if(std::string(tool)!="sword") TS_ASSERT_DELTA(bronze.miningSpeed(),5.5f,0.001f);
		}
		const auto &pick=registry.getItemFromStringID("default:bronze_pickaxe");
		TS_ASSERT(retro::canHarvest(registry.getBlockFromStringID("default:iron_ore").getState(0),pick));
		TS_ASSERT(!retro::canHarvest(registry.getBlockFromStringID("default:diamond_ore").getState(0),pick));
		TS_ASSERT(!retro::canHarvest(registry.getBlockFromStringID("default:obsidian").getState(0),pick));
		TS_ASSERT(!retro::canHarvest(registry.getBlockFromStringID("default:tin_ore").getState(0),registry.getItemFromStringID("default:wooden_pickaxe")));
		TS_ASSERT(retro::canHarvest(registry.getBlockFromStringID("default:tin_ore").getState(0),registry.getItemFromStringID("default:stone_pickaxe")));
		const auto &tin=registry.getBlockFromStringID("default:tin_ore").getState(0);
		TS_ASSERT(retro::breakTime(tin,registry.getItemFromStringID("default:copper_pickaxe"))>retro::breakTime(tin,pick));
		TS_ASSERT(retro::breakTime(tin,pick)>retro::breakTime(tin,registry.getItemFromStringID("default:iron_pickaxe")));
		TS_ASSERT_EQUALS(registry.getItemFromStringID("default:bronze_sword").getGroupValue("group:om_attack_damage"),7);
		const char *pieces[]={"helmet","chestplate","leggings","boots"};
		const char *patterns[][3]={{"BBB","B B",""},{"B B","BBB","BBB"},{"BBB","B B","B B"},{"B B","B B",""}};
		for(u16 slot=0;slot<4;++slot) {
			std::string id=std::string("default:")+pieces[slot]+"_bronze";
			const auto &armor=registry.getItemFromStringID(id);
			TS_ASSERT(armor.canBeActivated()); TS_ASSERT_EQUALS(armor.maxStackSize(),1);
			TS_ASSERT_EQUALS(armor.getGroupValue("group:om_armor_slot"),slot+1);
			TS_ASSERT_EQUALS(armor.getGroupValue("group:om_projectile_reduction"),10);
			Inventory grid(3,3);
			for(u16 y=0;y<3;++y) for(u16 x=0;patterns[slot][y][x];++x)
				if(patterns[slot][y][x]=='B') grid.setStack(x,y,"default:bronze_ingot",1);
			const Recipe *craft=registry.getRecipe(grid);
			TS_ASSERT(craft); if(craft) TS_ASSERT_EQUALS(craft->result().stringID(),id);
		}
	}

	void testBronzeResistanceOnlyReducesProjectileDamageAndWearsOut() {
		Registry registry; Registry::setInstance(registry);
		registry.registerItem<Item>({},BLOCK_AIR,"air");
		auto &armor=registry.registerItem<Item>({},"test:bronze","bronze");
		armor.addGroup("group:om_armor_points",2); armor.addGroup("group:om_max_damage",10000);
		armor.addGroup("group:om_projectile_reduction",10);
		for(const char *cause:{"arrow:skeleton","arrow","projectile:trap","mob:zombie","blast:ghast","fire","fall"}) {
			Player player;
			for(u16 slot=0;slot<4;++slot) player.armor().setStack(0,slot,"test:bronze",1);
			bool projectile=std::string(cause)=="arrow:skeleton" || std::string(cause)=="arrow" || std::string(cause)=="projectile:trap";
			TS_ASSERT_EQUALS(player.absorbDamage(100,cause),projectile?40:68);
			TS_ASSERT_EQUALS(player.armor().getStack(0,0).damage(),100);
		}
		Player partial; partial.armor().setStack(0,0,"test:bronze",1);
		TS_ASSERT_EQUALS(partial.absorbDamage(100,"arrow:skeleton"),82);
		Player broken; broken.armor().setStack(0,0,"test:bronze",1);
		broken.armor().getStackRef(0,0).setDamage(9999);
		broken.absorbDamage(4,"arrow:skeleton");
		TS_ASSERT_EQUALS(broken.armor().getStack(0,0).amount(),0);
		TS_ASSERT_EQUALS(broken.absorbDamage(100,"arrow:skeleton"),100);
	}

	void testTinGeneratesAsStoneOnlyPocketsBelowCopper() {
		Registry registry; Registry::setInstance(registry);
		registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0).isCollidable(false);
		u16 stone=registry.registerBlock<ServerBlock>("default:stone").id();
		u16 tin=registry.registerBlock<ServerBlock>("default:tin_ore").id();
		PlayerList players; Dimension dimension{0,"test:world","test"}; ServerWorld world(players,dimension,42);
		unsigned count=0,pairs=0;
		for(int cx=-1;cx<=1;++cx) for(int cz=0;cz<3;++cz) {
			auto &chunk=world.getOrCreateChunk(cx,0,cz);
			for(int z=0;z<32;++z) for(int y=0;y<16;++y) for(int x=0;x<16;++x) chunk.setBlockRaw(x,y,z,stone);
			chunk.setBlockRaw(0,0,0,0); decorateOreTerrain(chunk,42);
			TS_ASSERT_EQUALS(chunk.getBlock(0,0,0),0);
			for(int z=0;z<32;++z) for(int y=0;y<16;++y) for(int x=0;x<16;++x) if(chunk.getBlock(x,y,z)==tin) {
				++count; TS_ASSERT(z+cz*32>=16 && z+cz*32<=68);
				if(x<15 && chunk.getBlock(x+1,y,z)==tin) ++pairs;
			}
		}
		TS_ASSERT(count>20); TS_ASSERT(pairs>5);
	}
};
#endif
