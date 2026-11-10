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
#ifndef NETWORK_HPP_
#define NETWORK_HPP_

#include <SFML/Network/Packet.hpp>
#include "common/core/IntTypes.hpp"

// local message ids also appear in saved component data.
namespace Network {
	using Packet = sf::Packet;


	enum class Command {
		// Client commands
		ClientConnect         = 0x00,
		ClientDisconnect      = 0x01,
		ClientOk              = 0x02,

		ServerClosed          = 0x11,

		ChunkData             = 0x20,
		ChunkUnload           = 0x21,

		PlayerPlaceBlock      = 0x30,
		PlayerDigBlock        = 0x31,
		NotePreview           = 0x94,
		PlayerInvUpdate       = 0x32,
		PlayerPosUpdate       = 0x33,
		PlayerRotUpdate       = 0x34,
		PlayerSpawn           = 0x35,
		PlayerChangeDimension = 0x36,
		PlayerHeldItemChanged = 0x37,
		PlayerReady           = 0x38,
		PlayerChunkPosUpdate  = 0x39,
		PlayerHealth          = 0x3a,
		PlayerAttack          = 0x3b,
		PlayerInteract        = 0x3c,
		PlayerDropItem        = 0x3d,
		PlayerToolBroken      = 0x3e,
		PlayerHurt            = 0x3f,

		BlockUpdate           = 0x40,
		BlockActivated        = 0x41,
		BlockGUIData          = 0x42,
		BlockInvUpdate        = 0x43,
		BlockDataUpdate       = 0x44,
		SignEditor            = 0x92,
		SignUpdate            = 0x93,

		EntitySpawn           = 0x50,
		EntityDespawn         = 0x51,
		EntityPosition        = 0x52,
		EntityRotation        = 0x53,
		EntityAnimation       = 0x54,
		EntityDrawableDef     = 0x55,
		EntityHitbox          = 0x56,
		EntityHurt            = 0x57,
		MinecartInvUpdate     = 0x58,

		ItemActivated         = 0x60,
		WorldSound            = 0x61,
		WorldMode             = 0x62,

		RegistryData          = 0x70,
		PlayerRespawn         = 0x80,
		PlayerScore           = 0x81,
		PlayerDropStack       = 0x82,
		PlayerPause           = 0x83,
		PlayerDeath           = 0x84,
		PlayerMessage         = 0x87,
		ParticleBurst         = 0x88,
		Jukebox               = 0x89,
		PlayerNoclip          = 0x8b,
		PlayerStatistics      = 0x8a,
		PlayerCrafted         = 0x8c,
		PlayerRideState       = 0x85,
		PlayerDismount        = 0x86,


		KeyPressed            = 0x90,
	};

	std::string commandToString(Command command);
}

sf::Packet &operator<<(sf::Packet &packet, Network::Command command);
sf::Packet &operator>>(sf::Packet &packet, Network::Command &command);

#endif // NETWORK_HPP_
