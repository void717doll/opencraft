/*
 * =====================================================================================
 *
 *  OpenMiner
 *
 *  Copyright (C) 2018-2020 Unarelith, Quentin Bazin <openminer@unarelith.net>
 *  Copyright (C) 2019-2020 the OpenMiner contributors (see CONTRIBUTORS.md)
 *
 *  This file is part of OpenMiner.
 *
 *  OpenMiner is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  OpenMiner is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with OpenMiner; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * =====================================================================================
 */
#include <entt/entt.hpp>

#include "common/core/ApplicationStateStack.hpp"
#include "common/core/Debug.hpp"
#include "common/core/GameTime.hpp"
#include "common/core/GameClock.hpp"
#include "client/scene/component/HurtComponent.hpp"
#include "common/core/Registry.hpp"
#include "common/scene/component/AnimationComponent.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/NetworkComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/scene/component/RotationComponent.hpp"

#include "client/audio/SoundManager.hpp"
#include "client/core/GameConfig.hpp"
#include "client/graphics/Camera.hpp"
#include "client/network/Client.hpp"
#include "client/network/ClientCommandHandler.hpp"
#include "client/scene/component/DrawableComponent.hpp"
#include "client/states/ConnectionErrorState.hpp"
#include "client/states/LuaGUIState.hpp"
#include "client/states/SignEditorState.hpp"
#include "common/world/SignText.hpp"
#include "client/world/ClientPlayer.hpp"
#include "client/world/ClientWorld.hpp"

void ClientCommandHandler::sendPlayerInteract() {
	Network::Packet packet;
	packet << Network::Command::PlayerInteract;
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerDismount() const {
	Network::Packet packet;
	packet << Network::Command::PlayerDismount;
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerAttack() {
	Network::Packet packet;
	packet << Network::Command::PlayerAttack;
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerDropItem() {
	if (m_player.heldItemSlot() < 0) return;
	sendPlayerPosUpdate();
	sendPlayerRotUpdate();
	Network::Packet packet;
	packet << Network::Command::PlayerDropItem;
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerRespawn() {
	Network::Packet packet;
	packet << Network::Command::PlayerRespawn;
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerPause(bool paused) {
	Network::Packet packet;
	packet << Network::Command::PlayerPause << paused;
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerDropStack(const ItemStack &stack) {
	Network::Packet packet;
	packet << Network::Command::PlayerDropStack << stack;
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerInvUpdate() {
	if (m_minecartInventory) { sendMinecartInvUpdate(*m_minecartInventory); return; }
	Network::Packet invPacket;
	invPacket << Network::Command::PlayerInvUpdate;
	invPacket << m_player.inventory() << m_player.armor();
	m_client.send(invPacket);
}

void ClientCommandHandler::sendPlayerPosUpdate() const {
	const Vector3d &position = m_player.physicsPosition();
	Network::Packet packet;
	packet << Network::Command::PlayerPosUpdate;
	packet << position.x << position.y << position.z;
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerRotUpdate() {
	Network::Packet packet;
	packet << Network::Command::PlayerRotUpdate;
	packet << m_player.cameraYaw();
	packet << m_player.cameraPitch();
	m_client.send(packet);
}

void ClientCommandHandler::sendNotePreview(const glm::ivec4 &selectedBlock) {
	Network::Packet packet;
	packet << Network::Command::NotePreview << s32(selectedBlock.x) << s32(selectedBlock.y) << s32(selectedBlock.z);
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerDigBlock(const glm::ivec4 &selectedBlock) {
	Network::Packet packet;
	packet << Network::Command::PlayerDigBlock
		<< s32(selectedBlock.x)
		<< s32(selectedBlock.y)
		<< s32(selectedBlock.z);
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerPlaceBlock(s32 x, s32 y, s32 z, u32 block) {
	Network::Packet packet;
	packet << Network::Command::PlayerPlaceBlock << x << y << z << block;
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerHeldItemChanged(u8 hotbarSlot, u16 itemID) {
	Network::Packet packet;
	packet << Network::Command::PlayerHeldItemChanged
		<< hotbarSlot << itemID;
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerReady() {
	Network::Packet packet;
	packet << Network::Command::PlayerReady;
	m_client.send(packet);
}

void ClientCommandHandler::sendPlayerChunkPosUpdate() const {
	Network::Packet packet;
	const Vector3i &chunkPos = m_player.getCurrentChunk();
	packet << Network::Command::PlayerChunkPosUpdate
		<< s32(chunkPos.x) << s32(chunkPos.y) << s32(chunkPos.z);
	m_client.send(packet);
}

void ClientCommandHandler::sendBlockActivated(const glm::ivec4 &selectedBlock) {
	Network::Packet packet;
	packet << Network::Command::BlockActivated
		<< s32(selectedBlock.x)
		<< s32(selectedBlock.y)
		<< s32(selectedBlock.z)
		<< u16(Config::screenWidth) << u16(Config::screenHeight) << u8(Config::guiScale);
	m_client.send(packet);
}

bool ClientCommandHandler::minecartInReach(u32 id) const {
	auto entry = m_entityMap.find(entt::entity(id));
	if (entry == m_entityMap.end()) return false;
	auto &registry = m_world.scene().registry();
	const auto *position = registry.try_get<PositionComponent>(entry->second);
	if (!position) return false;
	Vector3d distance{position->x - m_player.x(), position->y - m_player.y(), position->z - m_player.z()};
	return distance.length() <= 6;
}

void ClientCommandHandler::sendMinecartInvUpdate(Inventory &inventory) {
	Network::Packet packet;
	packet << Network::Command::MinecartInvUpdate << u32(inventory.blockPos().x) << inventory
		<< m_player.inventory() << m_player.armor();
	m_client.send(packet);
}

void ClientCommandHandler::sendBlockInvUpdate(Inventory &inventory) {
	Network::Packet packet;
	packet << Network::Command::BlockInvUpdate;
	packet << s32(inventory.blockPos().x) << s32(inventory.blockPos().y) << s32(inventory.blockPos().z);
	packet << inventory;
	m_client.send(packet);
}

void ClientCommandHandler::sendItemActivated(const glm::ivec4 &selectedBlock) {
	Network::Packet packet;
	packet << Network::Command::ItemActivated
		<< s32(selectedBlock.x)
		<< s32(selectedBlock.y)
		<< s32(selectedBlock.z) << s8(selectedBlock.w)
		<< u16(Config::screenWidth) << u16(Config::screenHeight) << u8(Config::guiScale);
	m_client.send(packet);
}

void ClientCommandHandler::sendKeyPressed(u16 keyID) {
	Network::Packet packet;
	packet << Network::Command::KeyPressed << keyID
		<< u16(Config::screenWidth) << u16(Config::screenHeight) << u8(Config::guiScale);
	m_client.send(packet);
}

template<typename ComponentType>
static void addComponentCommandCallback(Network::Command command, Client &client, ClientCommandHandler::EntityMap &entityMap, ClientWorld &world) {
	client.setCommandCallback(command, [&, command](Network::Packet &packet) {
		entt::entity entityID;
		packet >> entityID;

		auto it = entityMap.find(entityID);
		if (it != entityMap.end()) {
			// logDebug() << "Received component" << Network::commandToString(command);
			auto &component = world.scene().registry().get_or_emplace<ComponentType>(it->second);
			component.deserialize(packet);
		}
		else
			logError() << Network::commandToString(command) + ": Entity ID" << std::underlying_type_t<entt::entity>(entityID) << "is invalid";
	});
}

void ClientCommandHandler::setupCallbacks() {
	m_client.setCommandCallback(Network::Command::PlayerStatistics, [this](Network::Packet &packet) {
		m_player.statistics().read(packet);
	});

	m_client.setCommandCallback(Network::Command::WorldMode, [this](Network::Packet &packet) {
		bool creative;
		if (packet >> creative) m_player.setCreative(creative);
	});
	m_client.setCommandCallback(Network::Command::PlayerDeath, [this](Network::Packet &packet) {
		std::string message;
		if (packet >> message) m_player.addChatMessage(message);
	});
	m_client.setCommandCallback(Network::Command::PlayerMessage, [this](Network::Packet &packet) {
		std::string message;
		if (packet >> message) m_player.addChatMessage(message);
	});
	m_client.setCommandCallback(Network::Command::EntityHurt, [this](Network::Packet &packet) {
		entt::entity id;
		u16 damage;
		double x, y, z;
		if (!(packet >> id >> damage >> x >> y >> z) || !damage) return;
		auto entity = m_entityMap.find(id);
		if (entity != m_entityMap.end())
			m_world.scene().registry().get_or_emplace<HurtComponent>(entity->second).until = GameClock::getInstance().getTicks() + 500;
		m_player.showHit({x, y, z}, damage);
		if (auto *sound = SoundManager::instance()) sound->play("player_damage", 0.35f);
	});
	m_client.setCommandCallback(Network::Command::PlayerScore, [this](Network::Packet &packet) {
		s32 score;
		if (packet >> score) m_player.setScore(score);
	});
	m_client.setCommandCallback(Network::Command::ServerClosed, [this](Network::Packet &packet) {
		std::string message;
		packet >> message;

		GameConfig::isGameRunning = false;

		ApplicationStateStack::getInstance().clear();
		ApplicationStateStack::getInstance().push<ConnectionErrorState>(message, nullptr);
	});

	m_client.setCommandCallback(Network::Command::RegistryData, [this](Network::Packet &) {
		m_isRegistryInitialized = true;
	});

	m_client.setCommandCallback(Network::Command::ChunkData, [this](Network::Packet &packet) {
		m_world.receiveChunkData(packet);
	});

	m_client.setCommandCallback(Network::Command::ChunkUnload, [this](Network::Packet &packet) {
		s32 chunkX, chunkY, chunkZ;
		packet >> chunkX >> chunkY >> chunkZ;
		m_world.removeChunk({chunkX, chunkY, chunkZ});
	});

	m_client.setCommandCallback(Network::Command::PlayerHealth, [this](Network::Packet &packet) {
		u16 health;
		if (packet >> health) {
			m_player.setHealth(health);
		}
	});

	m_client.setCommandCallback(Network::Command::PlayerHurt, [this](Network::Packet &) {
		m_player.notifyDamage();
		if (auto *sound = SoundManager::instance()) sound->play("player_damage", 0.7f);
	});

	m_client.setCommandCallback(Network::Command::PlayerToolBroken, [](Network::Packet &) {
		if (auto *sound = SoundManager::instance()) sound->play("default_tool_breaks", 0.5f);
	});
	m_client.setCommandCallback(Network::Command::ParticleBurst, [this](Network::Packet &packet) {
		u16 dimension; u8 effect; Vector3d position;
		if (!(packet >> dimension >> effect >> position.x >> position.y >> position.z) || dimension != m_player.dimension()) return;
		m_world.scene().particles().burst(position, effect);
	});
	m_client.setCommandCallback(Network::Command::WorldSound, [this](Network::Packet &packet) {
		std::string name;
		Vector3d position;
		u16 dimension;
		float gain, pitch;
		if (!(packet >> name >> position.x >> position.y >> position.z >> dimension >> gain >> pitch)
		 || dimension != m_player.dimension()) return;
		if (auto *sound = SoundManager::instance()) sound->playAt(name, position, gain, pitch);
	});
	m_client.setCommandCallback(Network::Command::Jukebox, [this](Network::Packet &packet) {
		u16 dimension; s32 x, y, z; u8 record;
		if (!(packet >> dimension >> x >> y >> z >> record) || dimension != m_player.dimension()) return;
		if (auto *sound = SoundManager::instance()) sound->playRecord(record, {x + 0.5, y + 0.5, z + 0.5});
	});

	m_client.setCommandCallback(Network::Command::BlockUpdate, [this](Network::Packet &packet) {
		s32 x, y, z;
		u32 block;
		if (!(packet >> x >> y >> z >> block)) return;
		m_world.receiveBlockUpdate({x, y, z}, block);
	});

	m_client.setCommandCallback(Network::Command::PlayerInvUpdate, [this](Network::Packet &packet) {
		u16 clientId;
		packet >> clientId;

		if (clientId == m_client.id()) {
			packet >> m_player.inventory();

			s8 heldItemSlot;
			packet >> heldItemSlot >> m_player.armor();

			if (m_player.heldItemSlot() == -1) {
				m_player.setHeldItemSlot(heldItemSlot);
			}
		}

	});

	m_client.setCommandCallback(Network::Command::PlayerRideState, [this](Network::Packet &packet) {
		bool mounted;
		double x, y, z;
		if (packet >> mounted >> x >> y >> z) m_player.setRidePosition(mounted, {x, y, z});
	});

	m_client.setCommandCallback(Network::Command::PlayerPosUpdate, [this](Network::Packet &packet) {
		double x, y, z;
		u16 clientId;
		bool isTeleportation;
		packet >> clientId;
		packet >> x >> y >> z;
		packet >> isTeleportation;

		if (clientId == m_client.id() && isTeleportation)
			m_player.setPosition(x, y, z);
	});

	m_client.setCommandCallback(Network::Command::PlayerSpawn, [this](Network::Packet &packet) {
		u16 clientId;
		Vector3d pos;
		u16 dimension;
		std::string username;
		float cameraYaw, cameraPitch;
		packet >> clientId >> pos.x >> pos.y >> pos.z >> dimension
			>> username >> cameraYaw >> cameraPitch;

		if (clientId == m_client.id()) {
			m_player.setPosition(pos.x, pos.y, pos.z);
			m_player.setRotation(cameraYaw, cameraPitch);
			m_player.setRidePosition(false, pos);
			m_player.setDimension(dimension);
			m_player.updateCamera();
		}
	});

	m_client.setCommandCallback(Network::Command::PlayerChangeDimension, [this](Network::Packet &packet) {
		u16 clientId, dimension;
		s32 x, y, z;
		packet >> clientId >> x >> y >> z >> dimension;

		if (clientId == m_client.id()) {
			m_player.setDimension(dimension);
			if (auto *sound = SoundManager::instance()) sound->stopRecord();
			m_player.setRidePosition(false, {x + 0.5, y + 0.5, z + 0.5});
			m_world.clear();
			m_world.changeDimension(dimension);
			m_entityMap.clear();
			sendPlayerChunkPosUpdate();
		}
	});

	m_client.setCommandCallback(Network::Command::SignEditor, [this](Network::Packet &packet) {
		Vector3i position;
		sign::Lines lines;
		packet >> position;
		for (auto &line : lines) { packet >> line; line = sign::cleanLine(line); }
		auto &stack = ApplicationStateStack::getInstance();
		if (packet && !stack.empty() && !dynamic_cast<SignEditorState *>(&stack.top()))
			stack.push<SignEditorState>(m_client, m_world, position, lines, &stack.top());
	});

	m_client.setCommandCallback(Network::Command::BlockGUIData, [this](Network::Packet &packet) {
		if (!LuaGUIState::isActive)
			ApplicationStateStack::getInstance().push<LuaGUIState>(*this, m_player, m_world, packet, &ApplicationStateStack::getInstance().top());
	});

	m_client.setCommandCallback(Network::Command::BlockInvUpdate, [this](Network::Packet &packet) {
		Vector3<s32> pos;
		if (!(packet >> pos.x >> pos.y >> pos.z) || m_world.isBlockDigPending(pos)) return;

		BlockData *data = m_world.getBlockData(pos.x, pos.y, pos.z);
		if (!data)
			data = m_world.addBlockData(pos.x, pos.y, pos.z);

		if (data)
			packet >> data->inventory;
	});

	m_client.setCommandCallback(Network::Command::BlockDataUpdate, [this](Network::Packet &packet) {
		Vector3<s32> pos;
		if (!(packet >> pos.x >> pos.y >> pos.z) || m_world.isBlockDigPending(pos)) return;

		Chunk *chunk = m_world.getChunkAtBlockPos(pos.x, pos.y, pos.z);
		if (chunk) {
			BlockData *data = chunk->getBlockData(pos.x & (CHUNK_WIDTH - 1), pos.y & (CHUNK_DEPTH - 1), pos.z & (CHUNK_HEIGHT - 1));
			if (!data)
				data = m_world.addBlockData(pos.x, pos.y, pos.z);

			if (data) {
				packet >> data->meta;
			}
		}
	});

	m_client.setCommandCallback(Network::Command::EntitySpawn, [this](Network::Packet &packet) {
		entt::entity entityID;
		packet >> entityID;

		auto &registry = m_world.scene().registry();

		auto it = m_entityMap.find(entityID);
		if (it == m_entityMap.end()) {
			entt::entity entity = registry.create();
			m_entityMap.emplace(entityID, entity);
			registry.emplace<NetworkComponent>(entity, entityID);
			// logDebug() << "Entity spawned:" << std::underlying_type_t<entt::entity>(entityID);
		}
		else if (registry.get<NetworkComponent>(it->second).entityID != entityID) {
			logError() << "EntitySpawn: Entity ID" << std::underlying_type_t<entt::entity>(entityID) << "is invalid";
		}
	});

	m_client.setCommandCallback(Network::Command::EntityDespawn, [this](Network::Packet &packet) {
		entt::entity entityID;
		packet >> entityID;

		auto it = m_entityMap.find(entityID);
		if (it != m_entityMap.end()) {
			m_world.scene().registry().destroy(it->second);
			m_entityMap.erase(it);
		}
		else
			logError() << "EntityDespawn: Entity ID" << std::underlying_type_t<entt::entity>(entityID) << "is invalid";
	});

	m_client.setCommandCallback(Network::Command::EntityHitbox, [this](Network::Packet &packet) {
		entt::entity id;
		packet >> id;
		auto it = m_entityMap.find(id);
		if (it != m_entityMap.end())
			packet >> m_world.scene().registry().get_or_emplace<DoubleBox>(it->second);
	});

	addComponentCommandCallback<PositionComponent>(Network::Command::EntityPosition, m_client, m_entityMap, m_world);
	addComponentCommandCallback<RotationComponent>(Network::Command::EntityRotation, m_client, m_entityMap, m_world);
	addComponentCommandCallback<AnimationComponent>(Network::Command::EntityAnimation, m_client, m_entityMap, m_world);
	addComponentCommandCallback<DrawableDef>(Network::Command::EntityDrawableDef, m_client, m_entityMap, m_world);
}

void ClientCommandHandler::sendPlayerNoclip(bool enabled) {
	Network::Packet packet;
	packet << Network::Command::PlayerNoclip << enabled;
	m_client.send(packet);
}

void ClientCommandHandler::requestStatistics() {
	Network::Packet packet;
	packet << Network::Command::PlayerStatistics;
	m_client.send(packet);
}

void ClientCommandHandler::sendCraftedItem(const std::string &name, u16 amount) {
	Network::Packet packet;
	packet << Network::Command::PlayerCrafted << name << amount;
	m_client.send(packet);
}
