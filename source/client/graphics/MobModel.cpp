#include <glm/gtc/matrix_inverse.hpp>

#include "common/core/Exception.hpp"
#include "common/core/Registry.hpp"
#include "common/core/GameClock.hpp"
#include "common/core/GameTime.hpp"
#include "common/scene/RetroMobMesh.hpp"
#include "client/graphics/MobModel.hpp"
#include "client/graphics/RenderTarget.hpp"
#include "client/graphics/Vertex.hpp"

namespace {

void loadTexture(Texture &texture, const std::string &name) {
	std::string path = "mods/default/textures/mobs/" + name + ".png";
	if (texture.filename() == path) return;
	texture.free();
	texture.loadFromFile(path);
}

}

MobModel::MobModel(unsigned model, float scale, u8 appearance) {
	for (auto &vertices : m_vertices) vertices.setupDefaultLayout();
	configure(model, scale, appearance);
}

void MobModel::configure(unsigned model, float scale, u8 appearance) {
	static const char *names[] = {"", "pig", "zombie", "cow", "sheep", "chicken", "skeleton", "creeper",
		"spider", "slime", "ghast", "pig_zombie", "wolf", "squid", "zombie", "arrow", "fireball", "ocelot", "bat", "axolotl_brown", "rabbit_brown", "snow_golem", "snowball", "egg"};
	if (model == 0 || (model >= std::size(names) && model != 32)) throw EXCEPTION("unknown mob model", model);
	m_model = model;
	m_scale = std::isfinite(scale) && scale > 0 ? scale : 1.f;
	m_appearance = appearance;
	std::string name = model == 32 ? "minecart" : names[model];
	if (model == 12) name = appearance & 2 ? "wolf_tame" : appearance & 1 ? "wolf_angry" : "wolf";
	if (model == 10 && (appearance & 1)) name = "ghast_shooting";
	if (model == 19) {
		static const char *colors[] = {"brown", "yellow", "green", "pink", "black", "purple", "white"};
		name = "axolotl_" + std::string(colors[appearance % std::size(colors)]);
	}
	if (model == 20) {
		static const char *colors[] = {"brown", "gold", "white", "white_splotched", "salt", "black"};
		name = "rabbit_" + std::string(colors[appearance % std::size(colors)]);
	}
	loadTexture(m_texture, name);
	m_cargo.reset();
	if (model == 32 && (appearance & 3)) {
		m_cargo = std::make_unique<InventoryCube>(0.75f, true);
		m_cargo->setPosition(-0.375f, -0.375f, 0.25f);
		m_cargo->updateVertexBuffer(Registry::getInstance().getBlockFromStringID((appearance & 3) == 1 ? "default:chest" : "default:furnace"), (appearance & 4) ? 1 : 0);
	}
	if (model == 1 && (appearance & 1)) loadTexture(m_overlay, "pig_saddle");
	else if (model == 4) loadTexture(m_overlay, "sheep_fur");
	else if (model == 8) loadTexture(m_overlay, "spider_eyes");
	else if (model == 9) loadTexture(m_overlay, "slime");
	else if (model == 21) loadTexture(m_overlay, "snow_golem_pumpkin");
}

void MobModel::updateMotion(const Vector3d &position) {
	if (m_hasPosition) {
		double dx = position.x - m_lastPosition.x, dy = position.y - m_lastPosition.y;
		double distance = std::sqrt(dx * dx + dy * dy);
		if (distance > 0.001) {
			m_lastMove = GameClock::getInstance().getTicks();
			m_stride += float(distance) * 4;
		}
	}
	m_lastPosition = position;
	m_hasPosition = true;
}

void MobModel::draw(RenderTarget &target, RenderStates states) const {
	static constexpr float fleece[16][3] = {{1,1,1},{0.95f,0.7f,0.2f},{0.9f,0.5f,0.85f},{0.6f,0.7f,0.95f},
		{0.9f,0.9f,0.2f},{0.5f,0.8f,0.1f},{0.95f,0.7f,0.8f},{0.3f,0.3f,0.3f},
		{0.6f,0.6f,0.6f},{0.3f,0.6f,0.7f},{0.7f,0.4f,0.9f},{0.2f,0.4f,0.8f},
		{0.5f,0.4f,0.3f},{0.4f,0.5f,0.2f},{0.8f,0.3f,0.3f},{0.1f,0.1f,0.1f}};
	if (m_cargo) {
		m_cargo->setLight(m_sunlight, m_torchlight);
		target.draw(*m_cargo, states);
	}
	u64 now = GameClock::getInstance().getTicks();
	float strength = now - m_lastMove < 200 ? 0.6f : 0.f;
	float age = float(now % 100000) / 50;
	for (unsigned layer = 0; layer < 2; ++layer) {
		auto mesh = retro::mobMesh(m_model, m_stride, strength, age, m_appearance, layer);
		if (mesh.empty()) continue;
		std::vector<Vertex> vertices;
		vertices.reserve(mesh.size());
		float scale = m_model == 10 ? m_scale * 4.5f : m_scale;
		glm::vec3 right{1,0,0}, up{0,0,1};
		if ((m_model == 16 || m_model == 22 || m_model == 23) && target.getView()) {
			const auto &view = target.getView()->getViewTransform().getMatrix();
			glm::mat3 local = glm::inverse(glm::mat3(states.transform.getMatrix()));
			right = local * glm::vec3(view[0][0],view[1][0],view[2][0]);
			up = local * glm::vec3(view[0][1],view[1][1],view[2][1]);
		}
		for (const auto &point : mesh) {
			Vertex vertex;
			vertex.coord3d[3] = m_hurt ? 7.f : 6.f;
			glm::vec3 position = (m_model == 16 || m_model == 22 || m_model == 23) ? right * point.x + up * point.z : glm::vec3(point.x,point.y,point.z);
			position *= scale;
			vertex.coord3d[0] = position.x; vertex.coord3d[1] = position.y; vertex.coord3d[2] = position.z;
			vertex.texCoord[0] = point.u; vertex.texCoord[1] = point.v;
			for (float &color : vertex.color) color = 1;
			if (layer == 1 && m_model == 4) {
				for (int channel = 0; channel < 3; ++channel) vertex.color[channel] = fleece[m_appearance & 15][channel];
			}
			if (layer == 1 && m_model == 9) vertex.color[3] = 0.5f;
			if (layer == 1 && m_model == 8)
				vertex.color[3] = (1 - GameTime::getSunlightIntensityFromTime(GameTime::getCurrentTime())) * 0.5f;
			vertex.lightValue[0] = m_sunlight;
			vertex.lightValue[1] = layer == 1 && m_model == 8 ? 15.f : m_torchlight;
			vertices.push_back(vertex);
		}
		m_vertices[layer].init(vertices.data(), u32(vertices.size() * sizeof(Vertex)), true);
		states.texture = layer ? &m_overlay : &m_texture;
		states.isCullFaceEnabled = false;
		states.isDepthTestEnabled = true;
		target.draw(m_vertices[layer], 0, u32(vertices.size()), states);
	}
}
