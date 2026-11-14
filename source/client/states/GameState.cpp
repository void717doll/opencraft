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
#include <glm/gtc/matrix_transform.hpp>

#include "common/core/ApplicationStateStack.hpp"
#include "common/core/Exception.hpp"
#include "common/core/GameClock.hpp"
#include "common/core/GameTime.hpp"
#include "common/core/Registry.hpp"
#include "common/core/input/GameKey.hpp"
#include "common/event/Events.hpp"
#include "common/resource/ResourceHandler.hpp"

#include "client/audio/SoundManager.hpp"
#include "client/core/KeyboardHandler.hpp"
#include "client/core/Mouse.hpp"
#include "client/core/input/GamePad.hpp"
#include "client/graphics/BgfxView.hpp"
#include "client/graphics/RenderTarget.hpp"
#include "client/graphics/TextureAtlas.hpp"
#include "client/states/GameState.hpp"
#include "client/states/DeathState.hpp"
#include "client/states/LuaGUIState.hpp"
#include "client/states/PauseMenuState.hpp"

GameState::GameState()
	: m_textureAtlas(ResourceHandler::getInstance().get<TextureAtlas>("atlas-blocks"))
{
	Registry::setInstance(m_registry);

	initShaders();

	m_clientCommandHandler.setupCallbacks();

	m_camera.setAspectRatio((float)Config::screenWidth / Config::screenHeight);
	m_camera.setFarClippingPlane(1000.0f);

	m_world.setClient(m_clientCommandHandler);
	m_world.setCamera(m_player.camera());

	m_keyboardHandler = dynamic_cast<KeyboardHandler *>(GamePad::getInputHandler());

	m_skyColor.init("u_skyColor", bgfx::UniformType::Vec4);
	m_sunlightIntensity.init("u_sunlightIntensity", bgfx::UniformType::Vec4);

	m_fbo.init(Config::screenWidth, Config::screenHeight);
}

GameState::~GameState() {
	if (auto *sound = SoundManager::instance()) { sound->stopRecord(); sound->setPaused(true); }
	m_client.disconnect();
}

void GameState::init() {
	m_world.setEventHandler(*m_eventHandler);

	m_eventHandler->addListener<GuiScaleChangedEvent>(&GameState::onGuiScaleChanged, this);

	m_eventHandler->addListener<ChunkCreatedEvent>(&Minimap::onChunkCreatedEvent, &m_hud.minimap());
	m_eventHandler->addListener<ChunkRemovedEvent>(&Minimap::onChunkRemovedEvent, &m_hud.minimap());
}

void GameState::onStateInactive() {
	resetSpyglass();
	m_player.clearInputs();
	m_hud.pause();
}

bool GameState::holdingSpyglass() {
	int slot = m_player.heldItemSlot();
	if (slot < 0 || slot >= m_player.inventory().width()) return false;
	const ItemStack &stack = m_player.useItemStack();
	return stack.amount() && stack.stringID() == "default:spyglass";
}

void GameState::resetSpyglass() {
	m_spyglass.reset();
	m_camera.setFieldOfView(Config::cameraFOV);
	m_hud.setSpyglass(false);
}

void GameState::connect() {
	m_player.setName("Player");
	m_client.connect(m_player);
	m_player.setClientID(m_client.id());

	Mouse::setCursorVisible(false);
	Mouse::setCursorGrabbed(true);
}

void GameState::onEvent(const SDL_Event &event) {
	if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_RIGHT) resetSpyglass();
	if (event.type == SDL_QUIT) {
		m_client.disconnect();

		m_stateStack->clear();
	}

	if (!m_stateStack->empty() && &m_stateStack->top() == this) {
		KeyboardHandler *keyboardHandler = (KeyboardHandler *)GamePad::getInputHandler();
		if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT && holdingSpyglass() && m_player.health()) {
			m_spyglass.begin(m_player.heldItemSlot(), m_player.dimension());
			m_hud.pause();
			return;
		}
		if (m_spyglass.active() && event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) return;

		if (event.type == SDL_MOUSEMOTION) {
			if(Config::screenWidth / 2 != event.motion.x || Config::screenHeight / 2 != event.motion.y) {
				float sensitivity = Config::mouseSensitivity * (m_spyglass.active() ? 8.f / Config::cameraFOV : 1.f);
				m_player.turnH((float)event.motion.xrel * -0.01f * sensitivity);
				m_player.turnViewV((float)event.motion.yrel * -0.01f * sensitivity);

				Mouse::resetToWindowCenter();
			}
		}
		else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
			m_stateStack->push<PauseMenuState>(m_client, m_player, this);
		}
		else if (event.type == SDL_WINDOWEVENT) {
			if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
				m_stateStack->push<PauseMenuState>(m_client, m_player, this);

				Mouse::setCursorGrabbed(false);
				Mouse::setCursorVisible(true);
			}
			else if (event.type == SDL_WINDOWEVENT_FOCUS_GAINED) {
				Mouse::setCursorGrabbed(true);
				Mouse::setCursorVisible(false);
			}
		}
		else if (event.type == SDL_KEYDOWN) {
			if (!event.key.repeat && event.key.keysym.sym == keyboardHandler->getKeycode(GameKey::SwapHands)) m_clientCommandHandler.sendPlayerSwapHands();
			if (!event.key.repeat && event.key.keysym.sym == keyboardHandler->getKeycode(GameKey::Noclip)) {
				m_player.toggleNoclip();
				m_clientCommandHandler.sendPlayerNoclip(m_player.isNoclip());
			}
			if (!event.key.repeat && event.key.keysym.sym == keyboardHandler->getKeycode(GameKey::Jump)) m_player.pressJump();
			if (!event.key.repeat && event.key.keysym.sym == keyboardHandler->getKeycode(GameKey::Drop)) {
				m_clientCommandHandler.sendPlayerDropItem();
			}
			else if (event.key.keysym.sym == SDLK_F2) {
				std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

				char filename[100];
				std::strftime(filename, sizeof(filename), "Screenshot-%Y-%m-%d-%H-%M-%S", std::localtime(&now));

				Window::saveScreenshot(filename);
				logInfo() << "screenshot saved:" << std::string(filename) + ".png";
			}

			for (auto &key : Registry::getInstance().keys()) {
				if (event.key.keysym.sym == key.keycode()) {
					m_clientCommandHandler.sendKeyPressed(key.id());
				}
			}
		}

		m_hud.onEvent(event);
	}

	if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
		if (event.window.data1 != 0 && event.window.data2 != 0) {
			Config::screenWidth = (u16)event.window.data1;
			Config::screenHeight = (u16)event.window.data2;

			m_camera.setAspectRatio((float)Config::screenWidth / Config::screenHeight);
			m_hud.setup();

			m_fbo.init(Config::screenWidth, Config::screenHeight);
		}
	}
}

void GameState::update() {
	if (!m_player.health() && !m_stateStack->empty() && &m_stateStack->top() == this)
		m_stateStack->push<DeathState>(m_clientCommandHandler, m_player, this);
	bool paused = false;
	for (ApplicationState *state = m_stateStack->empty() ? nullptr : &m_stateStack->top(); state && state != this; state = state->parent())
		if (dynamic_cast<PauseMenuState *>(state)) paused = true;
	if (paused != m_paused) {
		m_clientCommandHandler.sendPlayerPause(paused);
		m_paused = paused;
	}
	if (auto *sound = SoundManager::instance()) {
		sound->setPaused(paused);
		sound->setListener(m_player.physicsPosition(), {m_player.dirTargetedX(), m_player.dirTargetedY(), m_player.dirTargetedZ()});
	}
	m_world.checkPlayerChunk(m_player.x(), m_player.y(), m_player.z());
	m_world.update(!m_stateStack->empty() && (&m_stateStack->top() == this || m_stateStack->top().parent() == this));

	if (!m_stateStack->empty() && &m_stateStack->top() == this) {
		m_player.processInputs();
	}

	if (!m_areModKeysLoaded) {
		for (auto &it : Registry::getInstance().keys()) {
			m_keyboardHandler->addKey(it.id(), it.name(), it.keycode(), it.stringID(), &it);
		}

		m_areModKeysLoaded = true;
	}

	if (paused) m_player.pauseMovement();
	else m_player.updatePosition(m_world);

	m_hud.update();

	if (GameClock::getInstance().getTicks() % 100 < 10) {
		m_clientCommandHandler.sendPlayerRotUpdate();
	}

	m_client.update();
	m_spyglass.update(!m_stateStack->empty() && &m_stateStack->top() == this && m_player.health(),
		holdingSpyglass(), m_player.heldItemSlot(), m_player.dimension());
	float fov = m_spyglass.fieldOfView(Config::cameraFOV);
	if (m_camera.getFieldOfView() != fov) m_camera.setFieldOfView(fov);
	m_hud.setSpyglass(m_spyglass.active());

	static const Sky *sky = nullptr;
	if (sky != m_world.sky() && m_world.sky()) {
		sky = m_world.sky();
		m_skybox.loadSky(*sky);
	}
}

void GameState::initShaders() {
	m_shader.loadFromFile("game");

	m_fbo.loadShader("screen");
}

void GameState::onGuiScaleChanged(const GuiScaleChangedEvent &event) {
	m_hud.onGuiScaleChanged(event);
}

void GameState::draw(RenderTarget &target, RenderStates states) const {
	if (m_world.sky()) {
		Color color = m_world.sky()->color();
		float sunlightIntensity = 1.f;

		if (m_world.sky()->daylightCycleSpeed() > 0.f) {
			float time = GameTime::getCurrentTime(0, m_world.sky()->daylightCycleSpeed());
			color = GameTime::getSkyColorFromTime(*m_world.sky(), time);
			sunlightIntensity = GameTime::getSunlightIntensityFromTime(time);
		}

		u32 iColor = (color.r255() << 24)
		           | (color.g255() << 16)
		           | (color.b255() << 8)
		           | color.a255();

		bgfx::setViewClear(BgfxView::Sky, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, iColor);

		m_skyColor.setValue(color, true);

		m_sunlightIntensity.setValue(sunlightIntensity);
	}

	states.shader = &m_shader;

	target.setView(m_camera);

	m_fbo.prepareDraw();

	states.view = BgfxView::Sky;
	{
		target.draw(m_skybox, states);
	}

	states.view = BgfxView::World;
	{
		target.draw(m_world, states);

		target.draw(m_hud.blockCursor(), states);
	}

	states.view = BgfxView::HUD;
	{
		target.draw(m_hud, states);
	}

	m_fbo.draw();
}
