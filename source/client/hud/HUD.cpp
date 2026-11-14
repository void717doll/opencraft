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

#include "common/core/GameClock.hpp"
#include "common/core/GameTime.hpp"
#include "common/event/Events.hpp"

#include "client/core/Config.hpp"
#include "client/core/ClientProfiler.hpp"
#include "client/hud/HUD.hpp"
#include "client/graphics/Camera.hpp"
#include "client/network/ClientCommandHandler.hpp"
#include "client/world/ClientPlayer.hpp"

HUD::HUD(ClientPlayer &player, ClientWorld &world, ClientCommandHandler &client)
	: m_player(player), m_world(world),
	m_hotbar(player, client),
	m_wield(player),
	m_offhandWield(player, true),
	m_blockCursor(player, world, client),
	m_debugOverlay(player, world),
	m_debugLightmapViewer(player)
{
	setScale(Config::guiScale, Config::guiScale, 1);

	m_shader.loadFromFile("basic");
	m_spyglassTexture.loadFromFile("mods/default/textures/items/refi_spyglass_scope.png");
	m_spyglassScope.load(m_spyglassTexture);

	setup();
}

void HUD::setup() {
	m_view.setSize((float)Config::screenWidth, (float)Config::screenHeight);
	m_view.setCenter((float)Config::screenWidth / 2.f, (float)Config::screenHeight / 2.f);
	m_spyglassScope.setPosRect(0, 0, Config::screenWidth, Config::screenHeight);

	m_hotbar.setPosition(
		Config::screenWidth / getScale().x / 2.f - (float)m_hotbar.width() / 2.f,
		Config::screenHeight / getScale().y - (float)m_hotbar.height() - 4.f
	);

	m_hearts.setPosition(m_hotbar.getPosition().x, m_hotbar.getPosition().y - hearts::hotbarOffset);
	m_armor.setPosition(m_hotbar.getPosition().x + m_hotbar.width() - 81.f, m_hearts.getPosition().y);

	m_fpsText.setPosition(Config::screenWidth / getScale().x - 36.f, 2.f);
	m_scoreLabel.setString("Score: ");
	m_scoreLabel.updateVertexBuffer();
	m_scoreLabel.setPosition(2, 2);
	m_scoreValue.setColor(Color::fromRGBA32(255, 255, 85));
	m_scoreValue.setPosition(2.f + m_scoreLabel.getSize().x, 2);
	m_coordinatesText.setPosition(2, 12);
	m_debugOverlay.setPosition(0, Config::isCoordinatesVisible ? 24 : 12);

	m_chatText.setMaxLineLength(u32(std::max(1.f, Config::screenWidth / getScale().x - 8)));
	m_crosshair.setup();

	m_minimap.setPosition(float(Config::screenWidth / Config::guiScale - Minimap::minimapSize) - 15.f, 15.f);

	m_debugLightmapViewer.setPosition(0.f, float(Config::screenHeight / Config::guiScale - DebugLightmapViewer::totalSize));

	m_debugProfilerWindow.setPosition(2.f, 2.f);
}

void HUD::onEvent(const SDL_Event &event) {
	if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F3) {
		m_isDebugOverlayVisible ^= 1;
		m_printRendererInfo = event.key.keysym.mod & KMOD_SHIFT;
	}

	if (Config::isHotbarVisible)
		m_hotbar.onEvent(event);
	m_wield.onEvent(event);
	m_offhandWield.onEvent(event);

	m_blockCursor.onEvent(event, m_hotbar);

	if (Config::isChunkMinimapEnabled)
		m_minimap.onEvent(event);

	if (Config::isProfilerWindowEnabled)
		m_debugProfilerWindow.onEvent(event);
}

void HUD::onGuiScaleChanged(const GuiScaleChangedEvent &event) {
	setScale(event.guiScale, event.guiScale, 1);

	setup();
}

void HUD::update() {
	m_paperMap.update(m_player, m_world);
	m_world.textureAtlas().updateCompass(GameTime::getTicks(), m_player.dimension(), m_player.x(), m_player.y(), m_player.cameraYaw());
	m_scoreValue.setString(std::to_string(m_player.score()));
	if (Config::isCoordinatesVisible) {
		m_coordinatesText.setString("x: " + std::to_string(int(std::floor(m_player.x())))
			+ "  y: " + std::to_string(int(std::floor(m_player.z())))
			+ "  z: " + std::to_string(int(std::floor(m_player.y()))));
	}
	m_debugOverlay.setPosition(0, Config::isCoordinatesVisible ? 24 : 12);
	u64 now = GameClock::getInstance().getTicks();
	m_hitVisible = now < m_player.hitUntil();
	if (m_hitVisible) {
		Camera camera = m_player.camera();
		Vector3d point = m_player.hitPosition() - camera.getDPosition();
		camera.setPosition(0, 0, 0);
		glm::vec4 clip = camera.getTransform().getMatrix() * camera.getViewTransform().getMatrix()
			* glm::vec4(float(point.x), float(point.y), float(point.z), 1);
		m_hitVisible = clip.w > 0 && std::abs(clip.x) < clip.w && std::abs(clip.y) < clip.w;
		if (m_hitVisible) {
			m_hitText.setString("-" + std::to_string(m_player.hitDamage()));
			m_hitText.setColor(Color::fromRGBA32(255, 100, 100));
			m_hitText.updateVertexBuffer();
			float rise = (650.f - float(m_player.hitUntil() - now)) / 50.f;
			m_hitText.setPosition((clip.x / clip.w + 1) * Config::screenWidth / (2 * getScale().x) - m_hitText.getSize().x / 2.f,
				(1 - clip.y / clip.w) * Config::screenHeight / (2 * getScale().y) - 10 - rise);
		}
	}
	m_chatVisible = now < m_player.chatUntil();
	if (m_chatVisible) {
		std::string messages;
		for (const auto &message : m_player.chatMessages()) {
			if (!messages.empty()) messages += "\n";
			messages += message;
		}
		m_chatText.setString(messages);
		m_chatText.updateVertexBuffer();
		float y = Config::screenHeight / getScale().y - 48 - m_chatText.getSize().y;
		m_chatText.setPosition(4, y);
		m_chatBackground.setPosition(2, y - 2);
		m_chatBackground.setSize(m_chatText.getSize().x + 4, m_chatText.getSize().y + 4);
		m_chatBackground.setFillColor(Color::fromRGBA32(0, 0, 0, 150));
	}
	m_hearts.update(m_player.health(), m_player.damageSerial());
	m_armor.update(m_player.armorValue());
	m_wield.update();
	m_offhandWield.update();
	// FIXME: Shouldn't be called every tick
	if (Config::isHotbarVisible)
		m_hotbar.update();

	if (Config::isFpsCounterEnabled)
		m_fpsText.setString(std::to_string(GameClock::getInstance().getFpsAverage()) + " FPS");

	m_blockCursor.update(m_hotbar);

	if (m_isDebugOverlayVisible)
		m_debugOverlay.update(m_printRendererInfo);
	else if (Config::isProfilerWindowEnabled)
		m_debugProfilerWindow.update();

	if (Config::isChunkMinimapEnabled)
		m_minimap.update(m_player, m_world);

	if (Config::isLightmapViewerEnabled)
		m_debugLightmapViewer.update(m_world);
}

void HUD::draw(RenderTarget &target, RenderStates states) const {
	OM_PROFILE_START("HUD::draw");
	if (!m_isSpyglassActive && !m_paperMap.visible()) {
		target.draw(m_wield, states);
		target.draw(m_offhandWield, states);
	}

	target.setView(m_view);

	states.shader = &m_shader;
	if (m_isSpyglassActive) {
		RenderStates scope = states;
		scope.transform = Transform::Identity;
		target.draw(m_spyglassScope, scope);
	}

	states.transform *= getTransform();
	if (!m_isSpyglassActive) target.draw(m_paperMap, states);

	if (m_isDebugOverlayVisible)
		target.draw(m_debugOverlay, states);
	else if (Config::isProfilerWindowEnabled)
		target.draw(m_debugProfilerWindow, states);

	if (Config::isHotbarVisible)
		target.draw(m_hotbar, states);

	if (Config::isHotbarVisible) {
		if (!m_player.isCreative()) target.draw(m_hearts, states);
		target.draw(m_armor, states);
	}

	if (Config::isFpsCounterEnabled)
		target.draw(m_fpsText, states);
	target.draw(m_scoreLabel, states);
	target.draw(m_scoreValue, states);
	if (Config::isCoordinatesVisible) target.draw(m_coordinatesText, states);
	if (m_hitVisible) target.draw(m_hitText, states);
	if (m_chatVisible) {
		target.draw(m_chatBackground, states);
		target.draw(m_chatText, states);
	}

	if (Config::isChunkMinimapEnabled)
		target.draw(m_minimap, states);

	if (Config::isLightmapViewerEnabled)
		target.draw(m_debugLightmapViewer, states);

	states.transform = Transform::Identity;

	if (Config::isCrosshairVisible && !m_isSpyglassActive)
		target.draw(m_crosshair, states);

	OM_PROFILE_END("HUD::draw");
}
