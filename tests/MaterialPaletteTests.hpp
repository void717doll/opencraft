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
