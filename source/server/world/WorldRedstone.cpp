#include "common/core/Registry.hpp"
#include "common/world/RedstonePower.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/world/ServerPlayer.hpp"
#include "server/world/ServerWorld.hpp"

unsigned ServerWorld::redstonePower(const Vector3i &point, bool includeWire) const {
	return redstone::power(*this, point, includeWire);
}

void ServerWorld::queueRedstone(const Vector3i &point) {
	const auto *state=redstone::stateAt(*this,point);
	if (!state || (!redstone::source(state) && !state->block().getGroupValue("group:om_redstone_device")
	 && state->block().stringID()!="default:tnt")) return;
	if (m_redstonePending.emplace(point).second) m_redstoneQueue.push_back(point);
}

void ServerWorld::notifyRedstone(const Vector3i &point) {
	static const std::array<Vector3i,25> neighbors{{
		{0,0,0},{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1},
		{-2,0,0},{2,0,0},{0,-2,0},{0,2,0},{0,0,-2},{0,0,2},
		{-1,-1,0},{-1,1,0},{1,-1,0},{1,1,0},
		{-1,0,-1},{-1,0,1},{1,0,-1},{1,0,1},
		{0,-1,-1},{0,-1,1},{0,1,-1},{0,1,1}
	}};
	for (auto offset : neighbors) queueRedstone(point + offset);
	// quasi-power can reach a piston through a solid neighbor above its base.
	static const std::array<Vector3i,13> pistonNeighbors{{
		{-2,0,-1},{2,0,-1},{0,-2,-1},{0,2,-1},
		{-1,-1,-1},{-1,1,-1},{1,-1,-1},{1,1,-1},
		{-1,0,-2},{1,0,-2},{0,-1,-2},{0,1,-2},{0,0,-3}
	}};
	for (auto offset : pistonNeighbors) {
		Vector3i piston=point+offset;
		const auto *state=redstone::stateAt(*this,piston);
		if (state && state->block().hasGroup("group:om_piston")) queueRedstone(piston);
	}
}

void ServerWorld::scheduleRedstone(const Vector3i &point, u16 state, u16 delay) {
	if (m_redstoneTimers.count(point)) return;
	const auto *block = redstone::stateAt(*this, point);
	if (!block) return;
	m_redstoneTimers.emplace(point, RedstoneTimer{block->block().id(), state, m_redstoneTicks + delay});
	m_redstoneTimerQueue.push_back(point);
	auto *data = getBlockData(point.x, point.y, point.z);
	if (!data) data = addBlockData(point.x, point.y, point.z, 0, 0);
	data->meta.setInt("redstone_delay", delay);
	data->meta.setInt("redstone_next", state);
	data->meta.setInt("redstone_block", block->block().id());
	static_cast<ServerChunk *>(getChunkAtBlockPos(point.x,point.y,point.z))->setModified(true);
}

void ServerWorld::refreshRedstone(const Vector3i &point) {
	const auto *state = redstone::stateAt(*this, point);
	if (!state) return;
	unsigned kind = redstone::source(state);
	if (state->block().hasGroup("group:om_piston") || state->block().hasGroup("group:om_note_block") || state->block().hasGroup("group:om_piston_head")) {
		refreshBetaDevice(point);
		return;
	}
	if (!kind && !state->block().getGroupValue("group:om_redstone_device") && state->block().stringID() != "default:tnt") return;
	if (kind == 7) {
		unsigned power = redstonePower(point, false) ? 15 : 0;
		for (auto side : redstone::horizontal) {
			auto neighbor = redstone::wireNeighbor(point, side, [&](Vector3i p) { return redstone::stateAt(*this,p); });
			if (!neighbor) continue;
			unsigned other = getBlockState(neighbor->x, neighbor->y, neighbor->z)->id();
			if (other) power = std::max(power, other - 1);
		}
		if (power != state->id()) {
			setBlockState(point.x,point.y,point.z,u16(power));
			notifyRedstone(point);
		}
		return;
	}
	if (kind == 1 || kind == 3 || kind == 4 || kind == 5 || kind == 6
	 || state->block().getGroupValue("group:om_redstone_device")) watchRailDevice(point);
	if (!m_redstoneTimers.count(point)) {
		auto *data = getBlockData(point.x,point.y,point.z);
		if (data && data->meta.hasInt("redstone_delay") && data->meta.get<int>("redstone_delay") > 0
		 && data->meta.hasInt("redstone_block") && data->meta.get<int>("redstone_block") == state->block().id()
		 && data->meta.hasInt("redstone_next"))
			scheduleRedstone(point,u16(data->meta.get<int>("redstone_next")),u16(std::clamp(data->meta.get<int>("redstone_delay"),1,80)));
	}
	if (kind == 1) {
		auto &switches = m_torchSwitches[point];
		while (!switches.empty() && m_railPowerTicks-switches.front() >= 60) switches.pop_front();
		bool off = redstone::strongPower(*this,redstone::support(*this,point,*state)) != 0 || switches.size() >= 8;
		if (unsigned(off) != state->id()) scheduleRedstone(point,u16(off),2);
	} else if (kind == 5) {
		if (state->id() && !m_redstoneTimers.count(point)) scheduleRedstone(point,0,20);
	} else if (kind == 6) {
		Vector3i behind = point-redstone::forward(redstone::rotation(*this,point,*state));
		bool input = redstone::inputFrom(*this,point,behind) != 0;
		if (input != bool(state->id()&4)) scheduleRedstone(point,u16((state->id()&3)|(input?4:0)),u16(((state->id()&3)+1)*2));
	} else if (state->block().stringID() == "default:tnt") {
		if (redstonePower(point)) primeTnt(point);
	}
}

void ServerWorld::updateRedstoneCircuits() {
	++m_redstoneTicks;
	std::size_t timers = std::min<std::size_t>(256,m_redstoneTimerQueue.size());
	for (std::size_t i = 0; i < timers; ++i) {
		Vector3i point = m_redstoneTimerQueue.front(); m_redstoneTimerQueue.pop_front();
		auto entry = m_redstoneTimers.find(point);
		if (entry == m_redstoneTimers.end()) continue;
		const auto *state = redstone::stateAt(*this,point);
		if (!state) {
			// save the remaining delay and resume when this chunk becomes available again.
			m_redstoneTimers.erase(entry);
			continue;
		}
		if (state->block().id() != entry->second.block) { m_redstoneTimers.erase(entry); continue; }
		auto *data = getBlockData(point.x,point.y,point.z);
		u64 remaining = entry->second.due > m_redstoneTicks ? entry->second.due-m_redstoneTicks : 0;
		if (data) data->meta.setInt("redstone_delay",int(remaining));
		static_cast<ServerChunk *>(getChunkAtBlockPos(point.x,point.y,point.z))->setModified(true);
		if (remaining) { m_redstoneTimerQueue.push_back(point); continue; }
		u16 next = entry->second.state;
		m_redstoneTimers.erase(entry);
		unsigned kind = redstone::source(state);
		if (kind == 1) {
			bool off = redstone::strongPower(*this,redstone::support(*this,point,*state)) != 0 || m_torchSwitches[point].size() >= 8;
			if (next != unsigned(off)) continue;
			if (next && !state->id()) m_torchSwitches[point].push_back(m_railPowerTicks);
		}
		if (kind == 6) next = u16((next&4)|(state->id()&3));
		if (next >= state->block().states().size()) continue;
		setBlockState(point.x,point.y,point.z,next);
		notifyRedstone(point);
	}
	// process newly queued wires in this same pass, but never recurse through a circuit.
	for (unsigned work = 0; work < 1024 && !m_redstoneQueue.empty(); ++work) {
		Vector3i point = m_redstoneQueue.front(); m_redstoneQueue.pop_front(); m_redstonePending.erase(point);
		refreshRedstone(point);
	}
}

bool ServerWorld::activateRedstone(const Vector3i &point) {
	const auto *state = redstone::stateAt(*this,point);
	unsigned kind = redstone::source(state);
	if (kind == 2) setBlockState(point.x,point.y,point.z,state->id()?0:1);
	else if (kind == 5) {
		if (state->id()) return true;
		setBlockState(point.x,point.y,point.z,1);
		scheduleRedstone(point,0,20);
	} else if (kind == 6) setBlockState(point.x,point.y,point.z,u16((state->id()&4)|((state->id()+1)&3)));
	else return false;
	notifyRedstone(point);
	return true;
}

bool ServerWorld::placeRedstoneDust(Player &player, const Vector3i &point, s8 face) {
	const auto *clicked = redstone::stateAt(*this,point);
	if (!clicked || face < 0 || face > 5) return false;
	static constexpr std::array<Vector3i,6> faces{{{1,0,0},{0,1,0},{0,0,1},{-1,0,0},{0,-1,0},{0,0,-1}}};
	Vector3i target = point + faces[unsigned(face)];
	const auto *current = redstone::stateAt(*this,target);
	const auto *below = redstone::stateAt(*this,target+Vector3i{0,0,-1});
	if (!current || (current->block().id() && current->drawType()!=BlockDrawType::XShape) || !redstone::solid(below)) return false;
	auto &definitions = Registry::getInstance();
	if (!definitions.hasBlock("default:redstone_wire")) return false;
	auto held = player.heldItemStack();
	if (!held.amount() || held.stringID() != "default:redstone_dust") return false;
	setData(target.x,target.y,target.z,0);
	setBlock(target.x,target.y,target.z,definitions.getBlockFromStringID("default:redstone_wire").id());
	if (!player.isCreative()) player.inventory().setStack(u16(player.heldItemSlot()),0,
		held.amount()>1?held.stringID():"",held.amount()-1);
	if (m_server) m_server->sendPlayerInvUpdate(player.clientID());
	notifyRedstone(target);
	return true;
}
