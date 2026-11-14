#include <SDL_image.h>

#include "common/core/Registry.hpp"
#include "client/graphics/ItemMeshBuilder.hpp"
#include "client/graphics/TextureAtlas.hpp"

namespace {

void appendQuad(std::vector<Vertex> &vertices, const glm::vec3 (&points)[4], const FloatRect &uv) {
	constexpr int order[] = {0,1,2,0,2,3};
	for (int index : order) {
		Vertex vertex;
		for (int axis = 0; axis < 3; ++axis) vertex.coord3d[axis] = points[index][axis];
		vertex.texCoord[0] = uv.x + (index == 1 || index == 2 ? uv.sizeX : 0);
		vertex.texCoord[1] = uv.y + (index >= 2 ? uv.sizeY : 0);
		for (float &color : vertex.color) color = 1;
		vertices.push_back(vertex);
	}
}

}

std::vector<Vertex> buildItemMesh(const Item &item, const TextureAtlas &atlas, const std::string &filename, bool blockTexture, float size) {
	const FloatRect uv = atlas.getTexCoords(filename);
	std::vector<Vertex> vertices;
	float half = size * 0.5f, depth = size * 0.05f;
	glm::vec3 front[4] = {{-half,half,depth},{half,half,depth},{half,-half,depth},{-half,-half,depth}};
	glm::vec3 back[4] = {{-half,half,-depth},{half,half,-depth},{half,-half,-depth},{-half,-half,-depth}};
	appendQuad(vertices, front, uv);
	appendQuad(vertices, back, uv);

	std::string path = "mods/" + item.modName() + (blockTexture ? "/textures/blocks/" : "/textures/items/") + filename;
	SDL_Surface *original = IMG_Load(path.c_str());
	SDL_Surface *surface = original ? SDL_ConvertSurfaceFormat(original, SDL_PIXELFORMAT_RGBA32, 0) : nullptr;
	if (original) SDL_FreeSurface(original);
	if (surface) {
		auto opaque = [&](int x, int y) {
			if (x < 0 || y < 0 || x >= surface->w || y >= surface->h) return false;
			auto *pixels = static_cast<u8 *>(surface->pixels);
			return pixels[y * surface->pitch + x * 4 + 3] >= 76;
		};
		// outline edges give the sprite thickness without filling transparent pixels.
		for (int y = 0; y < surface->h; ++y) {
			for (int x = 0; x < surface->w; ++x) {
				if (!opaque(x,y)) continue;
				float left = -half + 2 * half * float(x) / float(surface->w);
				float right = -half + 2 * half * float(x + 1) / float(surface->w);
				float top = half - 2 * half * float(y) / float(surface->h);
				float bottom = half - 2 * half * float(y + 1) / float(surface->h);
				FloatRect pixel{uv.x + uv.sizeX * (float(x) + 0.5f) / float(surface->w),
					uv.y + uv.sizeY * (float(y) + 0.5f) / float(surface->h),0,0};
				if (!opaque(x - 1,y)) {
					glm::vec3 edge[4] = {{left,top,depth},{left,top,-depth},{left,bottom,-depth},{left,bottom,depth}};
					appendQuad(vertices,edge,pixel);
				}
				if (!opaque(x + 1,y)) {
					glm::vec3 edge[4] = {{right,top,depth},{right,top,-depth},{right,bottom,-depth},{right,bottom,depth}};
					appendQuad(vertices,edge,pixel);
				}
				if (!opaque(x,y - 1)) {
					glm::vec3 edge[4] = {{left,top,depth},{right,top,depth},{right,top,-depth},{left,top,-depth}};
					appendQuad(vertices,edge,pixel);
				}
				if (!opaque(x,y + 1)) {
					glm::vec3 edge[4] = {{left,bottom,depth},{right,bottom,depth},{right,bottom,-depth},{left,bottom,-depth}};
					appendQuad(vertices,edge,pixel);
				}
			}
		}
		SDL_FreeSurface(surface);
	}
	if (item.isBlock()) {
		const auto &state = Registry::getInstance().getBlockFromStringID(item.stringID()).getState(0);
		const Color &color = state.inventoryImage().empty() ? state.colorMultiplier() : Color::White;
		for (auto &vertex : vertices) {
			vertex.color[0] = color.r;
			vertex.color[1] = color.g;
			vertex.color[2] = color.b;
			vertex.color[3] = color.a;
		}
	}
	u16 brightness = item.getGroupValue("group:om_item_brightness");
	if (brightness) {
		float multiplier = float(std::min<u16>(100, brightness)) / 100.f;
		for (auto &vertex : vertices)
			for (int channel = 0; channel < 3; ++channel) vertex.color[channel] *= multiplier;
	}
	return vertices;
}
