#ifndef EXPERIENCE_HPP_
#define EXPERIENCE_HPP_

#include <algorithm>
#include <limits>
#include <string>
#include "common/core/IntTypes.hpp"

namespace experience {
struct Progress { unsigned level=0, points=0, capacity=17; };

inline unsigned capacity(unsigned level) {
	return level>=30 ? 62+(level-30)*7 : level>=15 ? 17+(level-15)*3 : 17;
}

inline Progress progress(s32 total) {
	Progress result;
	result.points=unsigned(std::max<s32>(0,total));
	while(result.points>=result.capacity) {
		result.points-=result.capacity;
		result.capacity=capacity(++result.level);
	}
	return result;
}

inline s32 add(s32 total,u32 points) {
	return s32(std::min<u64>(u64(std::max<s32>(0,total))+points,std::numeric_limits<s32>::max()));
}

inline unsigned oreReward(const std::string &id,u32 roll) {
	if(id=="default:coal_ore") return roll%3;
	if(id=="default:diamond_ore") return 3+roll%5;
	if(id=="default:lapis_ore" || id=="default:quartz_ore") return 2+roll%4;
	if(id=="default:redstone_ore") return 1+roll%5;
	return 0;
}
}
#endif
