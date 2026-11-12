#include "common/scene/component/DrawableDef.hpp"
#include "common/core/GameTime.hpp"
#include "common/world/CopperOxidation.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerWorld.hpp"

void ServerWorld::updateCopperOxidation() {
	if (++m_copperAgeTicks<1200) return;
	m_copperAgeTicks=0;
	m_copperRandom^=u32(GameTime::getTicks());
	auto age=[&](ItemStack &stack) {
		if (!stack.amount() || !stack.item().getGroupValue("group:om_copper_tool")) return false;
		m_copperRandom=m_copperRandom*1664525u+1013904223u;
		return oxidizeCopperTool(stack,m_copperRandom>>8);
	};
	auto inventory=[&](Inventory &items) {
		bool changed=false;
		for(u16 y=0;y<items.height();++y) for(u16 x=0;x<items.width();++x) changed=age(items.getStackRef(x,y)) || changed;
		if(changed) items.setChanged(true);
		return changed;
	};
	for(auto &entry:m_players) {
		auto &player=entry.second;
		if(!player.isReady() || !player.isOnline() || player.dimension()!=m_dimension.id()) continue;
		if(inventory(player.inventory()) && m_server) m_server->sendPlayerInvUpdate(player.clientID());
	}
	for(auto &entry:m_chunks) {
		auto &chunk=*entry.second;
		if(!chunk.isInitialized() || !chunk.hasBlockMetadata()) continue;
		for(auto &cell:chunk.blockData()) {
			auto &items=cell.second->inventory;
			if(!inventory(items)) continue;
			chunk.setModified(true);
			const auto &pos=items.blockPos();
			if(m_server) m_server->sendBlockInvUpdate(pos.x,pos.y,pos.z,items);
		}
	}
	m_scene.registry().view<ItemStack>().each([&](auto entity,auto &stack) {
		if(!age(stack)) return;
		if(auto *visual=m_scene.registry().try_get<DrawableDef>(entity)) {
			visual->getInventoryCubeDef().blockID=stack.stringID();
			visual->isUpdated=true;
		}
	});
}
