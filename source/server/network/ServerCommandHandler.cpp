#include "common/world/BetaPiston.hpp"
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
#include "common/world/BlockSupport.hpp"
#include "common/world/Experience.hpp"
#include <random.hpp>
#include "common/core/GameTime.hpp"
#include "common/core/Registry.hpp"
#include "common/core/Utils.hpp"
#include "common/scene/component/AnimationComponent.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/NetworkComponent.hpp"
#include "common/world/BlockData.hpp"
#include "common/world/SignText.hpp"
#include "server/core/PlayerList.hpp"
#include "server/lua/ScriptEngine.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "common/world/WorldCollision.hpp"
#include "common/world/BetaToolRules.hpp"
#include "common/world/RetroMobSpawn.hpp"
#include "common/core/GameClock.hpp"
#include "common/scene/component/MobComponent.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerItem.hpp"
#include "server/world/WorldController.hpp"
#include "common/scene/component/CargoMinecartComponent.hpp"
#include "common/scene/component/PositionComponent.hpp"

namespace {
struct UseHand {
	Player &player;
	explicit UseHand(Player &value) : player(value) { player.setUsingOffhand(player.shouldUseOffhand()); }
	~UseHand() { player.setUsingOffhand(false); }
};

bool canReachBlock(const ServerWorld &world, const ServerPlayer &player, int x, int y, int z) {
	const Chunk *chunk = world.getChunkAtBlockPos(x, y, z);
	if (!player.isReady() || !player.health() || !chunk || !chunk->isInitialized()) return false;
	Vector3d eye{player.x(), player.y(), player.z() + 1.625};
	Vector3d target{std::clamp(eye.x, double(x), double(x + 1)),
		std::clamp(eye.y, double(y), double(y + 1)), std::clamp(eye.z, double(z), double(z + 1))};
	Vector3d delta = target - eye;
	if (delta.length() > 4.001) return false;
	int steps = std::max(1, int(std::ceil(delta.length() * 16)));
	for (int step = 1; step < steps; ++step) {
		Vector3d point = eye + delta * (double(step) / steps);
		int bx = int(std::floor(point.x)), by = int(std::floor(point.y)), bz = int(std::floor(point.z));
		if (bx == x && by == y && bz == z) continue;
		const BlockState *state = world.getBlockState(bx, by, bz);
		if (!state) return false;
		if (state->block().id() && state->isCollidable()) {
			DoubleBox box = collision::blockBox(*state, world.getData(bx, by, bz), bx, by, bz);
			if (point.x > box.x && point.x < box.x + box.sizeX && point.y > box.y && point.y < box.y + box.sizeY
			 && point.z > box.z && point.z < box.z + box.sizeZ) return false;
		}
	}
	return true;
}
}

void ServerCommandHandler::sendParticles(Vector3d position, u16 dimension, u8 effect) const {
	if (effect > 2) return;
	Network::Packet packet;
	packet << Network::Command::ParticleBurst << dimension << effect << position.x << position.y << position.z;
	m_server.sendToAllClients(packet);
}

bool ServerCommandHandler::consumeHeldItem(Player &player, u16 amount, u16 clientID) {
	if (player.clientID() != clientID || !amount) return false;
	int slot = player.activeItemSlot();
	if (slot < 0 || slot >= player.inventory().width()) return false;
	ItemStack stack = player.heldItemStack();
	if (stack.amount() < amount) return false;
	if (player.isCreative()) return true;
	stack.setAmount(stack.amount() - amount);
	player.heldInventory().setItemStack(u16(slot), 0, stack);
	sendPlayerInvUpdate(clientID);
	return true;
}

bool ServerCommandHandler::usePortal(u16 clientID, s32 x, s32 y, s32 z) {
	ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (!player || !player->isOnline() || !player->isReady() || !player->health() || player->dimension() > 1) return false;
	ServerWorld &source = m_worldController.getWorld(player->dimension());
	if (!canReachBlock(source, *player, x, y, z)) return false;
	Vector3i origin, axis;
	if (!source.portalFrameAt({x, y, z}, origin, axis)) return false;
	if (!player->tryPortal(GameClock::getInstance().getTicks(true))) return false;
	u16 targetDimension = 1 - player->dimension();
	ServerWorld &target = m_worldController.getWorld(targetDimension);
	double scale = targetDimension ? 0.125 : 8.0;
	double targetX = std::floor(origin.x * scale), targetY = std::floor(origin.y * scale);
	if (std::abs(targetX) > 1000000000 || std::abs(targetY) > 1000000000) return false;
	Vector3i requested{int(targetX), int(targetY), origin.z};
	BlockData *data = source.getBlockData(origin.x, origin.y, origin.z);
	if (data && data->meta.hasInt("portal_x") && data->meta.hasInt("portal_y") && data->meta.hasInt("portal_z")) {
		requested = {data->meta.get<int>("portal_x"), data->meta.get<int>("portal_y"), data->meta.get<int>("portal_z")};
		if (std::abs(double(requested.x)) > 1000000000 || std::abs(double(requested.y)) > 1000000000) return false;
	}
	Vector3i arrival;
	if (!target.findPortalDestination(requested, arrival)) return false;
	auto link = [](ServerWorld &world, Vector3i position, Vector3i destination) {
		Chunk *chunk = world.getChunkAtBlockPos(position.x, position.y, position.z);
		BlockData *data = chunk->addBlockData(position.x & (CHUNK_WIDTH - 1), position.y & (CHUNK_DEPTH - 1), position.z & (CHUNK_HEIGHT - 1));
		data->meta.setInt("portal_x", destination.x);
		data->meta.setInt("portal_y", destination.y);
		data->meta.setInt("portal_z", destination.z);
		static_cast<ServerChunk *>(chunk)->setModified(true);
	};
	link(source, origin, arrival);
	link(target, arrival, origin);
	sendPlayerChangeDimension(clientID, arrival.x, arrival.y, arrival.z, targetDimension, player->client());
	return true;
}

void ServerCommandHandler::wearHeldItem(Player &player, u16 amount) {
	if (player.isCreative()) return;
	int slot = player.activeItemSlot();
	if (slot < 0 || slot >= player.inventory().width()) return;
	auto &stack = player.heldInventory().getStackRef(u16(slot), 0);
	u16 previous = stack.amount();
	std::string name = stack.stringID();
	if (amount && previous) ++player.statistics().items[name].used;
	if (!stack.wear(amount)) return;
	sendPlayerInvUpdate(player.clientID());
	if (stack.amount() < previous) {
		++player.statistics().items[name].broken;
		Network::Packet packet;
		packet << Network::Command::PlayerToolBroken;
		m_server.sendToAllClients(packet);
	}
}

void ServerCommandHandler::sendServerClosed(const std::string &message, ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::ServerClosed << message;

	if (!client) {
		m_server.sendToAllClients(packet);
		m_server.disconnectAllClients();
	}
	else {
		client->send(packet);
		m_server.disconnectClient(*client);
	}
}

void ServerCommandHandler::sendChunkUnload(s32 chunkX, s32 chunkY, s32 chunkZ, const ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::ChunkUnload << chunkX << chunkY << chunkZ;

	if (!client)
		m_server.sendToAllClients(packet);
	else
		client->send(packet);
}

void ServerCommandHandler::sendBlockDataUpdate(s32 x, s32 y, s32 z, const BlockData *blockData, const ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::BlockDataUpdate << x << y << z << blockData->meta;

	if (!client)
		m_server.sendToAllClients(packet);
	else
		client->send(packet);
}

void ServerCommandHandler::sendSound(const std::string &name, const Vector3d &position, u16 dimension, float gain, float pitch, const ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::WorldSound << name << position.x << position.y << position.z << dimension << gain << pitch;
	if (client) client->send(packet);
	else m_server.sendToAllClients(packet);
}

void ServerCommandHandler::sendBlockInvUpdate(s32 x, s32 y, s32 z, const Inventory &inventory, const ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::BlockInvUpdate << x << y << z << inventory;

	if (!client)
		m_server.sendToAllClients(packet);
	else
		client->send(packet);
}

void ServerCommandHandler::sendPlayerPosUpdate(u16 clientID, bool isTeleportation, const ClientInfo *client) const {
	const ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (player) {
		Network::Packet packet;
		packet << Network::Command::PlayerPosUpdate;
		packet << clientID;
		packet << player->x() << player->y() << player->z();
		packet << isTeleportation;

		if (!client)
			m_server.sendToAllClients(packet);
		else
			client->send(packet);
	}
	else
		logError() << ("Failed to send position update for player " + std::to_string(clientID) + ": Player not found").c_str();
}

const std::string &ServerCommandHandler::worldName() const { return m_worldController.worldName(); }

void ServerCommandHandler::sendPlayerInvUpdate(u16 clientID, const ClientInfo *client) const {
	ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (player) {
		Network::Packet packet;
		packet << Network::Command::PlayerInvUpdate;
		packet << clientID << player->inventory() << player->heldItemSlot() << player->armor() << player->offhand();

		if (!client)
			m_server.sendToAllClients(packet);
		else
			client->send(packet);
	}
	else
		logError() << ("Failed to send inventory update for player " + std::to_string(clientID) + ": Player not found").c_str();
}

void ServerCommandHandler::sendPlayerChangeDimension(u16 clientID, s32 x, s32 y, s32 z, u16 dimension, const ClientInfo *client) const {
	ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (player) {
		Network::Packet packet;
		packet << Network::Command::PlayerChangeDimension;
		packet << clientID << x << y << z << dimension;

		if (client) {
			client->send(packet);

			// FIXME: sendPlayerChangeDimension shouldn't be exposed to Lua
			//        Instead, there should be a changePlayerDimension function that sends
			//        the packet above + the entities (instead of doing that here)
			//        also, it should change player dimension and position accordingly
			m_worldController.getWorld(dimension).scene().sendEntities(*client);

			player->setPosition(x + 0.5, y + 0.5, z + 0.5);
			player->setDimension(dimension);
			player->resetSurvival();
			player->clearLoadedChunks();
			player->sentChunks.clear();
		}
		else
			m_server.sendToAllClients(packet);
	}
	else
		logError() << ("Failed to send dimension change for player " + std::to_string(clientID) + ": Player not found").c_str();
}

void ServerCommandHandler::sendDimensionPacket(Network::Packet &packet, u16 dimension) const {
	for (const auto &entry : m_players) {
		const auto &player = entry.second;
		if (player.isOnline() && player.isReady() && player.dimension() == dimension)
			player.client()->send(packet);
	}
}

void ServerCommandHandler::sendEntitySpawn(entt::entity entityID, u16 dimension, const ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::EntitySpawn << entityID;
	if (client) client->send(packet);
	else sendDimensionPacket(packet, dimension);
}

void ServerCommandHandler::sendEntityDespawn(entt::entity entityID, u16 dimension, const ClientInfo *client) const {
	Network::Packet packet;
	packet << Network::Command::EntityDespawn << entityID;
	if (client) client->send(packet);
	else sendDimensionPacket(packet, dimension);
}

void ServerCommandHandler::sendPlayerHealth(u16 clientID) const {
	ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (!player || !player->client()) return;
	Network::Packet packet;
	packet << Network::Command::PlayerHealth << player->health();
	player->client()->send(packet);
}

void ServerCommandHandler::sendPlayerScore(u16 clientID) const {
	const ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (!player || !player->client()) return;
	Network::Packet packet;
	packet << Network::Command::PlayerScore << player->score();
	player->client()->send(packet);
}

void ServerCommandHandler::sendEntityHurt(entt::entity entity, u16 dimension, u16 amount, double x, double y, double z) const {
	Network::Packet packet;
	packet << Network::Command::EntityHurt << entity << amount << x << y << z;
	sendDimensionPacket(packet, dimension);
}

void ServerCommandHandler::damagePlayer(u16 clientID, u16 amount, const std::string &cause) {
	ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (!player || !player->health() || player->isCreative()) return;
	amount = player->survival().filterDamage(amount);
	if (!amount) return;
	amount = player->absorbDamage(amount, cause);
	if (player->armor().hasChanged()) {
		sendPlayerInvUpdate(clientID);
		player->armor().setChanged(false);
	}
	u16 dealt = std::min(amount, player->health());
	player->damage(amount);
	player->statistics().general[PlayerStatistics::DamageTaken] += dealt;
	Network::Packet hurt;
	hurt << Network::Command::PlayerHurt;
	if (player->client()) player->client()->send(hurt);
	if (!player->health()) {
		++player->statistics().general[PlayerStatistics::Deaths];
		getWorldForClient(clientID).dropPlayerItems(*player);
		std::string message = player->name();
		std::string killer = cause.substr(cause.find(':') + 1);
		std::replace(killer.begin(), killer.end(), '_', ' ');
		if (cause.compare(0, 4, "mob:") == 0) message += " was slain by " + (killer.empty() ? "a mob" : killer);
		else if (cause.compare(0, 6, "arrow:") == 0) message += " was shot by " + (killer.empty() ? "an arrow" : killer);
		else if (cause.compare(0, 6, "blast:") == 0) message += " was blown up by " + (killer.empty() ? "an explosion" : killer);
		else if (cause == "fall") message += " hit the ground too hard";
		else if (cause == "drowning") message += " drowned";
		else if (cause == "lava") message += " tried to swim in lava";
		else if (cause == "fire") message += " burned to death";
		else if (cause == "cactus") message += " was pricked to death by a cactus";
		else if (cause == "suffocation") message += " suffocated in a wall";
		else message += " died";
		Network::Packet death;
		death << Network::Command::PlayerDeath << message;
		m_server.sendToAllClients(death);
	}
	sendPlayerHealth(clientID);
}

void ServerCommandHandler::sendPlayerMessage(u16 clientID, const std::string &message) const {
	if (message.empty()) return;
	Network::Packet packet;
	packet << Network::Command::PlayerMessage << message;
	const auto *player = m_players.getPlayerFromClientID(clientID);
	if (player && player->client()) player->client()->send(packet);
}

void ServerCommandHandler::useBed(u16 clientID, s32 x, s32 y, s32 z) {
	auto *player = m_players.getPlayerFromClientID(clientID);
	if (!player) return;
	auto &world = getWorldForClient(clientID);
	if (!canReachBlock(world, *player, x, y, z)) return;
	std::string message = world.sleepInBed(*player, {x, y, z});
	sendPlayerMessage(clientID, message);
	if (message == "respawn point set") sendPlayerPosUpdate(clientID, true);
}

void ServerCommandHandler::respawnPlayer(u16 clientID) {
	ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (!player || player->health()) return;
	ServerWorld &world = m_worldController.getWorld(0);
	int z = world.heightmap().getHighestBlockAt(0, 0) + 1;
	Vector3d arrival{0.5, 0.5, double(z)};
	if (player->bedSpawn() && !world.bedRespawn(*player->bedSpawn(), arrival, true)) {
		player->clearBedSpawn();
		sendPlayerMessage(clientID, "your bed is missing or obstructed");
	}
	if (player->dimension() != 0) sendPlayerChangeDimension(clientID, s32(std::floor(arrival.x)),
		s32(std::floor(arrival.y)), s32(std::floor(arrival.z)), 0, player->client());
	player->setPosition(arrival.x, arrival.y, arrival.z);
	player->setHealth(20);
	player->setScore(0);
	player->resetSurvival();
	sendPlayerPosUpdate(clientID, true);
	sendPlayerHealth(clientID);
	sendPlayerScore(clientID);
}

void ServerCommandHandler::setupCallbacks() {
	m_server.setCommandCallback(Network::Command::SignUpdate, [this](ClientInfo &client, Network::Packet &packet) {
		Vector3i position;
		sign::Lines lines;
		packet >> position;
		for (auto &line : lines) packet >> line;
		auto *player = m_players.getPlayerFromClientID(client.id);
		auto &world = getWorldForClient(client.id);
		if (!packet || !player || !canReachBlock(world, *player, position.x, position.y, position.z)) return;
		const auto *state = world.getBlockState(position.x, position.y, position.z);
		if (!state || !state->block().hasGroup("group:om_sign")) return;
		auto *data = world.getBlockData(position.x, position.y, position.z);
		if (!data) data = world.addBlockData(position.x, position.y, position.z);
		if (!data) return;
		sign::write(data->meta, lines);
		static_cast<ServerChunk *>(world.getChunkAtBlockPos(position.x, position.y, position.z))->setModified(true);
		sendBlockDataUpdate(position.x, position.y, position.z, data);
	});
	m_server.setCommandCallback(Network::Command::PlayerStatistics, [this](ClientInfo &client, Network::Packet &) {
		auto *player = m_players.getPlayerFromClientID(client.id);
		if (!player) return;
		Network::Packet answer;
		answer << Network::Command::PlayerStatistics;
		player->statistics().write(answer);
		client.send(answer);
	});
	m_server.setCommandCallback(Network::Command::PlayerCrafted, [this](ClientInfo &client, Network::Packet &packet) {
		std::string name;
		u16 amount = 0;
		auto *player = m_players.getPlayerFromClientID(client.id);
		if (player && (packet >> name >> amount) && amount && amount <= 99 && m_registry.hasItem(name))
			player->statistics().items[name].crafted += amount;
	});

	m_server.setCommandCallback(Network::Command::PlayerNoclip, [this](ClientInfo &client, Network::Packet &packet) {
		bool enabled = false;
		auto *player = m_players.getPlayerFromClientID(client.id);
		if (player && (packet >> enabled)) player->setNoclip(enabled && !player->isRidingMinecart());
	});

	m_server.setCommandCallback(Network::Command::PlayerPause, [this](ClientInfo &, Network::Packet &packet) {
		bool paused;
		if (packet >> paused) m_server.setPaused(paused);
	});
	m_server.setCommandCallback(Network::Command::PlayerDropStack, [this](ClientInfo &client, Network::Packet &packet) {
		ItemStack stack;
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player && packet >> stack) getWorldForClient(client.id).dropPlayerStack(*player, stack);
	});
	m_server.setCommandCallback(Network::Command::PlayerRespawn, [this](ClientInfo &client, Network::Packet &) {
		respawnPlayer(client.id);
	});
	m_server.setConnectionCallback([this](ClientInfo &client, Network::Packet &connectionPacket) {
		std::string username;
		connectionPacket >> username;
		if (!connectionPacket) {
			sendServerClosed("unable to load the local player", &client);
			return;
		}

		ServerPlayer *player = m_players.connectPlayer(username, client);
		if (!player) {
			sendServerClosed("unable to load the local player", &client);
			return;
		}

		client.creative = m_worldController.getWorld(0).isCreative();
		player->setCreative(client.creative);
		Network::Packet mode;
		mode << Network::Command::WorldMode << client.creative;
		client.send(mode);

		// Try to find a valid spawn point (WIP)
		if (player->isNewPlayer()) {
			// FIXME: Default dimension hardcoded here
			ServerWorld &world = m_worldController.getWorld(0);
			Heightmap &heightmap = world.heightmap();

			bool hasFoundPosition = false;
			for (s32 spawnChunkX = 0 ; spawnChunkX < 16 && !hasFoundPosition ; ++spawnChunkX) {
				for (s32 spawnChunkY = 0 ; spawnChunkY < 16 && !hasFoundPosition ; ++spawnChunkY) {
					for(int y = 0 ; y < CHUNK_DEPTH && !hasFoundPosition ; y++) {
						for(int x = 0 ; x < CHUNK_WIDTH ; x++) {
							int maxChunkZ = heightmap.getHighestChunkAt(x + spawnChunkX * CHUNK_WIDTH, y + spawnChunkY * CHUNK_DEPTH);
							int worldZ = heightmap.getHighestBlockAt(x + spawnChunkX * CHUNK_WIDTH, y + spawnChunkY * CHUNK_DEPTH) + 1;
							int z = math::pmod(worldZ, CHUNK_WIDTH);

							world.generateChunk(world.getOrCreateChunk(spawnChunkX - 1, spawnChunkY,     maxChunkZ));
							world.generateChunk(world.getOrCreateChunk(spawnChunkX + 1, spawnChunkY,     maxChunkZ));
							world.generateChunk(world.getOrCreateChunk(spawnChunkX,     spawnChunkY - 1, maxChunkZ));
							world.generateChunk(world.getOrCreateChunk(spawnChunkX,     spawnChunkY + 1, maxChunkZ));
							world.generateChunk(world.getOrCreateChunk(spawnChunkX,     spawnChunkY,     maxChunkZ - 1));
							world.generateChunk(world.getOrCreateChunk(spawnChunkX,     spawnChunkY,     maxChunkZ + 1));

							ServerChunk &chunk = world.getOrCreateChunk(spawnChunkX, spawnChunkY, maxChunkZ);
							world.generateChunk(chunk);

							const BlockState *blockBelow = chunk.getBlockState(x, y, z - 1);
							const Block &blockFeet = m_registry.getBlock(chunk.getBlock(x, y, z));
							const Block &blockHead = m_registry.getBlock(chunk.getBlock(x, y, z + 1));

							if (blockFeet.id() == 0 && blockHead.id() == 0
							&& blockBelow && blockBelow->isCollidable()
							&& blockBelow->drawType() != BlockDrawType::Leaves)
							{
								player->setPosition(x + spawnChunkX * CHUNK_WIDTH + .5, y + spawnChunkY * CHUNK_DEPTH + .5, worldZ + .2);
								hasFoundPosition = true;
								break;
							}
						}
					}
				}
			}

			if (!hasFoundPosition)
				logError() << "Can't find a good position for the player";

			player->setHeldItemSlot(0);
		}

		// Send the registry
		Network::Packet packet;
		packet << Network::Command::RegistryData;
		client.send(packet);


		// Triggers the 'PlayerConnected' Lua event
		if (player->isNewPlayer())
			m_scriptEngine.luaCore().onEvent(LuaEventType::PlayerConnected, glm::ivec3{player->x(), player->y(), player->z()}, player, client, *this);

		// Send inventory
		sendPlayerInvUpdate(client.id, &client);
		sendPlayerHealth(client.id);
		sendPlayerScore(client.id);

		// send the local player after connection callbacks have run.
		Network::Packet spawnPacket;
		spawnPacket << Network::Command::PlayerSpawn << client.id;
		spawnPacket << player->x() << player->y() << player->z() << player->dimension() << player->name();
		spawnPacket << player->cameraYaw() << player->cameraPitch();
		client.send(spawnPacket);

		// Send entities to the client
		m_worldController.getWorld(player->dimension()).scene().sendEntities(client);
	});

	m_server.setCommandCallback(Network::Command::ClientDisconnect, [this](ClientInfo &client, Network::Packet &) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			player->clearLoadedChunks();
			m_players.disconnectPlayer(client.playerName);
		}
	});

	m_server.setCommandCallback(Network::Command::ChunkUnload, [this](ClientInfo &client, Network::Packet &packet) {
		s32 cx, cy, cz;
		packet >> cx >> cy >> cz;

		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			player->removeLoadedChunk(Vector3i{cx, cy, cz});
		}
		else
			logError() << ("Failed to unload chunk for player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::PlayerInvUpdate, [this](ClientInfo &client, Network::Packet &packet) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			Inventory inventory, armor;
			if (!(packet >> inventory >> armor) || inventory.width() != 9 || inventory.height() != 4
			 || armor.width() != 1 || armor.height() != 4) return;
			for (u16 slot = 0; slot < 4; ++slot) {
				const auto &stack = armor.getStack(0, slot);
				if (stack.amount() && (stack.amount() != 1 || stack.item().getGroupValue("group:om_armor_slot") != slot + 1)) {
					sendPlayerInvUpdate(client.id, &client);
					return;
				}
			}
			if (!packet.endOfPacket()) {
				Inventory offhand;
				if (!(packet >> offhand) || offhand.width() != 1 || offhand.height() != 1 || offhand.name() != "offhand" || offhand.inBlock()) return;
				player->offhand() = std::move(offhand);
			}
			player->inventory() = std::move(inventory);
			player->armor() = std::move(armor);
		}
		else
			logError() << ("Failed to update inventory of player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::PlayerPosUpdate, [this](ClientInfo &client, Network::Packet &packet) {
		double x, y, z;
		packet >> x >> y >> z;

		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player && packet) {
			if (!getWorldForClient(client.id).moveSurvivalPlayer(*player, x, y, z))
				sendPlayerPosUpdate(client.id, true, &client);
		}
		else
			logError() << ("Failed to update position of player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::PlayerRotUpdate, [this](ClientInfo &client, Network::Packet &packet) {
		float yaw, pitch;
		packet >> yaw >> pitch;

		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			player->setRotation(yaw, pitch);
		}
		else
			logError() << ("Failed to update rotation of player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::PlayerPlaceBlock, [this](ClientInfo &client, Network::Packet &packet) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			UseHand hand(*player);
			s32 x, y, z;
			u32 block;
			packet >> x >> y >> z >> block;
			if (!packet) return;

			ServerWorld &world = getWorldForClient(client.id);
			const BlockState *previous = world.getBlockState(x, y, z);
			u16 id = u16(block & 0xffff);
			const auto &blocks = Registry::getInstance().blocks();
			bool merged = false;
			if (id > 0 && id < blocks.size()) {
				Vector3i destination{x, y, z};
				merged = resolveSlabPlacement(world, destination, *blocks[id]);
				x = destination.x; y = destination.y; z = destination.z;
				previous = world.getBlockState(x, y, z);
				if (merged) block = u32(id) | (u32(blocks[id]->param().setParam(BlockParam::State, 0, 1)) << 16);
			}
			ItemStack stack;
			if (player->heldItemSlot() >= 0 && player->heldItemSlot() < player->inventory().width())
				stack = player->heldItemStack();
			bool valid = packet && previous && worldBounds::contains(z) && canReachBlock(world, *player, x, y, z) && id > 0 && id < blocks.size()
				&& stack.amount() > 0 && stack.item().isBlock() && stack.stringID() == blocks[id]->stringID();
			if (valid) {
				const Block &definition = *blocks[id];
				u16 data = u16(block >> 16);
				u16 stateID = definition.param().hasParam(BlockParam::State)
					? definition.param().getParam(BlockParam::State, data) : 0;
				valid = stateID < definition.states().size();
				if (definition.stringID() == "default:bed") valid = stateID == 0;
				if (definition.hasGroup("group:om_piston")) {
					valid = stateID == 0;
					data = definition.param().setParam(BlockParam::Rotation,0,retro::pistonPlacementRotation(*player,{x,y,z}));
					block = u32(id) | (u32(data) << 16);
				}
				if (definition.hasGroup("group:om_slab")) valid = stateID == (merged ? 1 : 0);
				if (definition.hasGroup("group:om_stairs") || definition.hasGroup("group:om_chest"))
					valid = valid && definition.param().getParam(BlockParam::Rotation, data) < 4;
				if (valid) {
					const BlockState &state = definition.getState(stateID);
					DoubleBox playerBox = player->hitbox();
					playerBox.move(player->x(), player->y(), player->z());
					valid = blockHasSupport(world, {x, y, z}, state, data)
						&& (!state.isCollidable() || !collision::blockBox(state, data, x, y, z).intersects(playerBox));
				}
				valid = valid && (merged || isReplaceableBlock(*previous))
					&& definition.placementConstraints().check(world, {x, y, z});
			}
			if (valid && blocks[id]->stringID() == "default:bed") {
				u16 data = 0;
				valid = world.placeBed(*player, {x, y, z}, data);
				if (valid) block = u32(id) | (u32(data) << 16);
			}
			if (!valid) {
				Network::Packet correction;
				correction << Network::Command::BlockUpdate << x << y << z
					<< (u32(world.getBlock(x, y, z)) | (u32(world.getData(x, y, z)) << 16));
				client.send(correction);
				sendPlayerInvUpdate(client.id, &client);
				return;
			}
			if (!player->isCreative())
				player->heldInventory().setStack(player->activeItemSlot(), 0,
					stack.amount() > 1 ? stack.item().stringID() : "", stack.amount() - 1);
			sendPlayerInvUpdate(client.id);
			if (merged) world.setBlockState(x, y, z, 1);
			else {
				world.setData(x, y, z, u16(block >> 16));
				world.setBlock(x, y, z, u16(block & 0xffff));
			}
			block = u32(world.getBlock(x, y, z)) | (u32(world.getData(x, y, z)) << 16);
			++player->statistics().items[blocks[id]->stringID()].placed;

			const BlockState *blockState = world.getBlockState(x, y, z);
			if (!blockState) return;

			m_scriptEngine.luaCore().onEvent(LuaEventType::BlockPlaced, glm::ivec3{x, y, z}, *blockState, *player, world, client, *this);

			Network::Packet answer;
			answer << Network::Command::BlockUpdate << x << y << z << block;
			m_server.sendToAllClients(answer);
			if (blocks[id]->hasGroup("group:om_sign")) {
				auto *data = world.getBlockData(x, y, z);
				if (!data) data = world.addBlockData(x, y, z);
				Network::Packet editor;
				editor << Network::Command::SignEditor << Vector3i{x, y, z};
				for (const auto &line : sign::read(data->meta)) editor << line;
				client.send(editor);
			}
		}
		else
			logError() << ("Failed to place block using player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::PlayerDismount, [this](ClientInfo &client, Network::Packet &) {
		auto *player = m_players.getPlayerFromClientID(client.id);
		if (player && player->isRidingMinecart()) getWorldForClient(client.id).dismountMinecart(*player);
	});

	m_server.setCommandCallback(Network::Command::PlayerInteract, [this](ClientInfo &client, Network::Packet &) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (!player || !player->isReady() || !player->health()) return;
		UseHand hand(*player);
		auto &world = getWorldForClient(client.id);
		if (!world.interactMinecart(*player)) world.interactMob(*player);
	});

	m_server.setCommandCallback(Network::Command::PlayerAttack, [this](ClientInfo &client, Network::Packet &) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (!player || !player->health() || !player->tryAttack(GameClock::getInstance().getTicks(true))) return;
		auto &world = getWorldForClient(client.id);
		if (!world.attackMinecart(*player)) world.attackMob(*player);
	});

	m_server.setCommandCallback(Network::Command::NotePreview, [this](ClientInfo &client, Network::Packet &packet) {
		s32 x,y,z;
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (!(packet >> x >> y >> z) || !player || !player->isReady() || !player->health()) return;
		auto &world = getWorldForClient(client.id);
		if (canReachBlock(world,*player,x,y,z)) world.playNote(x,y,z,false);
	});

	m_server.setCommandCallback(Network::Command::PlayerDigBlock, [this](ClientInfo &client, Network::Packet &packet) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			s32 x, y, z;
			packet >> x >> y >> z;

			ServerWorld &world = getWorldForClient(client.id);
			const BlockState *blockState = world.getBlockState(x, y, z);
			if (!packet) return;
			if (!blockState || !canReachBlock(world, *player, x, y, z)
			 || !blockState->block().id() || blockState->drawType() == BlockDrawType::Liquid
			 || (!player->isCreative() && (!std::isfinite(blockState->hardness()) || blockState->hardness() < 0))) {
				Network::Packet correction;
				u32 block = u32(world.getBlock(x, y, z)) | (u32(world.getData(x, y, z)) << 16);
				correction << Network::Command::BlockUpdate << x << y << z << block;
				client.send(correction);
				if (const auto *data = world.getBlockData(x, y, z)) {
					sendBlockDataUpdate(x, y, z, data, &client);
					sendBlockInvUpdate(x, y, z, data->inventory, &client);
				}
				return;
			}
			++player->statistics().items[blockState->block().stringID()].mined;
			if (!player->isCreative()) {
				ItemStack held;
				if (player->heldItemSlot()>=0 && player->heldItemSlot()<player->inventory().width()) held=player->heldItemStack();
				if (retro::canHarvest(*blockState,held.item())) {
					u32 roll=effolkronium::random_static::get<u32>(0,1000000);
					unsigned points=experience::oreReward(blockState->block().stringID(),roll);
					if (points) { player->setScore(experience::add(player->score(),points)); sendPlayerScore(client.id); }
				}
			}
			world.setBlock(x, y, z, 0);

			if (!player->isCreative()) m_scriptEngine.luaCore().onEvent(LuaEventType::BlockDigged, glm::ivec3{x, y, z}, *blockState, *player, world, client, *this);
			if (player->heldItemSlot() >= 0 && player->heldItemSlot() < player->inventory().width()) {
				auto &tool = player->heldInventory().getStackRef(player->activeItemSlot(), 0);
				wearHeldItem(*player, retro::diggingWear(tool.item(), *blockState));
			}

			Network::Packet answer;
			answer << Network::Command::BlockUpdate << x << y << z << u32(0);
			m_server.sendToAllClients(answer);
		}
		else
			logError() << ("Failed to dig block using player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::PlayerHeldItemChanged, [this](ClientInfo &client, Network::Packet &packet) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			u8 hotbarSlot;
			u16 itemID;
			packet >> hotbarSlot >> itemID;
			if (!packet || hotbarSlot >= 9 || hotbarSlot >= player->inventory().width()) return;
			if (player->inventory().getStack(hotbarSlot, 0).item().id() != itemID)
				logWarning() << "PlayerHeldItemChanged:" << "Desync of item ID between client and server";

			player->setHeldItemSlot(hotbarSlot);
		}
		else
			logError() << ("Failed to change held item of player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::PlayerSwapHands, [this](ClientInfo &client, Network::Packet &) {
		auto *player = m_players.getPlayerFromClientID(client.id);
		if (player && player->isReady() && player->health() && player->swapHands()) sendPlayerInvUpdate(client.id);
	});

	m_server.setCommandCallback(Network::Command::PlayerDropItem, [this](ClientInfo &client, Network::Packet &) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player && getWorldForClient(client.id).dropHeldItem(*player))
			++player->statistics().general[PlayerStatistics::Dropped];
	});

	m_server.setCommandCallback(Network::Command::PlayerReady, [this](ClientInfo &client, Network::Packet &) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			player->setReady(true);
		}
		else
			logError() << ("Failed to change held item of player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::PlayerChunkPosUpdate, [this](ClientInfo &client, Network::Packet &packet) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			s32 chunkX, chunkY, chunkZ;
			packet >> chunkX >> chunkY >> chunkZ;

			ServerWorld &world = getWorldForClient(client.id);
			world.updatePlayerChunks(*player, chunkX, chunkY, chunkZ);
		}
		else
			logError() << ("Failed to update chunk position of player " + std::to_string(client.id) + ": Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::BlockActivated, [this](ClientInfo &client, Network::Packet &packet) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			UseHand hand(*player);
			s32 x, y, z;
			packet >> x >> y >> z >> client.screenWidth >> client.screenHeight >> client.guiScale;

			ServerWorld &world = getWorldForClient(client.id);
			if (!packet || !canReachBlock(world, *player, x, y, z)) return;

			u16 id = world.getBlock(x, y, z);
			ServerBlock &block = (ServerBlock &)(m_registry.getBlock(id));
			if (block.hasGroup("group:om_sign")) {
				auto *data = world.getBlockData(x, y, z);
				if (!data) data = world.addBlockData(x, y, z);
				Network::Packet editor;
				editor << Network::Command::SignEditor << Vector3i{x, y, z};
				for (const auto &line : sign::read(data->meta)) editor << line;
				client.send(editor);
				return;
			}
			bool hasBeenActivated = block.onBlockActivated({x, y, z}, *player, world, client, *this);

			if (hasBeenActivated)
				m_scriptEngine.luaCore().onEvent(LuaEventType::BlockActivated, glm::ivec3{x, y, z}, block, *player, world, client, *this);
		}
		else
			logError() << ("Failed to activate block using player '" + client.playerName + "': Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::MinecartInvUpdate, [this](ClientInfo &client, Network::Packet &packet) {
		u32 id;
		Inventory inventory, main, armor;
		if (!(packet >> id >> inventory >> main >> armor) || inventory.width() != 9 || inventory.height() != 3
			|| inventory.name() != "minecart" || inventory.inBlock() || u32(inventory.blockPos().x) != id
			|| main.width() != 9 || main.height() != 4 || armor.width() != 1 || armor.height() != 4) return;
		auto *player = m_players.getPlayerFromClientID(client.id);
		if (!player || !player->health()) return;
		auto &registry = getWorldForClient(client.id).scene().registry();
		auto entity = entt::entity(id);
		if (!registry.valid(entity) || !registry.all_of<CargoMinecartComponent, PositionComponent>(entity)) return;
		auto &cargo = registry.get<CargoMinecartComponent>(entity);
		const auto &position = registry.get<PositionComponent>(entity);
		Vector3d distance{position.x-player->x(), position.y-player->y(), position.z-player->z()};
		if (cargo.kind != 1 || cargo.viewer != player->name() || distance.length() > 6) return;
		for (u16 slot = 0; slot < 4; ++slot) {
			const auto &stack = armor.getStack(0, slot);
			if (stack.amount() && (stack.amount() != 1 || stack.item().getGroupValue("group:om_armor_slot") != slot + 1)) return;
		}
		if (!packet.endOfPacket()) {
			Inventory offhand;
			if (!(packet >> offhand) || offhand.width() != 1 || offhand.height() != 1 || offhand.name() != "offhand" || offhand.inBlock()) return;
			player->offhand() = std::move(offhand);
		}
		cargo.inventory = std::move(inventory);
		player->inventory() = std::move(main);
		player->armor() = std::move(armor);
	});

	m_server.setCommandCallback(Network::Command::BlockInvUpdate, [this](ClientInfo &client, Network::Packet &packet) {
		Vector3<s32> pos;
		packet >> pos.x >> pos.y >> pos.z;

		auto &world = getWorldForClient(client.id);
		BlockData *data = world.getBlockData(pos.x, pos.y, pos.z);
		if (data) {
			Inventory inventory;
			packet >> inventory;
			if (!packet || inventory.width() != data->inventory.width() || inventory.height() != data->inventory.height()
			 || !inventory.inBlock() || inventory.blockPos() != pos) return;
			data->inventory = std::move(inventory);
			auto *chunk = static_cast<ServerChunk *>(world.getChunkAtBlockPos(pos.x, pos.y, pos.z));
			if (chunk) chunk->setModified(true);
		}
		else
			logError() << "BlockInvUpdate: No block data found at" << pos.x << pos.y << pos.z;
	});

	m_server.setCommandCallback(Network::Command::BlockDataUpdate, [this](ClientInfo &client, Network::Packet &packet) {
		Vector3<s32> pos;
		packet >> pos.x >> pos.y >> pos.z;

		BlockData *data = getWorldForClient(client.id).getBlockData(pos.x, pos.y, pos.z);
		if (data) {
			packet >> data->meta;
		}
		else
			logError() << "BlockDataUpdate: No block data found at" << pos.x << pos.y << pos.z;
	});

	m_server.setCommandCallback(Network::Command::ItemActivated, [this](ClientInfo &client, Network::Packet &packet) {
		ServerPlayer *player = m_players.getPlayerFromClientID(client.id);
		if (player) {
			UseHand hand(*player);
			s32 x, y, z;
			s8 face;
			packet >> x >> y >> z >> face >> client.screenWidth >> client.screenHeight >> client.guiScale;

			ServerWorld &world = getWorldForClient(client.id);
			if (!packet || !canReachBlock(world, *player, x, y, z)) return;

			u16 id = world.getBlock(x, y, z);
			ServerBlock &block = (ServerBlock &)(m_registry.getBlock(id));

			if (player->heldItemSlot() < 0 || player->heldItemSlot() >= player->inventory().width()
			 || player->heldItemStack().amount() == 0 || player->heldItemStack().item().isBlock()) return;
			const ServerItem &item = static_cast<const ServerItem &>(player->heldItemStack().item());
			if (item.canBeActivated()) {
				bool hasBeenActivated = item.onItemActivated({x, y, z}, block, *player, world, client, *this, face);

				if (hasBeenActivated)
					m_scriptEngine.luaCore().onEvent(LuaEventType::ItemActivated, glm::ivec3{x, y, z}, block, *player, world, client, *this);
			}
		}
		else
			logError() << ("Failed to activate item using player '" + client.playerName + "': Player not found").c_str();
	});

	m_server.setCommandCallback(Network::Command::KeyPressed, [this](ClientInfo &client, Network::Packet &packet) {
		u16 keyID;
		packet >> keyID >> client.screenWidth >> client.screenHeight >> client.guiScale;

		m_registry.getKey(keyID).callback()(keyID, client);
	});
}

void ServerCommandHandler::setPlayerPosition(u16 clientID, s32 x, s32 y, s32 z) {
	ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (player) {
		player->setPosition(x, y, z);
		player->resetSurvival();
	}
	else
		logError() << ("Failed to set position for player " + std::to_string(clientID) + ": Player not found").c_str();
}

inline ServerWorld &ServerCommandHandler::getWorldForClient(u16 clientID) {
	ServerPlayer *player = m_players.getPlayerFromClientID(clientID);
	if (!player)
		throw EXCEPTION("Player instance not found for client", clientID);

	return m_worldController.getWorld(player->dimension());
}

// Please update 'docs/lua-api-cpp.md' if you change this
void ServerCommandHandler::initUsertype(sol::state &lua) {
	lua.new_usertype<ServerCommandHandler>("ServerCommandHandler",
		"send_player_change_dimension", &ServerCommandHandler::sendPlayerChangeDimension,
		"send_player_health", &ServerCommandHandler::sendPlayerHealth,
		"use_portal", &ServerCommandHandler::usePortal,
		"use_bed", &ServerCommandHandler::useBed,
		"consume_held_item", &ServerCommandHandler::consumeHeldItem,
		"wear_held_item", &ServerCommandHandler::wearHeldItem,
		"damage_player", &ServerCommandHandler::damagePlayer,
		"send_player_inv_update", [](ServerCommandHandler &server, u16 clientID) { server.sendPlayerInvUpdate(clientID); }
	);
}
