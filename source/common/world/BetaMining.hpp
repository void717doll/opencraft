#ifndef BETAMINING_HPP_
#define BETAMINING_HPP_

#include <cmath>
#include <limits>

#include "common/inventory/Item.hpp"
#include "common/world/Block.hpp"

namespace retro {

inline u16 attackDamage(const Item &item) {
	u16 damage = item.getGroupValue("group:om_attack_damage");
	return damage ? damage : 1;
}

inline bool canHarvest(const BlockState &state, const Item &item) {
	if (!state.block().id() || !std::isfinite(state.hardness()) || state.hardness() < 0
	 || state.drawType() == BlockDrawType::Liquid) return false;
	if (state.block().hasGroup("group:om_web")) {
		u16 type = item.getGroupValue("group:om_tool_type");
		return type == 1 || type == 4;
	}
	if (!(state.harvestRequirements() & 1)) return true;
	if (!(item.harvestCapability() & 1)) return false;

	u16 level = item.getGroupValue("group:om_harvest_level");
	if (level < state.block().getGroupValue("group:om_min_harvest_level")) return false;
	const std::string &id = state.block().stringID();
	if (id == "default:obsidian") return level == 3;
	if (id == "default:diamond_ore" || id == "default:diamond_block"
	 || id == "default:gold_ore" || id == "default:gold_block"
	 || id == "default:redstone_ore" || id == "default:redstone_block") return level >= 2;
	if (id == "default:iron_ore" || id == "default:iron_block")
		return level >= 2 || item.getGroupValue("group:om_copper_tool") != 0 || item.getGroupValue("group:om_bronze_tool") != 0;
	if (id == "default:copper_ore" || id == "default:lapis_ore" || id == "default:lapis_block") return level >= 1;
	return true;
}

inline float breakTime(const BlockState &state, const Item &item) {
	if (!state.block().id() || !std::isfinite(state.hardness()) || state.hardness() < 0
	 || state.drawType() == BlockDrawType::Liquid) return std::numeric_limits<float>::infinity();
	if (!canHarvest(state, item)) return 5.f * state.hardness();

	bool effective = (state.harvestRequirements() & item.harvestCapability()) != 0;
	for (const auto &target : item.effectiveOn()) {
		if (target == state.block().stringID() || state.block().hasGroup(target)) {
			effective = true;
			break;
		}
	}
	// beta's pickaxe effective-block list omits obsidian and redstone ore.
	if (state.block().stringID() == "default:obsidian" || state.block().stringID() == "default:redstone_ore")
		effective = false;
	float speed = effective ? item.miningSpeed() : 1.f;
	if (!std::isfinite(speed) || speed <= 0) speed = 1.f;
	float time = 1.5f * state.hardness() / speed;
	u16 type = item.getGroupValue("group:om_tool_type");
	u16 kind = item.getGroupValue("group:om_tool_kind");
	const std::string &id = state.block().stringID();
	if (kind) {
		bool effective = false;
		if (kind == 1) effective = id == "default:oak_planks" || id == "default:oak_wood"
			|| id == "default:birch_planks" || id == "default:birch_wood"
			|| id == "default:spruce_planks" || id == "default:spruce_wood"
			|| id == "default:cherry_planks" || id == "default:cherry_wood"
			|| id == "default:jungle_planks" || id == "default:jungle_wood"
			|| id == "default:acacia_planks" || id == "default:acacia_wood"
			|| id == "default:dark_oak_planks" || id == "default:dark_oak_wood"
			|| id == "default:bookshelf" || id == "default:chest"
			|| (state.block().hasGroup("group:om_material_wood")
			 && (state.block().hasGroup("group:om_slab") || state.block().hasGroup("group:om_stairs")));
		if (kind == 2) {
			effective = !state.block().hasGroup("group:om_material_wood")
				&& (state.block().hasGroup("group:om_slab") || state.block().hasGroup("group:om_stairs"));
			for (const char *block : {"cobblestone", "cobblestone_mossy", "stone", "sandstone", "iron_ore",
				"iron_block", "copper_ore", "tin_ore", "coal_ore", "gold_block", "gold_ore", "diamond_ore", "diamond_block", "ice",
				"netherrack", "lapis_ore", "lapis_block", "stone_slab", "double_stone_slab"})
				if (id == std::string("default:") + block) { effective = true; break; }
		}
		if (kind == 3) {
			for (const char *block : {"grass", "dirt", "sand", "gravel", "snow", "snow_layer", "clay", "farmland"})
				if (id == std::string("default:") + block) { effective = true; break; }
		}
		time = 1.5f * state.hardness() / (effective && std::isfinite(item.miningSpeed()) && item.miningSpeed() > 0 ? item.miningSpeed() : 1.f);
	}
	if (type == 1) time = 1.5f * state.hardness() / (state.block().hasGroup("group:om_web") ? 15.f : 1.5f);
	if (type == 4) {
		float speed = state.drawType() == BlockDrawType::Leaves || state.block().hasGroup("group:om_web") ? 15.f
			: state.block().stringID() == "default:wool" ? 5.f : 1.f;
		time = 1.5f * state.hardness() / speed;
	}
	return time;
}

}

#endif // BETAMINING_HPP_
