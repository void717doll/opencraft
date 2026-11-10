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
#include <map>

#include "common/core/IntTypes.hpp"
#include "common/network/Network.hpp"

std::string Network::commandToString(Network::Command command) {
	std::map<Network::Command, std::string> commandNames = {
		{Network::Command::ClientConnect,         "ClientConnect"},
		{Network::Command::ClientDisconnect,      "ClientDisconnect"},
		{Network::Command::ClientOk,              "ClientOk"},

		{Network::Command::ServerClosed,          "ServerClosed"},

		{Network::Command::ChunkData,             "ChunkData"},
		{Network::Command::ChunkUnload,           "ChunkUnload"},

		{Network::Command::PlayerPlaceBlock,      "PlayerPlaceBlock"},
		{Network::Command::PlayerDigBlock,        "PlayerDigBlock"},
		{Network::Command::NotePreview,           "NotePreview"},
		{Network::Command::PlayerInvUpdate,       "PlayerInvUpdate"},
		{Network::Command::PlayerPosUpdate,       "PlayerPosUpdate"},
		{Network::Command::PlayerRotUpdate,       "PlayerRotUpdate"},
		{Network::Command::PlayerSpawn,           "PlayerSpawn"},
		{Network::Command::PlayerChangeDimension, "PlayerChangeDimension"},
		{Network::Command::PlayerHeldItemChanged, "PlayerHeldItemChanged"},
		{Network::Command::PlayerDropItem, "PlayerDropItem"},
		{Network::Command::PlayerToolBroken, "PlayerToolBroken"},
		{Network::Command::PlayerHurt, "PlayerHurt"},
		{Network::Command::PlayerRespawn, "PlayerRespawn"},
		{Network::Command::PlayerScore, "PlayerScore"},
		{Network::Command::Jukebox, "Jukebox"},
		{Network::Command::PlayerDropStack, "PlayerDropStack"},
		{Network::Command::PlayerPause, "PlayerPause"},
		{Network::Command::PlayerDeath, "PlayerDeath"},
		{Network::Command::PlayerMessage, "PlayerMessage"},
		{Network::Command::ParticleBurst, "ParticleBurst"},
		{Network::Command::EntityHurt, "EntityHurt"},
		{Network::Command::PlayerReady,           "PlayerReady"},
		{Network::Command::PlayerHealth, "PlayerHealth"},
		{Network::Command::PlayerAttack, "PlayerAttack"},
		{Network::Command::PlayerInteract, "PlayerInteract"},
		{Network::Command::EntityHitbox, "EntityHitbox"},
		{Network::Command::PlayerChunkPosUpdate,  "PlayerChunkPosUpdate"},

		{Network::Command::BlockUpdate,           "BlockUpdate"},
		{Network::Command::BlockActivated,        "BlockActivated"},
		{Network::Command::BlockGUIData,          "BlockGUIData"},
		{Network::Command::MinecartInvUpdate, "MinecartInvUpdate"},
		{Network::Command::BlockInvUpdate,        "BlockInvUpdate"},
		{Network::Command::BlockDataUpdate,       "BlockDataUpdate"},
		{Network::Command::SignEditor, "SignEditor"},
		{Network::Command::SignUpdate, "SignUpdate"},

		{Network::Command::ItemActivated,         "ItemActivated"},
		{Network::Command::WorldSound,            "WorldSound"},
		{Network::Command::WorldMode,             "WorldMode"},

		{Network::Command::RegistryData,          "RegistryData"},


		{Network::Command::EntitySpawn,           "EntitySpawn"},
		{Network::Command::EntityDespawn,         "EntityDespawn"},
		{Network::Command::EntityPosition,        "EntityPosition"},
		{Network::Command::EntityRotation,        "EntityRotation"},
		{Network::Command::EntityAnimation,       "EntityAnimation"},
		{Network::Command::EntityDrawableDef,     "EntityDrawableDef"},

		{Network::Command::PlayerRideState, "PlayerRideState"},
		{Network::Command::PlayerDismount, "PlayerDismount"},
		{Network::Command::KeyPressed,            "KeyPressed"},
	};

	return commandNames[command];
}

sf::Packet &operator<<(sf::Packet &packet, Network::Command command) {
	return packet << static_cast<u16>(command);
}

sf::Packet &operator>>(sf::Packet &packet, Network::Command &command) {
	u16 tmp = 0;
	packet >> tmp;
	command = static_cast<Network::Command>(tmp);
	return packet;
}
