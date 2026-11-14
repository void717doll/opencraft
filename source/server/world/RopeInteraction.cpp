#include <algorithm>
#include <vector>
#include "common/core/Registry.hpp"
#include "common/world/Player.hpp"
#include "common/world/WorldBounds.hpp"
#include "server/world/RopeInteraction.hpp"
#include "server/world/ServerWorld.hpp"

bool useClimbingRope(ServerWorld &world, Player &player, Vector3i position, s8 face) {
	auto &registry = Registry::getInstance();
	if (!registry.hasBlock("default:rope")) return false;
	u16 rope = registry.getBlockFromStringID("default:rope").id();
	auto loaded = [&](Vector3i p) {
		const auto *chunk = world.getChunkAtBlockPos(p.x,p.y,p.z);
		return worldBounds::contains(p.z) && chunk && chunk->isInitialized();
	};
	if (!loaded(position)) return false;
	if (world.getBlock(position.x,position.y,position.z) == rope) {
		unsigned above = 0;
		while (world.getBlock(position.x,position.y,position.z) == rope) {
			if (++above > 32) return false;
			++position.z;
			if (!loaded(position)) return false;
		}
	} else if (face != 5) return false;
	const auto *anchor = world.getBlockState(position.x,position.y,position.z);
	if (!anchor->isCollidable() || !anchor->isOpaque()) return false;
	--position.z;
	unsigned length = 0;
	while (loaded(position) && world.getBlock(position.x,position.y,position.z) == rope) {
		if (++length >= 32) return false;
		--position.z;
	}
	ItemStack held = player.heldItemStack();
	unsigned budget = std::min<unsigned>(8,32-length);
	if (!player.isCreative()) budget = std::min<unsigned>(budget,held.amount());
	std::vector<Vector3i> cells;
	while (cells.size() < budget && loaded(position) && world.getBlock(position.x,position.y,position.z) == 0) {
		cells.push_back(position);
		--position.z;
	}
	if (cells.empty()) return false;
	for (const auto &cell : cells) world.setBlock(cell.x,cell.y,cell.z,rope);
	if (!player.isCreative()) {
		held.setAmount(u16(held.amount()-cells.size()));
		player.heldInventory().setItemStack(player.activeItemSlot(),0,held);
	}
	return true;
}
