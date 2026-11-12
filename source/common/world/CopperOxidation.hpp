#ifndef COPPEROXIDATION_HPP_
#define COPPEROXIDATION_HPP_
#include "common/core/Registry.hpp"
#include "common/inventory/ItemStack.hpp"

inline bool oxidizeCopperTool(ItemStack &stack,u32 roll) {
	if (!stack.amount() || roll%20 || !stack.item().getGroupValue("group:om_copper_tool")) return false;
	unsigned stage=stack.item().getGroupValue("group:om_copper_stage");
	if (stage>=3) return false;
	auto copper=stack.stringID().find("copper_");
	if (copper==std::string::npos) return false;
	const char *prefix[]={"exposed_","weathered_","oxidized_"};
	std::string next=stack.item().modName()+":"+prefix[stage]+stack.stringID().substr(copper);
	if (!Registry::getInstance().hasItem(next)) return false;
	stack=ItemStack{next,stack.amount(),stack.damage()};
	return true;
}
#endif
