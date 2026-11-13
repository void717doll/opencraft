#include <array>
#include <algorithm>
#include <deque>
#include <unordered_set>

#include "common/scene/component/MinecartComponent.hpp"
#include "common/scene/component/MobComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/world/BetaRails.hpp"
#include "common/world/RedstonePower.hpp"
#include "common/inventory/ItemStack.hpp"
#include "common/world/Dimension.hpp"
#include "server/core/PlayerList.hpp"
#include "server/world/ServerWorld.hpp"

namespace {
const std::array<Vector3i,6> sides{{{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}}};

const BlockState *loadedState(const World &world,Vector3i point) {
	const Chunk *chunk=world.getChunkAtBlockPos(point.x,point.y,point.z);
	return chunk && chunk->isInitialized() ? world.getBlockState(point.x,point.y,point.z) : nullptr;
}

bool railConnection(Vector3i from,unsigned shape,Vector3i to,unsigned otherShape) {
	for (auto end:retro::railEnds(shape)) for (auto other:retro::railEnds(otherShape)) {
		Vector3d difference=Vector3d(from)+end-Vector3d(to)-other;
		if (difference.length()<0.0001) return true;
	}
	return false;
}
}

bool ServerWorld::railDirectlyPowered(const Vector3i &point) const {
	return redstonePower(point) || redstonePower(point+Vector3i{0,0,1});
}

void ServerWorld::refreshPoweredRail(const Vector3i &point) {
	const auto *state=loadedState(*this,point);
	if (!state || state->block().getGroupValue("group:om_rail_type")!=2) return;
	bool powered=false;
	std::deque<std::pair<Vector3i,unsigned>> queue{{point,0}};
	std::unordered_set<Vector3i> visited{point};
	// mineclonia's source strength is eight; only connected beta track endpoints conduct.
	while (!queue.empty()) {
		auto entry=queue.front(); queue.pop_front();
		if (railDirectlyPowered(entry.first)) { powered=true; break; }
		if (entry.second>=7) continue;
		const auto *rail=loadedState(*this,entry.first);
		if (!rail) continue;
		unsigned shape=rail->id()&7;
		for (auto end:retro::railEnds(shape)) {
			Vector3i side{int(std::round((end.x-0.5)*2)),int(std::round((end.y-0.5)*2)),int(end.z)};
			for (int below:{0,-1}) {
				Vector3i neighbor=entry.first+side+Vector3i{0,0,below};
				const auto *other=loadedState(*this,neighbor);
				if (!other || other->block().getGroupValue("group:om_rail_type")!=2) continue;
				if (!railConnection(entry.first,shape,neighbor,other->id()&7)) continue;
				if (visited.emplace(neighbor).second) queue.emplace_back(neighbor,entry.second+1);
			}
		}
	}
	u16 next=u16((state->id()&7)|(powered?8:0));
	if (next!=state->id()) setBlockState(point.x,point.y,point.z,next);
}

void ServerWorld::watchRailDevice(const Vector3i &point) {
	if (m_railDevices.emplace(point).second) m_railDeviceQueue.push_back(point);
}

void ServerWorld::updateRailDevices() {
	++m_railPowerTicks;
	std::size_t count=std::min<std::size_t>(64,m_railDeviceQueue.size());
	for(std::size_t i=0;i<count;++i) {
		Vector3i point=m_railDeviceQueue.front(); m_railDeviceQueue.pop_front();
		const auto *state=loadedState(*this,point);
		unsigned source=state?state->block().getGroupValue("group:om_power_source"):0;
		bool detector=state && state->block().getGroupValue("group:om_rail_type")==3;
		if (!state || (!detector && source!=1 && source!=3 && source!=4 && source!=5 && source!=6
		 && !state->block().getGroupValue("group:om_redstone_device"))) {
			m_railDevices.erase(point); m_torchSwitches.erase(point); continue;
		}
		u16 next=state->id();
		bool device=state->block().getGroupValue("group:om_redstone_device")!=0;
		if (source==1 || source==5 || source==6 || device) {
			if (device) m_railDevices.erase(point);
			else m_railDeviceQueue.push_back(point);
			refreshRedstone(point);
			continue;
		} else {
			m_railDeviceQueue.push_back(point);
			double height=detector && (state->id()&7)>=2 ? 1.25 : 0.25;
			DoubleBox trigger{point.x+0.125,point.y+0.125,double(point.z),0.75,0.75,height};
			bool active=false;
			if (detector || source==4) m_scene.registry().view<MinecartComponent,PositionComponent>().each([&](auto &,auto &cart) {
				if (trigger.intersects({cart.x-0.49,cart.y-0.49,cart.z,0.98,0.98,0.7})) active=true;
			});
			if (source==4) m_scene.registry().view<ItemStack,PositionComponent>().each([&](auto &,auto &position) {
				if (trigger.intersects({position.x-0.125,position.y-0.125,position.z-0.125,0.25,0.25,0.25})) active=true;
			});
			if (!detector) {
				for (const auto &entry:m_players) {
					const auto &player=entry.second;
					if (!player.isOnline() || !player.isReady() || !player.health() || player.dimension()!=m_dimension.id()) continue;
					DoubleBox body=player.hitbox(); body.move(player.x(),player.y(),player.z());
					if (trigger.intersects(body)) active=true;
				}
				m_scene.registry().view<MobComponent,PositionComponent,DoubleBox>().each([&](auto entity,auto &,auto &position,auto &hitbox) {
					const auto *traits=m_scene.registry().try_get<BetaMobComponent>(entity);
					if (traits && betaMobIsProjectile(traits->kind)) return;
					DoubleBox body=hitbox; body.move(position.x,position.y,position.z);
					if (trigger.intersects(body)) active=true;
				});
			}
			next=detector?u16((state->id()&7)|(active?8:0)):u16(active);
		}
		if (next==state->id()) continue;
		setBlockState(point.x,point.y,point.z,next);
		for(auto side:sides) m_blockPhysics.enqueueNeighbors(point+side);
	}
}
