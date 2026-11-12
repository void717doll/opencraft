#ifndef MATERIALPALETTETESTS_HPP_
#define MATERIALPALETTETESTS_HPP_
#include <cxxtest/TestSuite.h>
#include "common/core/MaterialPalette.hpp"

class MaterialPaletteTests : public CxxTest::TestSuite {
public:
	void testMaterialColorsCoverInventoryTerrainAndWornArmor() {
		using namespace materialPalette;
		TS_ASSERT(forTexture("mods/default/textures/armor/helmet_bronze.png")==Kind::Bronze);
		TS_ASSERT(forTexture("bronze_ingot.png")==Kind::Bronze);
		TS_ASSERT(forTexture("copper_ore.png")==Kind::Copper);
		TS_ASSERT(forTexture("copper_pickaxe.png")==Kind::CopperTool);
		TS_ASSERT(forTexture("refi_rope.png")==Kind::Rope);
		TS_ASSERT(forTexture("refi_rope_strand.png")==Kind::Rope);
		TS_ASSERT(forTexture("helmet_iron.png")==Kind::None);
		auto copper=color(Kind::CopperTool,130,121,38);
		TS_ASSERT(copper[0]>copper[1] && copper[1]>copper[2]);
		auto bronze=color(Kind::Bronze,130,178,122);
		TS_ASSERT(bronze[0]>bronze[1] && bronze[1]>bronze[2]);
		auto rope=color(Kind::Rope,6,116,31);
		TS_ASSERT(rope[0]>rope[1] && rope[1]>rope[2]);
		TS_ASSERT(copper!=bronze);
	}
	void testSteelMetalIsDarkerThanStoneAndPreservesTheHandle() {
		using namespace materialPalette;
		TS_ASSERT(forTexture("steel_pickaxe.png")==Kind::Steel);
		TS_ASSERT(forTexture("steel_sword.png")==Kind::Steel);
		TS_ASSERT(forTexture("stone_pickaxe.png")==Kind::None);
		auto steel=color(Kind::Steel,194,194,194);
		TS_ASSERT(steel[0]<100); TS_ASSERT_EQUALS(steel[0],steel[1]); TS_ASSERT_EQUALS(steel[1],steel[2]);
		auto stone=color(Kind::None,116,116,116); TS_ASSERT(steel[0]<stone[0]);
		auto handle=color(Kind::Steel,104,87,42);
		TS_ASSERT_EQUALS(handle[0],104); TS_ASSERT_EQUALS(handle[1],87); TS_ASSERT_EQUALS(handle[2],42);
		auto outline=color(Kind::Steel,47,47,47); TS_ASSERT_EQUALS(outline[0],47);
	}
	void testPistonInteriorMarkerUsesDarkMetal() {
		using namespace materialPalette;
		TS_ASSERT(forTexture("refi_piston_extended.png")==Kind::PistonInterior);
		auto metal=color(Kind::PistonInterior,13,205,203);
		TS_ASSERT_EQUALS(metal[0],metal[1]); TS_ASSERT_EQUALS(metal[1],metal[2]); TS_ASSERT(metal[0]<80);
		auto stone=color(Kind::PistonInterior,128,128,128); TS_ASSERT_EQUALS(stone[0],128);
	}
	void testStoneShadingAndWoodenHandlesKeepTheirColors() {
		using namespace materialPalette;
		for(u8 value:{0,47,116,255}) {
			auto stone=color(Kind::Copper,value,value,value);
			TS_ASSERT_EQUALS(stone[0],value); TS_ASSERT_EQUALS(stone[1],value); TS_ASSERT_EQUALS(stone[2],value);
		}
		auto handle=color(Kind::CopperTool,104,87,42);
		TS_ASSERT_EQUALS(handle[0],104); TS_ASSERT_EQUALS(handle[1],87); TS_ASSERT_EQUALS(handle[2],42);
		auto outline=color(Kind::CopperTool,0,40,1);
		TS_ASSERT(outline[0]>outline[1]);
	}
};
#endif
