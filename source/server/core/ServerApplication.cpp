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
#include <filesystem.hpp>

#include "common/core/BlockGeometry.hpp"
#include "common/core/GameTime.hpp"
#include "common/event/Events.hpp"

#include "server/core/ServerApplication.hpp"
#include "server/core/ServerConfig.hpp"
#include "server/world/ServerBlock.hpp"

namespace fs = ghc::filesystem;

ServerApplication::ServerApplication(EventHandler &eventHandler) : m_eventHandler(eventHandler) {}

ServerApplication::~ServerApplication() {
	// release lua-backed definitions before the script engine is destroyed.
	m_worldController.clearEntities();
	m_registry.clear();
}

bool ServerApplication::init() {
	std::srand((unsigned int)std::time(nullptr));

	LoggerHandler::setInstance(m_loggerHandler);
	m_loggerHandler.setName("server");

	fs::create_directory("logs");
	m_loggerHandler.openLogFile("logs/opencraft_server.log");

	BlockGeometry::initOrientation();

	ServerConfig::loadConfigFromFile("config/server.lua");

	m_server.init();
	m_server.setRunning(true);

	Registry::setInstance(m_registry);

	BlockState &blockState = m_registry.registerBlock<ServerBlock>(BLOCK_AIR).getState(0);
	blockState.label("Air");
	blockState.isOpaque(false);
	blockState.isCollidable(false);
	m_registry.registerItem<Item>({}, BLOCK_AIR, "Air").setIsBlock(true);

	m_modLoader.loadMods();

	m_serverCommandHandler.setupCallbacks();

	m_worldController.setServer(m_serverCommandHandler);
	m_worldController.init(m_players, m_seed);
	m_worldController.setCreative(m_creative);

	m_scriptEngine.luaCore().setRegistry(&m_registry);

	logInfo() << "local world simulation started";

	if (!m_worldName.empty())
		m_worldController.load(m_worldName);

	m_eventHandler.emplaceEvent<ServerOnlineEvent>(true);

	return true;
}

int ServerApplication::run() {
	std::string error;
	try {
		if (!init()) return 1;
		mainLoop();
		logInfo() << "stopping world simulation";
		if (!m_worldName.empty()) m_worldController.save(m_worldName);
	}
	catch (const Exception &e) {
		error = e.what();
	}
	catch (const std::exception &e) {
		error = e.what();
	}
	if (!error.empty()) {
		m_eventHandler.emplaceEvent<ServerOnlineEvent>(false);
		logError() << "world simulation error" << error;
	}

	ServerConfig::saveConfigToFile("config/server.lua");
	ServerConfig::options.clear();
	m_serverCommandHandler.sendServerClosed(error.empty() ? "world closed" : "world error: " + error);
	m_server.disconnectAllClients();
	return error.empty() ? 0 : 1;
}

void ServerApplication::update() {
	bool doTick = false;
	if (m_lastTick < m_clock.getTicks() / 50) {
		m_lastTick = m_clock.getTicks() / 50;
		doTick = !m_server.isPaused();
		if (doTick) GameTime::incrementTicks();

	}

	m_worldController.update(doTick);

	if (m_clock.getTicks() % 100 < 10) {
		for (auto &it : m_players) {
			if (it.second.isOnline()) {
				m_serverCommandHandler.sendPlayerPosUpdate(it.second.clientID());
			}
		}
	}
}

void ServerApplication::mainLoop() {
	while (m_server.isRunning()) {
		m_server.handleGameEvents();

		m_clock.updateGame([this] {
			update();
		});

		m_clock.waitForNextFrame();
	}
}
