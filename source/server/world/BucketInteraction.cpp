#include "common/world/WorldBounds.hpp"
#include "common/core/Registry.hpp"
#include "common/world/Dimension.hpp"
#include "common/world/Player.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/BucketInteraction.hpp"
#include "server/world/ServerWorld.hpp"

namespace {

bool replaceBucket(Player &player, const std::string &replacement) {
	if (player.isCreative()) return true;
	int slot = player.activeItemSlot();
	ItemStack held = player.heldItemStack();
	if (held.amount() == 1) player.heldInventory().setStack(u16(slot), 0, replacement, 1);
	else {
		if (player.inventory().addStack(replacement, 1).amount()) return false;
		held.setAmount(held.amount() - 1);
		player.heldInventory().setItemStack(u16(slot), 0, held);
	}
	return true;
}

}

bool useBucket(const Item &item, glm::ivec3 position, s8 face, Player &player,
		ServerWorld &world, ServerCommandHandler &server) {
	u16 kind = item.getGroupValue("group:om_bucket");
	if (kind < 1 || kind > 4 || player.heldItemSlot() < 0 || player.heldItemSlot() >= player.inventory().width()
	 || !player.heldItemStack().amount() || player.heldItemStack().item().id() != item.id()) return false;
	if (kind == 4) {
		if (!replaceBucket(player, "default:bucket_empty")) return false;
		server.sendPlayerInvUpdate(player.clientID());
		return true;
	}
	if (!worldBounds::contains(position.z) || world.getLight(position.x, position.y, position.z) < 0) return false;
	const BlockState *clicked = world.getBlockState(position.x, position.y, position.z);
	if (kind == 1) {
		const std::string &id = clicked->block().stringID();
		if (id != "default:water" && id != "default:lava") return false;
		if (!replaceBucket(player, id == "default:water" ? "default:bucket_water" : "default:bucket_lava")) return false;
		world.setBlock(position.x, position.y, position.z, 0);
	}
	else {
		if (face < 0 || face > 5) return false;
		if (clicked->drawType() != BlockDrawType::XShape) {
			static const glm::ivec3 offsets[] = {{1,0,0},{0,1,0},{0,0,1},{-1,0,0},{0,-1,0},{0,0,-1}};
			position += offsets[face];
		}
		if (!worldBounds::contains(position.z) || world.getLight(position.x, position.y, position.z) < 0) return false;
		const BlockState *target = world.getBlockState(position.x, position.y, position.z);
		if (target->block().id() && target->drawType() != BlockDrawType::Liquid
		 && target->drawType() != BlockDrawType::XShape && target->block().stringID() != "default:snow") return false;
		if (!replaceBucket(player, "default:bucket_empty")) return false;
		// beta water evaporates in the nether instead of creating a source.
		if (kind == 2 && world.dimension().id() == 1) server.sendParticles({position.x + 0.5, position.y + 0.5, position.z + 0.5}, world.dimension().id(), 1);
		else {
			if (target->block().id() && target->drawType() != BlockDrawType::Liquid) {
				for (const auto &drop : target->getItemDrops())
					world.dropWorldItem({position.x + 0.5, position.y + 0.5, position.z + 0.5}, drop.stringID(), drop.amount());
			}
			u16 liquid = Registry::getInstance().getBlockFromStringID(kind == 2 ? "default:water" : "default:lava").id();
			world.setBlock(position.x, position.y, position.z, liquid);
		}
	}
	server.sendPlayerInvUpdate(player.clientID());
	return true;
}
