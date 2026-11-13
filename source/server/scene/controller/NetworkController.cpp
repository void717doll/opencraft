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
#include "common/network/Network.hpp"
#include "common/scene/component/NetworkComponent.hpp"
#include "common/scene/component/AnimationComponent.hpp"
#include "common/scene/component/DrawableDef.hpp"
#include "common/scene/component/PositionComponent.hpp"
#include "common/scene/component/RotationComponent.hpp"
#include "server/network/Server.hpp"
#include "server/network/ServerCommandHandler.hpp"
#include "server/scene/controller/NetworkController.hpp"

using namespace entt::literals;

void NetworkController::update(entt::registry &registry) {
	registry.view<NetworkComponent>().each([&] (auto entity, auto &network) {
		if (!network.hasSpawned) {
			m_server->sendEntitySpawn(network.entityID);
			network.hasSpawned = true;
		}

		auto send = [&](auto *component) {
			if (!component || !component->isUpdated) return;
			Network::Packet packet;
			packet << component->packetType << entity << *component;
			m_server->server().sendToAllClients(packet);
			component->isUpdated = false;
		};
		send(registry.try_get<PositionComponent>(entity));
		send(registry.try_get<RotationComponent>(entity));
		send(registry.try_get<AnimationComponent>(entity));
		send(registry.try_get<DrawableDef>(entity));
		if (const auto *box = registry.try_get<DoubleBox>(entity)) {
			const auto &last = network.lastHitbox;
			if (!network.hasHitbox || box->x != last.x || box->y != last.y || box->z != last.z
			 || box->sizeX != last.sizeX || box->sizeY != last.sizeY || box->sizeZ != last.sizeZ) {
				Network::Packet packet;
				packet << Network::Command::EntityHitbox << entity << *box;
				m_server->server().sendToAllClients(packet);
				network.lastHitbox = *box;
				network.hasHitbox = true;
			}
		}
	});
}

void NetworkController::sendEntities(entt::registry &registry, const ClientInfo &client) {
	registry.view<NetworkComponent>().each([&] (auto entity, auto &network) {
		m_server->sendEntitySpawn(network.entityID, &client);

		// logDebug() << "sendEntitySpawn" << std::underlying_type_t<entt::entity>(network.entityID);

		registry.visit(entity, [&] (const auto &component_type) {
			const auto &type = entt::resolve(component_type);
			if (type.prop("is_serializable"_hs).value().template cast<bool>()) {
				const auto &component = type.func("get"_hs).invoke({}, std::ref(registry), entity);
				Network::Packet packet = type.func("serialize"_hs).invoke({}, entity, component, false).template cast<Network::Packet>();
				if (packet.getDataSize()) {
					// logDebug() << "sendEntities: Serializing component" << type.prop("name"_hs).value().template cast<std::string>() << "for entity" << std::underlying_type_t<entt::entity>(entity) << "of size" << packet.getDataSize();
					client.send(packet);
				}
			}
		});
	});
}
