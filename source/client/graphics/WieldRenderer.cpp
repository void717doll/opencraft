#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "common/core/GameClock.hpp"
#include "common/core/Registry.hpp"
#include "common/resource/ResourceHandler.hpp"
#include "common/scene/RetroMobMesh.hpp"
#include "client/graphics/BgfxView.hpp"
#include "client/graphics/ItemMeshBuilder.hpp"
#include "client/core/Config.hpp"
#include "client/graphics/RenderTarget.hpp"
#include "client/graphics/TextureAtlas.hpp"
#include "client/graphics/Vertex.hpp"
#include "client/graphics/WieldRenderer.hpp"
#include "client/gui/InventoryCube.hpp"
#include "client/world/ClientPlayer.hpp"

namespace {

constexpr float pi = 3.14159265358979323846f;
constexpr u64 swingDuration = 286;

}

WieldRenderer::WieldRenderer(ClientPlayer &player, bool offhand)
	: m_player(player), m_offhand(offhand), m_atlas(ResourceHandler::getInstance().get<TextureAtlas>("atlas-blocks")) {
	m_shader.loadFromFile("basic");
	m_skin.loadFromFile("resources/textures/player.png");
	m_armVertices.setupDefaultLayout();
	m_armorArmVertices.setupDefaultLayout();
	m_itemVertices.setupDefaultLayout();
	m_lastX = player.x();
	m_lastY = player.y();

	for (bool armor : {false, true}) {
		retro::MobMeshBuilder arm;
		arm.box(-2,0,-2,4,12,4,0,0,0,40,16,0,0,0,armor ? 1.f : 0.f);
		std::vector<Vertex> vertices;
		for (const auto &point : arm.vertices) {
			Vertex vertex;
			vertex.coord3d[0] = point.x;
			vertex.coord3d[1] = 1.125f - point.z;
			vertex.coord3d[2] = -point.y;
			vertex.texCoord[0] = point.u;
			vertex.texCoord[1] = point.v;
			for (float &color : vertex.color) color = 1;
			vertices.push_back(vertex);
		}
		(armor ? m_armorArmVertices : m_armVertices).init(vertices.data(), u32(vertices.size() * sizeof(Vertex)));
	}
}

WieldRenderer::~WieldRenderer() = default;

void WieldRenderer::pause() {
	m_attacking = false;
	m_using = false;
	m_swinging = false;
}

void WieldRenderer::onEvent(const SDL_Event &event) {
	if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) pause();
	if (event.type != SDL_MOUSEBUTTONDOWN && event.type != SDL_MOUSEBUTTONUP) return;
	bool pressed = event.type == SDL_MOUSEBUTTONDOWN;
	if (event.button.button == SDL_BUTTON_LEFT) {
		if (m_offhand) return;
		m_attacking = pressed;
	}
	else if (event.button.button == SDL_BUTTON_RIGHT) {
		if (pressed && m_player.shouldUseOffhand() != m_offhand) return;
		m_using = pressed;
	}
	else return;
	if (pressed && !m_swinging) {
		m_swingStart = GameClock::getInstance().getTicks();
		m_swinging = true;
	}
}

void WieldRenderer::setItem(const Item *item) {
	std::string id = item ? item->stringID() : "";
	if (m_itemID == id) return;
	m_itemID = id;
	m_cube.reset();
	m_itemVertexCount = 0;
	if (!item) return;
	if (item->isBlock() && Registry::getInstance().hasBlock(id)) {
		const Block &block = Registry::getInstance().getBlockFromStringID(id);
		const BlockState &state = block.getState(0);
		if (state.drawType() == BlockDrawType::XShape || !state.inventoryImage().empty()) {
			bool blockTexture = state.inventoryImage().empty();
			const std::string &filename = blockTexture ? state.tiles().getTextureForFace(0) : state.inventoryImage();
			buildItem(*item, filename, blockTexture);
		} else {
			m_cube = std::make_unique<InventoryCube>(0.3f, true);
			m_cube->updateVertexBuffer(block);
		}
	}
	else buildItem(*item, item->tiles().getTextureForFace(0), false);
}

void WieldRenderer::buildItem(const Item &item, const std::string &filename, bool blockTexture) {
	auto vertices = buildItemMesh(item, m_atlas, filename, blockTexture, 0.4f);
	m_itemVertexCount = u32(vertices.size());
	m_itemVertices.init(vertices.data(), u32(vertices.size() * sizeof(Vertex)), true);
}

void WieldRenderer::update() {
	const auto &chestplate = m_player.armor().getStack(0, 1);
	if (chestplate.amount()) {
		std::string id = chestplate.stringID();
		std::string path = "mods/" + chestplate.item().modName() + "/textures/armor/" + id.substr(id.find(':') + 1) + ".png";
		if (m_armorSkin.filename() != path) {
			m_armorSkin.free();
			m_armorSkin.loadFromFile(path);
		}
	}

	const Item *item = nullptr;
	if (m_player.heldItemSlot() >= 0) {
		const ItemStack &stack = m_offhand ? m_player.offhand().getStack(0, 0) : m_player.heldItemStack();
		if (stack.amount() && stack.item().id()) item = &stack.item();
	}
	u64 now = GameClock::getInstance().getTicks();
	float elapsed = m_lastUpdate ? std::min(float(now - m_lastUpdate) / 1000.f, 0.05f) : 0;
	m_lastUpdate = now;
	std::string selected = item ? item->stringID() : "";
	if (selected != m_itemID) {
		m_equip = std::max(0.f, m_equip - elapsed * 8);
		if (m_equip == 0) setItem(item);
	} else m_equip = std::min(1.f, m_equip + elapsed * 8);
	if (m_swinging && now - m_swingStart >= swingDuration) {
		m_swinging = m_attacking || m_using;
		m_swingStart = now;
	}
	double distance = std::hypot(m_player.x() - m_lastX, m_player.y() - m_lastY);
	m_lastX = m_player.x();
	m_lastY = m_player.y();
	if (Config::isViewBobbingEnabled && m_player.onGround() && distance > 0.0001 && distance < 1) {
		m_walkPhase += float(distance) * 1.5f;
		m_bob = std::min(1.f, m_bob + elapsed * 8);
	}
	else m_bob = std::max(0.f, m_bob - elapsed * 8);
}

void WieldRenderer::draw(RenderTarget &target, RenderStates states) const {
	if (m_offhand && !m_player.offhand().getStack(0, 0).amount()) return;
	Vector2u size = target.getSize();
	if (!size.x || !size.y) return;
	float aspect = float(size.x) / float(size.y);
	m_camera.setAspectRatio(aspect);
	target.setView(m_camera);
	bgfx::setViewClear(BgfxView::Wield, BGFX_CLEAR_DEPTH, 0, 1.f);
	bgfx::setViewMode(BgfxView::Wield, bgfx::ViewMode::Sequential);
	states.view = BgfxView::Wield;
	states.shader = &m_shader;
	states.transform = Transform::Identity;
	states.isCullFaceEnabled = false;
	states.isDepthTestEnabled = true;

	u64 now = GameClock::getInstance().getTicks();
	float progress = m_swinging ? std::min(float(now - m_swingStart) / swingDuration, 1.f) : 0;
	// adapted from lunarchy's camera.cpp (lgpl-2.1-or-later),
	// copyright (c) 2010-2013 celeron55, perttu ahola <celeron55@gmail.com>.
	// source wield units are divided by 100 and its forward z axis is reflected.
	glm::vec3 position{0.55f, -0.35f - (1 - m_equip) * 0.4f, 0.65f};
	position.x *= std::min(1.f, aspect / 1.6f);
	glm::quat rotation{glm::radians(glm::vec3{-100, 120, -100})};
	if (m_swinging) {
		position.x -= 0.5f * std::sin(std::pow(progress, 0.8f) * pi);
		position.y += 0.24f * std::sin(progress * 1.8f * pi);
		position.z += 0.125f;
		glm::quat end{glm::radians(glm::vec3{80, 30, 100})};
		rotation = glm::slerp(rotation, end, std::sin(progress * pi));
	}
	else {
		position.x -= std::sin(m_walkPhase * 2 * pi) * 0.03f * m_bob;
		position.y += std::abs(std::sin(m_walkPhase * 2 * pi)) * 0.03f * m_bob;
	}
	position.z += Config::handViewZ / 100.f;
	states.transform.translate(position.x, position.y, -position.z);
	if (m_cube) {
		states.transform.scale(1, 1, -1);
		states.transform *= Transform{glm::mat4_cast(rotation)};
		states.transform.rotateX(-90.f);
		states.transform.translate(-0.15f, -0.15f, -0.15f);
		if (m_offhand) {
			Transform mirror;
			mirror.scale(-1, 1, 1);
			states.transform = mirror * states.transform;
		}
		target.draw(*m_cube, states);
	}
	else if (m_itemVertexCount) {
		states.transform.scale(1, 1, -1);
		states.transform *= Transform{glm::mat4_cast(rotation)};
		states.texture = &m_atlas.texture();
		if (m_offhand) {
			Transform mirror;
			mirror.scale(-1, 1, 1);
			states.transform = mirror * states.transform;
		}
		target.draw(m_itemVertices, 0, m_itemVertexCount, states);
	}
	else {
		float swing = std::sin(std::sqrt(progress) * pi);
		float strike = std::sin(progress * progress * pi);
		states.transform = Transform::Identity;
		float x = std::min(aspect, 1.6f) * std::tan(36.f * pi / 180) * 1.15f * 0.58f;
		states.transform.translate(x - swing * 0.26f, -0.47f - (1 - m_equip) * 0.8f
			+ std::sin(std::sqrt(progress) * 2 * pi) * 0.1f, -1.15f - strike * 0.15f);
		states.transform.translate(0, 0, -Config::handViewZ / 100.f);
		states.transform.translate(0.1f, -0.06f, 0);
		states.transform.rotateZ(-15.f - swing * 25.f);
		states.transform.rotateY(-20.f + strike * 30.f);
		states.transform.rotateX(-35.f - swing * 45.f);
		states.texture = &m_skin;
		target.draw(m_armVertices, 0, 36, states);
		if (m_player.armor().getStack(0, 1).amount()) {
			states.texture = &m_armorSkin;
			target.draw(m_armorArmVertices, 0, 36, states);
		}
	}
}
