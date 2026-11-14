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
#include <fstream>
#include <map>
#include "common/world/Dimension.hpp"
#include "common/world/WorldFiles.hpp"
#include "server/world/save/ChunkDatabase.hpp"

#include <filesystem.hpp>

#include <stdexcept>

#include "common/core/Debug.hpp"
#include "common/core/GameTime.hpp"
#include "common/core/Registry.hpp"
#include "common/network/Network.hpp"
#include "common/scene/component/ComponentType.hpp"
#include "common/scene/component/MobComponent.hpp"
#include "common/scene/component/PhysicalComponent.hpp"
#include "common/scene/component/FallingBlockComponent.hpp"
#include "common/scene/component/TntComponent.hpp"
#include "common/scene/component/MinecartComponent.hpp"
#include "common/scene/component/CargoMinecartComponent.hpp"
#include "common/scene/component/RareMobComponent.hpp"
#include "common/scene/component/NetworkComponent.hpp"
#include "server/world/ServerBlock.hpp"
#include "server/world/ServerWorld.hpp"
#include "server/world/save/WorldSaveBasicBackend.hpp"

namespace fs = ghc::filesystem;

using namespace entt::literals;

namespace {
using Overflow = std::vector<std::pair<Vector3d, ItemStack>>;

sf::Packet saveChunk(const ServerChunk &chunk) {
	auto &registry = Registry::getInstance();
	std::map<u16, u16> palette;
	for (int z = 0; z < CHUNK_HEIGHT; ++z) for (int y = 0; y < CHUNK_DEPTH; ++y) for (int x = 0; x < CHUNK_WIDTH; ++x)
		palette.emplace(chunk.getBlock(x, y, z), 0);
	sf::Packet packet;
	packet << u8(1) << u16(palette.size());
	u16 index = 0;
	for (auto &entry : palette) { entry.second = index++; packet << registry.getBlock(entry.first).stringID(); }
	for (int z = 0; z < CHUNK_HEIGHT; ++z) for (int y = 0; y < CHUNK_DEPTH; ++y) for (int x = 0; x < CHUNK_WIDTH; ++x)
		packet << palette.at(chunk.getBlock(x, y, z)) << chunk.getData(x, y, z);
	std::map<u16, const BlockData *> metadata;
	for (const auto &entry : chunk.blockData()) {
		const auto &position = entry.first;
		metadata.emplace(u16((position.z * CHUNK_DEPTH + position.y) * CHUNK_WIDTH + position.x), entry.second.get());
	}
	packet << u16(metadata.size());
	for (const auto &entry : metadata)
		packet << entry.first << entry.second->inventory << entry.second->meta;
	return packet;
}

void loadChunk(sf::Packet &packet, ServerChunk &chunk, Overflow &overflow) {
	u8 version = 0;
	u16 size = 0;
	packet >> version >> size;
	if (!packet || version != 1 || !size || size > CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT)
		throw std::runtime_error("unsupported saved chunk");
	auto &registry = Registry::getInstance();
	std::vector<u16> palette(size);
	for (auto &id : palette) {
		std::string name; packet >> name;
		id = registry.hasBlock(name) ? registry.getBlockFromStringID(name).id() : 0;
	}
	for (int z = 0; z < CHUNK_HEIGHT; ++z) for (int y = 0; y < CHUNK_DEPTH; ++y) for (int x = 0; x < CHUNK_WIDTH; ++x) {
		u16 id = 0, data = 0;
		if (!(packet >> id >> data) || id >= palette.size()) throw std::runtime_error("damaged chunk palette");
		const auto &block = registry.getBlock(palette[id]);
		if (block.param().hasParam(BlockParam::State) && block.param().getParam(BlockParam::State, data) >= block.states().size()) data = 0;
		chunk.setDataRaw(x, y, z, u32(palette[id]) | u32(data) << 16);
	}
	u16 count = 0;
	packet >> count;
	if (!packet || count > CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT) throw std::runtime_error("invalid chunk metadata count");
	for (u16 i = 0; i < count; ++i) {
		u16 pos = 0;
		if (!(packet >> pos) || pos >= CHUNK_WIDTH * CHUNK_DEPTH * CHUNK_HEIGHT) throw std::runtime_error("invalid chunk metadata position");
		int x = pos % CHUNK_WIDTH, y = pos / CHUNK_WIDTH % CHUNK_DEPTH, z = pos / (CHUNK_WIDTH * CHUNK_DEPTH);
		auto *data = chunk.addBlockData(x, y, z);
		packet >> data->inventory >> data->meta;
		for (const auto &stack : data->inventory.normalizeStacks())
			overflow.emplace_back(Vector3d{double(chunk.x() * CHUNK_WIDTH + x) + 0.5,
				double(chunk.y() * CHUNK_DEPTH + y) + 0.5, double(chunk.z() * CHUNK_HEIGHT + z) + 0.5}, stack);
	}
	if (!packet || !packet.endOfPacket()) throw std::runtime_error("damaged saved chunk");
	chunk.setInitialized(true);
	chunk.setModified(true);
	chunk.setSaved();
}
}

void WorldSaveBasicBackend::load(const std::string &name) {
	logInfo() << ("Loading '" + name + "'...").c_str();

	const std::string path = worldFiles::readable(name).string();
	if (!fs::exists(path)) return;
	std::unique_ptr<ChunkDatabase> database;
	Network::Packet save;
	if (ChunkDatabase::isDatabase(path)) {
		database = std::make_unique<ChunkDatabase>(path, true);
		save = database->readState();
	} else {
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file || file.tellg() <= 0) throw std::runtime_error("unable to read world save");
		std::vector<char> buffer(static_cast<size_t>(file.tellg()));
		file.seekg(0);
		if (!file.read(buffer.data(), buffer.size())) throw std::runtime_error("incomplete world save");
		save.append(buffer.data(), buffer.size());
	}
	for (auto &world : m_worldList) world.setGenerationRevision(0);
	{

		for (auto &world : m_worldList) {
			std::vector<std::pair<Vector3d, ItemStack>> overflow;
			unsigned int chunkCount;
			s32 seed;
			save >> chunkCount >> seed;

			if (!save || chunkCount > 1000000) throw std::runtime_error("invalid world header");
			world.setSeed(seed);
			if (database) database->readChunks(world.dimension().id(), [&](int x, int y, int z, sf::Packet &packet) {
				auto &chunk = world.getOrCreateChunk(x, y, z);
				loadChunk(packet, chunk, overflow);
				world.blockPhysics().seedChunk(chunk);
			});

			// logInfo() << "Loading dimension" << world.dimension().id() << "| Chunk count:" << chunkCount;

			for (unsigned int i = 0 ; i < chunkCount ; ++i) {
				int cx, cy, cz;
				if (!(save >> cx >> cy >> cz)) throw std::runtime_error("incomplete saved chunk");

				ServerChunk &chunk = world.getOrCreateChunk(cx, cy, cz);
				for (u8 z = 0 ; z < Chunk::height ; ++z) {
					for (u8 y = 0 ; y < Chunk::depth ; ++y) {
						for (u8 x = 0 ; x < Chunk::width ; ++x) {
							bool hasMetadata = false;
							if (!(save >> hasMetadata)) throw std::runtime_error("incomplete saved block");

							if (hasMetadata) {
								BlockData *blockData = chunk.addBlockData(x, y, z);
								save >> blockData->inventory >> blockData->meta;
								for (const auto &stack : blockData->inventory.normalizeStacks())
									overflow.emplace_back(Vector3d{double(cx * CHUNK_WIDTH + x) + 0.5,
										double(cy * CHUNK_DEPTH + y) + 0.5, double(cz * CHUNK_HEIGHT + z) + 0.5}, stack);
							}

							u32 data;
							u8 light;
							if (!(save >> data >> light)) throw std::runtime_error("incomplete saved block");
							const auto &definitions = Registry::getInstance();
							if ((data & 0xffff) >= definitions.blocks().size() || !definitions.getBlock(u16(data & 0xffff)).id())
								data = 0;

							chunk.setDataRaw(x, y, z, data);
							// lighting is rebuilt after every saved chunk and trailer is loaded.
						}
					}
				}

				chunk.setInitialized(true);
				chunk.setModified(true);
				world.blockPhysics().seedChunk(chunk);
			}

			loadEntities(save, world);
			for (const auto &entry : overflow)
				world.dropWorldItem(entry.first, entry.second.stringID(), entry.second.amount(), entry.second.damage(), false);
		}

		u16 playerCount = 0;
		if (!(save >> playerCount)) throw std::runtime_error("missing saved players");
		for (u16 i = 0 ; i < playerCount ; ++i) {
			std::string username;
			save >> username;

			auto &player = m_playerList.addPlayer(username, false);
			player.clearBedSpawn();
			player.offhand().clearStack(0, 0);
			player.statistics() = {};
			player.deserialize(save);
			if (!save) throw std::runtime_error("incomplete saved player");
			for (const auto &stack : player.inventory().normalizeStacks())
				if (player.dimension() < m_worldList.size()) m_worldList[player.dimension()].dropWorldItem({player.x(), player.y(), player.z() + 0.5}, stack.stringID(), stack.amount(), stack.damage(), false);
		}

		sf::Uint64 ticks = 0;
		if (!(save >> ticks)) throw std::runtime_error("missing saved time");
		GameTime::setTicks(ticks);
		while (!save.endOfPacket()) {
			u32 marker = 0;
			u16 count = 0;
			if (!(save >> marker >> count)) break;
			if (marker == 0x4845414c) {
				for (u16 i = 0; i < count; ++i) {
					std::string name;
					u16 health;
					if (!(save >> name >> health)) break;
					if (auto *player = m_playerList.getPlayer(name)) player->setHealth(health);
				}
			}
			else if (marker == 0x53434f52) {
				for (u16 i = 0; i < count; ++i) {
					std::string name;
					s32 score;
					if (!(save >> name >> score)) break;
					if (auto *player = m_playerList.getPlayer(name)) player->setScore(score);
				}
			}
			else if (marker == 0x4f464648) {
				for (u16 i = 0; i < count; ++i) {
					std::string name;
					Inventory offhand;
					if (!(save >> name >> offhand)) break;
					if (auto *player = m_playerList.getPlayer(name)) {
						if (offhand.width() == 1 && offhand.height() == 1 && offhand.name() == "offhand" && !offhand.inBlock()) player->offhand() = std::move(offhand);
					}
				}
			}
			else if (marker == 0x41524d52) {
				for (u16 i = 0; i < count; ++i) {
					std::string name;
					Inventory armor;
					if (!(save >> name >> armor)) break;
					auto *player = m_playerList.getPlayer(name);
					if (!player || armor.width() != 1 || armor.height() != 4) continue;
					for (u16 slot = 0; slot < 4; ++slot) {
						const auto &stack = armor.getStack(0, slot);
						if (stack.amount() == 1 && stack.item().getGroupValue("group:om_armor_slot") == slot + 1)
							player->armor().setItemStack(0, slot, stack);
					}
				}
			}
			else if (marker == 0x42454431) {
				for (u16 i = 0; i < count; ++i) {
					std::string name;
					bool hasBed;
					s32 x, y, z;
					if (!(save >> name >> hasBed >> x >> y >> z)) break;
					if (auto *player = m_playerList.getPlayer(name)) {
						if (hasBed && std::abs(double(x)) < 30000000 && std::abs(double(y)) < 30000000
						 && std::abs(double(z)) < 30000000) player->setBedSpawn({x, y, z});
						else player->clearBedSpawn();
					}
				}
			}
			else if (marker == 0x53544154) {
				for (u16 i = 0; i < count; ++i) {
					std::string name;
					save >> name;
					PlayerStatistics statistics;
					statistics.read(save);
					if (auto *player = m_playerList.getPlayer(name)) player->statistics() = std::move(statistics);
				}
			}
			else if (marker == 0x4d4f4445 && count == 1) {
				bool creative;
				if (!(save >> creative)) break;
				for (auto &world : m_worldList) world.setCreative(creative);
			}
			else if (marker == 0x47454e52 && count == m_worldList.size()) {
				for (auto &world : m_worldList) {
					u16 revision = 0;
					if (!(save >> revision) || revision > 1) throw std::runtime_error("unsupported world generation revision");
					world.setGenerationRevision(revision);
				}
			}
			else break;
		}
		if (!save) throw std::runtime_error("incomplete world metadata");
		for (auto &world : m_worldList) {
			for (const auto &entry : world.chunks()) {
				if (!entry.second->isInitialized()) continue;
				entry.second->initializeTickingBlocks();
				world.initializeChunkLighting(*entry.second);
			}
		}
	}

	m_savedPath = database ? path : "";
}

void WorldSaveBasicBackend::save(const std::string &name) {
	logInfo() << ("Saving '" + name + "'...").c_str();

	fs::create_directory("saves");

	fs::create_directories(worldFiles::directory(name));
	const std::string path = worldFiles::database(name).string();
	bool existing = ChunkDatabase::isDatabase(path);
	if (fs::exists(path) && !existing) throw std::runtime_error("invalid world database; original left unchanged");
	std::string destination = existing ? path : path + ".new";
	if (!existing && fs::exists(destination)) fs::remove(destination);
	std::vector<ServerChunk *> written;
	{
	ChunkDatabase database(destination, false);
	database.begin();
	Network::Packet save;
	for (auto &world : m_worldList) {
		for (auto &entry : world.chunks()) {
			auto &chunk = *entry.second;
			if (!chunk.isInitialized() || !chunk.hasBeenModified()) continue;
			if (existing && m_savedPath == path && !chunk.needsSave() && !chunk.hasBlockMetadata()) continue;
			database.writeChunk(world.dimension().id(), chunk.x(), chunk.y(), chunk.z(), saveChunk(chunk));
			written.push_back(&chunk);
		}
		// the player/entity stream stays compatible with the legacy reader; chunks live in the database.
		save << u32(0) << world.seed();
		saveEntities(save, world);
	}

	save << (u16)m_playerList.size();

	for (auto &it : m_playerList) {
		save << it.second.name();
		save << it.second;
	}

	save << (sf::Uint64)GameTime::getTicks();
	save << u32(0x4845414c) << u16(m_playerList.size());
	for (const auto &entry : m_playerList)
		save << entry.first << entry.second.health();
	save << u32(0x53434f52) << u16(m_playerList.size());
	for (const auto &entry : m_playerList)
		save << entry.first << entry.second.score();

	save << u32(0x4f464648) << u16(m_playerList.size());
	for (const auto &entry : m_playerList) save << entry.first << entry.second.offhand();

	save << u32(0x41524d52) << u16(m_playerList.size());
	for (const auto &entry : m_playerList)
		save << entry.first << entry.second.armor();

	save << u32(0x4d4f4445) << u16(1) << (!m_worldList.empty() && m_worldList.front().isCreative());
	save << u32(0x47454e52) << u16(m_worldList.size());
	for (const auto &world : m_worldList) save << world.generationRevision();
	save << u32(0x42454431) << u16(m_playerList.size());
	for (const auto &entry : m_playerList) {
		const auto &bed = entry.second.bedSpawn();
		Vector3i position = bed.value_or(Vector3i{0, 0, 0});
		save << entry.first << bool(bed) << position.x << position.y << position.z;
	}

	save << u32(0x53544154) << u16(m_playerList.size());
	for (const auto &entry : m_playerList) {
		save << entry.first;
		entry.second.statistics().write(save);
	}

	database.writeState(save);
	database.commit();
	}
	if (!existing) {
		fs::rename(destination, path);
		const auto backup = worldFiles::directory(name) / "legacy.dat";
		if (fs::exists(worldFiles::legacy(name)) && !fs::exists(backup)) fs::rename(worldFiles::legacy(name), backup);
	}
	for (auto *chunk : written) chunk->setSaved();
	m_savedPath = path;

	// logInfo() << "Saving done.";
}

#include "common/scene/component/AnimationComponent.hpp"
#include "common/scene/component/ComponentType.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/scene/component/RotationComponent.hpp"

void WorldSaveBasicBackend::loadEntities(sf::Packet &save, ServerWorld &world) {
	entt::registry &registry = world.scene().registry();
	registry.clear();

	u32 entityCount = 0;
	if (!(save >> entityCount) || entityCount > 1000000) throw std::runtime_error("invalid saved entity count");
	m_entityMap.clear();

	// logDebug() << "Loading" << componentCount << "components in dimension" << world.dimension().id();

	for (u32 i = 0 ; i < entityCount ; ++i) {
		entt::entity entityID;
		save >> entityID;

		entt::entity entity = registry.create();
		m_entityMap.emplace(entityID, entity);
		registry.emplace<NetworkComponent>(entity, entity);

		// logDebug() << "Creating entity" << std::underlying_type_t<entt::entity>(entityID);

		u32 componentCount = 0;
		if (!(save >> componentCount) || componentCount > 64) throw std::runtime_error("invalid saved component count");

		for (u32 i = 0 ; i < componentCount ; ++i) {
			ComponentType type;
			save >> type;

			// logDebug() << "Loading component" << (u16)type << "for entity" << std::underlying_type_t<entt::entity>(entityID);

			if (type == ComponentType::Position) {
				save >> registry.emplace<PositionComponent>(entity);
			}
			else if (type == ComponentType::Rotation) {
				save >> registry.emplace<RotationComponent>(entity);
			}
			else if (type == ComponentType::Animation) {
				save >> registry.emplace<AnimationComponent>(entity);
			}
			else if (type == ComponentType::Drawable) {
				save >> registry.emplace<DrawableDef>(entity);
			}
			else if (type == ComponentType::ItemStack) {
				save >> registry.emplace<ItemStack>(entity);
			}
			else if (type == ComponentType::Hitbox) {
				save >> registry.emplace<DoubleBox>(entity);
			}
			else if (type == ComponentType::FallingBlock) {
				save >> registry.emplace<FallingBlockComponent>(entity);
			}
			else if (type == ComponentType::RareMob) {
				save >> registry.emplace<RareMobComponent>(entity);
			}
			else if (type == ComponentType::CargoMinecart) {
				save >> registry.emplace<CargoMinecartComponent>(entity);
			}
			else if (type == ComponentType::Minecart) {
				save >> registry.emplace<MinecartComponent>(entity);
			}
			else if (type == ComponentType::Tnt) {
				save >> registry.emplace<TntComponent>(entity);
			}
			else if (type == ComponentType::Physical) {
				save >> registry.emplace<PhysicalComponent>(entity);
			}
			else if (type == ComponentType::BetaMob) {
				save >> registry.emplace<BetaMobComponent>(entity);
			}
			else if (type == ComponentType::Mob) {
				save >> registry.emplace<MobComponent>(entity);
			}
			else if (type == ComponentType::EntityID) {
				save >> registry.emplace<std::string>(entity);
			}
			else
				throw std::runtime_error("unknown saved entity component");
			if (!save) throw std::runtime_error("incomplete saved entity");
		}
		const auto *stack = registry.try_get<ItemStack>(entity);
		const auto *falling = registry.try_get<FallingBlockComponent>(entity);
		const auto &definitions = Registry::getInstance();
		if ((stack && !definitions.hasItem(stack->stringID())) || (falling &&
			(falling->block >= definitions.blocks().size() || !definitions.getBlock(falling->block).id())))
			registry.destroy(entity);
	}
}

void WorldSaveBasicBackend::saveEntities(sf::Packet &save, ServerWorld &world) {
	entt::registry &registry = world.scene().registry();

	auto view = registry.view<NetworkComponent>();
	save << (u32)view.size();

	view.each([&] (auto entity, auto &network) {
		u32 componentCount = 0;
		Network::Packet entityPacket;
		registry.visit(entity, [&] (const auto &component_type) {
			const auto &type = entt::resolve(component_type);
			if (type.prop("is_savable"_hs).value().template cast<bool>()) {
				entityPacket << type.prop("type"_hs).value().template cast<ComponentType>();

				const auto &component = type.func("get"_hs).invoke({}, std::ref(registry), entity);
				Network::Packet packet = type.func("save"_hs).invoke({}, component).template cast<Network::Packet>();
				entityPacket.append(packet.getData(), packet.getDataSize());

				// logDebug() << "Serializing component" << type.prop("name"_hs).value().template cast<std::string>() << "for entity" << std::underlying_type_t<entt::entity>(entity) << "of size" << packet.getDataSize();

				++componentCount;
			}
		});

		// logDebug() << "Saving" << componentCount << "components for entity" << std::underlying_type_t<entt::entity>(entity);

		save << network.entityID << componentCount;
		save.append(entityPacket.getData(), entityPacket.getDataSize());
	});

	// logDebug() << "Saving" << view.size() << "entities in dimension" << world.dimension().id();
}
