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
#include "common/core/EngineConfig.hpp"
#include "common/core/Utils.hpp"

#include "client/core/ClientApplication.hpp"
#include "client/core/Config.hpp"
#include "client/core/input/GamePad.hpp"
#include "client/graphics/TextureAtlas.hpp"
#include "client/graphics/Vertex.hpp"
#include "client/gui/Font.hpp"
#include "client/resource/TextureLoader.hpp"

#include "client/states/TitleScreenState.hpp"
#include "client/states/WorldSelectionState.hpp"

namespace fs = ghc::filesystem;

ClientApplication::ClientApplication(int argc, char **argv) : CoreApplication(argc, argv) {
	BlockGeometry::initOrientation();
}

bool ClientApplication::init() {
	m_argumentParser.addArgument("singleplayer", {"-s", "--singleplayer", "open local world selection."});
	m_argumentParser.addArgument("world", {"", "--world", "open a named local world.", "name"});
	m_argumentParser.addArgument("working-dir",  {"-w", "--working-dir",  "Change the working directory to <dir>.", "dir"});
	m_argumentParser.addArgument("log-level",    {"-l", "--log-level",    "Choose the log level (debug, info, warning, error)", "level"});

	m_loggerHandler.setName("client");

	fs::create_directory("logs");
	m_loggerHandler.openLogFile("logs/opencraft.log");

	CoreApplication::init();

	if (m_argumentParser.getArgument("help").isFound)
		return true;

	if (m_argumentParser.getArgument("log-level").isFound) {
		std::unordered_map<std::string, LogLevel> levels = {
			{"debug",   LogLevel::Debug},
			{"info",    LogLevel::Info},
			{"warning", LogLevel::Warning},
			{"error",   LogLevel::Error},
		};

		auto it = levels.find(m_argumentParser.getArgument("log-level").parameter);
		if (it != levels.end())
			m_loggerHandler.setMaxLevel(it->second);
		else
			logWarning() << ("Failed to set log level to '" + m_argumentParser.getArgument("log-level").parameter + "': Invalid value").c_str();
	}

	if (m_argumentParser.getArgument("working-dir").isFound)
		fs::current_path(m_argumentParser.getArgument("working-dir").parameter);

	Config::loadConfigFromFile("config/client.lua");

	m_keyboardHandler.loadKeysFromFile("config/keys.lua");
	GamePad::init(m_keyboardHandler);

	createWindow(Config::screenWidth, Config::screenHeight, APP_NAME);
	m_window.setVerticalSyncEnabled(Config::isVerticalSyncEnabled);
	m_window.disableView();
	m_soundManager.init("resources/config/sounds.xml");
	m_soundManager.setVolume(Config::soundVolume);

	if (Config::isWireframeModeEnabled)
		bgfx::setDebug(BGFX_DEBUG_WIREFRAME);

	m_resourceHandler.loadConfigFile<TextureLoader>("resources/config/textures.xml");
	m_resourceHandler.add<Font>("font-ascii", "texture-font", "resources/textures/font.properties");
	m_resourceHandler.add<TextureAtlas>("atlas-blocks");

#ifdef OM_PROFILER_ENABLED
	ClientProfiler::setInstance(&m_profiler);
#endif

	auto &titleScreen = m_stateStack.push<TitleScreenState>();
	const Argument &world = m_argumentParser.getArgument("world");
	if (world.isFound && utils::regexMatch(world.parameter, "^[A-Za-z0-9][A-Za-z0-9_ ]*$"))
		titleScreen.startSingleplayer(true, world.parameter);
	else if (world.isFound || m_argumentParser.getArgument("singleplayer").isFound)
		m_stateStack.push<WorldSelectionState>(&titleScreen);

	return true;
}

void ClientApplication::handleEvents() {
	m_soundManager.setVolume(Config::soundVolume);
	OM_PROFILE_START("OS events");

	CoreApplication::handleEvents();

	if ((Config::isFullscreenModeEnabled && m_window.getWindowMode() != Window::Mode::Fullscreen)
	|| (!Config::isFullscreenModeEnabled && m_window.getWindowMode() != Window::Mode::Windowed)) {
		m_window.setWindowMode(Config::isFullscreenModeEnabled ? Window::Mode::Fullscreen : Window::Mode::Windowed);
	}

	if (Config::screenWidth != m_window.getSize().x || Config::screenHeight != m_window.getSize().y) {
		m_window.resize(Config::screenWidth, Config::screenHeight);
	}

	if (Config::isVerticalSyncEnabled != m_window.isVerticalSyncEnabled())
		m_window.setVerticalSyncEnabled(Config::isVerticalSyncEnabled);

	OM_PROFILE_END("OS events");
}

void ClientApplication::onEvent(const SDL_Event &event) {
	CoreApplication::onEvent(event);

	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11)
		Config::isFullscreenModeEnabled ^= 1;
}

void ClientApplication::onExit() {
	Config::saveConfigToFile("config/client.lua");

	m_keyboardHandler.saveKeysToFile("config/keys.lua");
}

void ClientApplication::mainLoop() {
	m_clock.startFpsTimer();

	// FIXME: The window should probably be closed after the main loop ends
	while(m_window.isOpen() && !m_stateStack.empty() && !hasBeenInterrupted) {
		OM_PROFILE_BEGIN_TICK();

		handleEvents();

		m_eventHandler.processEvents();

		m_clock.updateGame([&] {
			OM_PROFILE_START("Update");

			if (!m_stateStack.empty())
				m_stateStack.top().update();

			m_stateStack.clearDeletedStates();

			OM_PROFILE_END("Update");
		});

		m_clock.drawGame([&] {
			m_window.clear();

			OM_PROFILE_START("Draw");

			if(!m_stateStack.empty())
				m_window.draw((const DrawableState &)m_stateStack.top(), m_renderStates);

			OM_PROFILE_END("Draw");

			m_window.display();
		});

		OM_PROFILE_END_TICK();
	}
}
