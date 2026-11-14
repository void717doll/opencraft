#include <algorithm>
#include <cmath>

#include "common/core/GameClock.hpp"
#include "common/core/GameTime.hpp"
#include "common/world/Block.hpp"
#include "common/world/BlockData.hpp"
#include "common/world/World.hpp"
#include "common/world/Chunk.hpp"
#include "client/graphics/ParticleRenderer.hpp"
#include "client/graphics/Camera.hpp"
#include "client/graphics/RenderTarget.hpp"
#include "client/graphics/Vertex.hpp"

ParticleRenderer::ParticleRenderer() {
	for (auto &vertices : m_vertices) vertices.setupDefaultLayout();
}

void ParticleRenderer::clear() {
	m_particles.clear(); m_anchors.clear(); m_lastScan = 0; m_lastEmit = 0;
}

void ParticleRenderer::emit(Vector3d position, Vector3f velocity, Kind kind, float size, u16 lifetime) {
	if (m_particles.size() < 192)
		m_particles.push_back({position, velocity, GameClock::getInstance().getTicks(), lifetime, kind, size});
}

void ParticleRenderer::burst(Vector3d position, u8 effect) {
	if (effect > 2 || !std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z)) return;
	auto random = [&] { m_random = m_random * 1664525u + 1013904223u; return float(m_random >> 8) / 16777216.f; };
	for (int i = 0; i < 12; ++i) {
		Vector3d start = position + Vector3d{random() - 0.5, random() - 0.5, random() * 0.6};
		emit(start, {random() * 0.4f - 0.2f, random() * 0.4f - 0.2f, 0.3f + random() * 0.5f},
			effect == 0 ? Spark : effect == 1 ? Smoke : Effect, effect == 1 ? 0.25f : 0.15f, 1000);
	}
}

void ParticleRenderer::scan(const World &world, Vector3d eye) {
	m_anchors.clear();
	unsigned insects = 0, sources = 0;
	bool day = GameTime::getTicks() % GameTime::dayLength < 12000;
	int cx = int(std::floor(eye.x)), cy = int(std::floor(eye.y)), cz = int(std::floor(eye.z));
	for (int z = cz - 6; z <= cz + 6; ++z) for (int y = cy - 12; y <= cy + 12; ++y) for (int x = cx - 12; x <= cx + 12; ++x) {
		const BlockState *state = world.getBlockState(x, y, z);
		if (!state || !state->block().id() || world.getLight(x, y, z + 1) < 0) continue;
		const auto &id = state->block().stringID();
		u32 seed = u32(x) * 73856093u ^ u32(y) * 19349663u ^ u32(z) * 83492791u;
		if (sources < 24 && (id == "default:torch" || id == "default:lava")) {
			const auto *above = world.getBlockState(x,y,z+1);
			if (id == "default:torch" || (above && !above->block().id() && seed % 17 == 0)) {
				m_anchors.push_back({{x,y,z},id == "default:torch" ? Smoke : Lava}); ++sources;
			}
		}
		else if (insects < 16 && day && (id == "default:rose" || id == "default:dandelion")
		 && seed % 3 == 0 && world.getLight(x,y,z+1) >= 12) {
			m_anchors.push_back({{x,y,z},Kind(Red + seed % 3)}); ++insects;
		}
		else if (insects < 16 && !day && id == "default:grass" && seed % 89 == 0) {
			const auto *above = world.getBlockState(x,y,z+1);
			const Chunk *chunk = world.getChunkAtBlockPos(x,y,z+1);
			if (above && !above->block().id() && chunk && chunk->lightmap().getSunlight(x & 15,y & 15,(z+1) & 31) >= 12) {
				m_anchors.push_back({{x,y,z},Firefly}); ++insects;
			}
		}
	}
}

void ParticleRenderer::draw(const World &world, const Camera &camera, RenderTarget &target, RenderStates states) {
	u64 now = GameClock::getInstance().getTicks();
	Vector3d eye = camera.getDPosition();
	if (!m_lastScan || now - m_lastScan >= 1000) { scan(world, eye); m_lastScan = now; }
	m_particles.erase(std::remove_if(m_particles.begin(), m_particles.end(), [&](const Particle &particle) {
		return now < particle.started || now - particle.started >= particle.lifetime;
	}), m_particles.end());
	if (now - m_lastEmit >= 250) {
		m_lastEmit = now;
		for (const auto &anchor : m_anchors) {
			if (anchor.kind == Smoke) {
				const auto &p = anchor.position;
				const auto *state = world.getBlockState(p.x, p.y, p.z);
				if (!state || state->block().stringID() != "default:torch") continue;
				u16 rotation = state->block().param().getParam(BlockParam::Rotation, world.getData(p.x, p.y, p.z));
				Vector3d tip{0.5, 0.5, 0.7};
				if (rotation) {
					// the wall mesh's cap is centered at (0.5, 0.2439175, 0.7977425).
					tip.z = 0.87;
					if (rotation == 1) tip.x = 0.2439175;
					else if (rotation == 2) tip.y = 0.2439175;
					else if (rotation == 3) tip.x = 0.7560825;
					else if (rotation == 4) tip.y = 0.7560825;
				}
				emit(Vector3d(p) + tip + Vector3d{0, 0, 0.05}, {0,0,0.45f}, Smoke, 0.14f, 1400);
				emit(Vector3d(p) + tip, {0,0,0.1f}, Flame, 0.12f, 300);
			}
			else if (anchor.kind == Lava && now % 1000 < 250)
				emit(Vector3d(anchor.position)+Vector3d{0.5,0.5,1},{0,0,0.35f},Lava,0.18f,1000);
		}
		const auto *state = world.getBlockState(int(std::floor(eye.x)),int(std::floor(eye.y)),int(std::floor(eye.z)));
		if (state && (state->block().stringID() == "default:water" || state->block().stringID() == "default:water_flowing"))
			emit(eye+Vector3d{0.35,0.2,-0.3},{0,0,0.7f},Bubble,0.12f,900);
	}
	const auto &view = camera.getViewTransform().getMatrix();
	glm::vec3 right(view[0][0],view[1][0],view[2][0]), up(view[0][1],view[1][1],view[2][1]);
	std::array<std::vector<Vertex>,Count> batches;
	auto quad = [&](Vector3d position, Kind kind, float size, int frame, int frames, bool flip = false) {
		int light = world.getLight(int(std::floor(position.x)),int(std::floor(position.y)),int(std::floor(position.z)));
		if (light < 0 || (position-eye).length() > 24) return;
		static constexpr float corners[][2] = {{0,0},{1,0},{1,1},{0,0},{1,1},{0,1}};
		for (const auto &corner : corners) {
			glm::vec3 point = glm::vec3(float(position.x),float(position.y),float(position.z))
				+ right*((corner[0]-0.5f)*size)+up*((0.5f-corner[1])*size);
			Vertex vertex;
			vertex.coord3d[0]=point.x; vertex.coord3d[1]=point.y; vertex.coord3d[2]=point.z; vertex.coord3d[3]=6;
			vertex.texCoord[0]=flip ? 1-corner[0] : corner[0]; vertex.texCoord[1]=(corner[1]+frame)/frames;
			for (float &color : vertex.color) color=1;
			vertex.lightValue[0]=float(light);
			vertex.lightValue[1]=kind==Firefly ? 10.f : kind==Flame || kind==Lava ? 14.f : 0.f;
			batches[kind].push_back(vertex);
		}
	};
	for (const auto &anchor : m_anchors) {
		const auto &p = anchor.position;
		const auto *state=world.getBlockState(p.x,p.y,p.z); if(!state || !state->block().id()) continue;
		float phase=float(double(now)/1000 + double(p.x)*0.37 + double(p.y)*0.61);

		if (anchor.kind >= Red) {
			bool firefly=anchor.kind==Firefly;
			quad(Vector3d(p)+Vector3d{0.5+std::sin(phase)*0.8,0.5+std::cos(phase*0.8)*0.8,1.2+std::sin(phase*1.3)*0.35},
				anchor.kind,firefly ? 0.2f : 0.3f,int(now/100)%8,8);
		}
	}
	for(const auto &particle:m_particles) {
		double age=double(now-particle.started)/1000;
		Vector3d position=particle.position+Vector3d(particle.velocity)*age;
		quad(position,particle.kind,particle.size*(particle.kind==Smoke ? 1+float(age)*0.5f : 1),0,1);
	}
	static const char *files[] = {"particles/refi_smoke.png","particles/refi_bubble.png",
		"particles/refi_flame.png","particles/refi_lava.png","particles/refi_bonemeal.png","particles/refi_effect.png",
		"particles/refi_butterfly_red.png","particles/refi_butterfly_violet.png","particles/refi_butterfly_white.png","particles/refi_firefly.png"};
	for(unsigned kind=0;kind<Count;++kind) {
		if(batches[kind].empty())continue;
		if(m_textures[kind].filename().empty())m_textures[kind].loadFromFile(std::string("mods/default/textures/")+files[kind]);
		m_vertices[kind].init(batches[kind].data(),u32(batches[kind].size()*sizeof(Vertex)),true);
		states.texture=&m_textures[kind]; states.isCullFaceEnabled=false; states.isDepthTestEnabled=true;
		target.draw(m_vertices[kind],0,u32(batches[kind].size()),states);
	}
}
