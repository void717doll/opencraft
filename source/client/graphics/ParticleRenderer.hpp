#ifndef PARTICLERENDERER_HPP_
#define PARTICLERENDERER_HPP_

#include <array>
#include <vector>

#include "common/core/Vector3.hpp"
#include "client/graphics/RenderStates.hpp"
#include "client/graphics/Texture.hpp"
#include "client/graphics/VertexBuffer.hpp"

class Camera;
class RenderTarget;
class World;

class ParticleRenderer {
	public:
		ParticleRenderer();
		void burst(Vector3d position, u8 effect);
		void clear();
		void draw(const World &world, const Camera &camera, RenderTarget &target, RenderStates states);

	private:
		enum Kind : u8 { Smoke, Bubble, Flame, Lava, Spark, Effect, Red, Violet, White, Firefly, Count };
		struct Particle {
			Vector3d position;
			Vector3f velocity;
			u64 started;
			u16 lifetime;
			Kind kind;
			float size;
		};
		struct Anchor { Vector3i position; Kind kind; };
		void emit(Vector3d position, Vector3f velocity, Kind kind, float size, u16 lifetime);
		void scan(const World &world, Vector3d eye);
		std::vector<Anchor> m_anchors;
		std::vector<Particle> m_particles;
		u64 m_lastScan = 0, m_lastEmit = 0;
		u32 m_random = 1;
		std::array<Texture, Count> m_textures;
		std::array<VertexBuffer, Count> m_vertices;
};

#endif
