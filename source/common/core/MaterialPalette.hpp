#ifndef MATERIALPALETTE_HPP_
#define MATERIALPALETTE_HPP_
#include <algorithm>
#include <array>
#include <string>
#include "common/core/IntTypes.hpp"

namespace materialPalette {
enum class Kind { None, Copper, CopperTool, Bronze, Rope };
inline Kind forTexture(const std::string &path) {
	std::string name=path.substr(path.find_last_of("/\\")+1);
	if (name=="refi_rope.png" || name=="refi_rope_strand.png") return Kind::Rope;
	if (name.compare(0,7,"bronze_")==0 || (name.size()>=11 && name.compare(name.size()-11,11,"_bronze.png")==0)) return Kind::Bronze;
	if (name=="copper_ingot.png" || name=="copper_ore.png") return Kind::Copper;
	if (name.compare(0,7,"copper_")==0) return Kind::CopperTool;
	return Kind::None;
}
inline std::array<u8,3> color(Kind kind,u8 r,u8 g,u8 b) {
	if (kind==Kind::None || (r==g && g==b)) return {r,g,b};
	unsigned value=std::max({unsigned(r),unsigned(g),unsigned(b)});
	if (kind==Kind::Bronze) {
		if (g<=r || g<=b) return {r,g,b};
		return {u8(value*95/100),u8(value*62/100),u8(value*24/100)};
	}
	if (kind==Kind::Rope) return {u8(value),u8(value*76/100),u8(value*46/100)};
	if (kind==Kind::CopperTool && unsigned(g)*100<unsigned(r)*86) return {r,g,b};
	return {u8(std::min(255u,value*145/100)),u8(value*72/100),u8(value*38/100)};
}
}
#endif
