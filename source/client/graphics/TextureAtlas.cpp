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
#include <SDL_image.h>

#include "common/core/Exception.hpp"
#include "common/core/Registry.hpp"
#include "client/graphics/TextureAtlas.hpp"

#include <filesystem.hpp>

namespace fs = ghc::filesystem;

void TextureAtlas::clear() {
	m_tileSize = 0;

	m_textureMap.clear();
	m_textures.clear();
	m_clockFrames.clear();
	m_compassFrames.clear();
	m_compassFrame = 32;
	m_compass = {};
	m_clockFrame = 64;
	m_clock = {};

	m_isReady = false;
}

void TextureAtlas::loadFromRegistry() {
	addFile("mods/default/textures/blocks/", "undefined.png");
	for (const auto &block : Registry::getInstance().blocks()) {
		std::string path = "mods/" + block->modName() + "/textures/blocks/";
		for (const auto &state : block->states()) {
			for (const auto &filename : state.tiles().textureFilenames())
				addFile(path, filename);
		}
	}
	for (const auto &item : Registry::getInstance().items()) {
		if (!item->isBlock() || !item->tiles().textureFilenames().empty()) {
			std::string path = "mods/" + item->modName() + "/textures/items/";
			for (const auto &filename : item->tiles().textureFilenames())
				addFile(path, filename);
			bool compass = item->getGroupValue("group:om_compass") != 0;
			if ((compass || item->getGroupValue("group:om_clock")) && item->tiles().textureFilenames().size() == (compass ? 32 : 64)) {
				auto &frames = compass ? m_compassFrames : m_clockFrames;
				(compass ? m_compassTexture : m_clockTexture) = getTextureID(item->tiles().getTextureForFace(0));
				for (const auto &filename : item->tiles().textureFilenames()) {
					SDL_Surface *surface = SDL_ConvertSurfaceFormat(m_textures.at(getTextureID(filename)).get(), SDL_PIXELFORMAT_RGBA32, 0);
					if (!surface) throw EXCEPTION("failed to read clock frame:", filename);
					std::vector<u8> pixels(m_tileSize * m_tileSize * 4);
					for (unsigned y = 0; y < m_tileSize; ++y)
						std::memcpy(pixels.data() + y * m_tileSize * 4, static_cast<u8 *>(surface->pixels) + y * surface->pitch, m_tileSize * 4);
					frames.emplace_back(std::move(pixels));
					SDL_FreeSurface(surface);
				}
			}
		}
	}
	packTextures();
}

void TextureAtlas::updateClock(u64 ticks, u16 dimension) {
	if (!m_isReady || m_clockFrames.size() != 64) return;
	unsigned frame = m_clock.update(ticks, dimension);
	if (frame == m_clockFrame) return;
	m_clockFrame = frame;
	updateTile(m_clockTexture, m_clockFrames[frame]);
}

void TextureAtlas::updateCompass(u64 ticks, u16 dimension, double x, double y, double yaw) {
	if (!m_isReady || m_compassFrames.size() != 32) return;
	unsigned frame = m_compass.update(ticks, dimension, x, y, yaw);
	if (frame == m_compassFrame) return;
	m_compassFrame = frame;
	updateTile(m_compassTexture, m_compassFrames[frame]);
}

void TextureAtlas::updateTile(u16 texture, std::vector<u8> pixels) {
	u16 across = u16(m_texture.getSize().x / m_tileSize);
	u16 x = u16((texture % across) * m_tileSize);
	u16 y = u16((texture / across) * m_tileSize);
	u16 size = m_tileSize;
	for (u8 mip = 0; size; ++mip) {
		m_texture.updateRegion(mip, x >> mip, y >> mip, size, size, pixels.data());
		if (size == 1) break;
		u16 smaller = size / 2;
		std::vector<u8> next(smaller * smaller * 4);
		for (unsigned py = 0; py < smaller; ++py) for (unsigned px = 0; px < smaller; ++px) {
			for (unsigned color = 0; color < 4; ++color) {
				unsigned total = 0;
				for (unsigned dy = 0; dy < 2; ++dy) for (unsigned dx = 0; dx < 2; ++dx)
					total += pixels[((py * 2 + dy) * size + px * 2 + dx) * 4 + color];
				next[(py * smaller + px) * 4 + color] = u8(total / 4);
			}
		}
		pixels = std::move(next);
		size = smaller;
	}
}

FloatRect TextureAtlas::getTexCoords(const std::string &filename, bool normalized) const {
	if (filename.empty()) return FloatRect{0, 0, 0, 0};

	if (!m_isReady)
		throw EXCEPTION("Can't get texture coordinates from empty atlas");

	u16 textureID = getTextureID(filename);

	float textureX = float((textureID % (m_texture.getSize().x / m_tileSize)) * m_tileSize);
	float textureY = float((textureID / (m_texture.getSize().x / m_tileSize)) * m_tileSize);

	if (normalized)
		return FloatRect{textureX / (float)m_texture.getSize().x,
							 textureY / (float)m_texture.getSize().y,
							 m_tileSize / (float)m_texture.getSize().x,
							 m_tileSize / (float)m_texture.getSize().y};
	else
		return FloatRect{textureX,
							 textureY,
							 (float)m_tileSize,
							 (float)m_tileSize};
}

void TextureAtlas::addFile(const std::string &path, const std::string &filename) {
	if (filename.empty()) return;

	auto it = m_textureMap.find(filename);
	if (it != m_textureMap.end())
		return;

	auto separator = filename.find('^');
	SurfacePtr surface{IMG_Load((path + filename.substr(0, separator)).c_str()), &SDL_FreeSurface};
	if(!surface) {
		logError() << "Failed to load texture:" << path + filename;
		return;
	}

	if (separator != std::string::npos) {
		SurfacePtr overlay{IMG_Load((path + filename.substr(separator + 1)).c_str()), &SDL_FreeSurface};
		if (!overlay || overlay->w != surface->w || overlay->h != surface->h)
			throw EXCEPTION("invalid texture overlay:", path + filename);
		SDL_SetSurfaceBlendMode(overlay.get(), SDL_BLENDMODE_BLEND);
		if (SDL_BlitSurface(overlay.get(), nullptr, surface.get(), nullptr) != 0)
			throw EXCEPTION("failed to blend texture overlay:", path + filename, SDL_GetError());
	}

	if (!m_tileSize)
		m_tileSize = (u16)surface->w;

	if (m_tileSize != surface->w || m_tileSize != surface->h)
		throw EXCEPTION("Texture size unexpected for", path + filename + ". Got", surface->w, surface->h, "instead of", m_tileSize, m_tileSize);

	m_textureMap.emplace(filename, (u16)m_textures.size());
	m_textures.emplace_back(std::move(surface));
}

u16 TextureAtlas::getTextureID(const std::string &filename) const {
	auto it = m_textureMap.find(filename);
	if (it == m_textureMap.end()) {
		return 0;
	}

	return it->second;
}

void TextureAtlas::packTextures() {
	if (!m_tileSize)
		throw EXCEPTION("Cannot pack zero-sized textures!");

	Uint32 rmask, gmask, bmask, amask;
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	rmask = 0xff000000;
	gmask = 0x00ff0000;
	bmask = 0x0000ff00;
	amask = 0x000000ff;
#else
	rmask = 0x000000ff;
	gmask = 0x0000ff00;
	bmask = 0x00ff0000;
	amask = 0xff000000;
#endif

	// Max amount of textures on one line
	const u16 atlasWidth = 32;

	// Max amount of textures on one column
	// const u16 atlasHeight = (u16)std::ceil((float)m_textures.size() / atlasWidth);
	const u16 atlasHeight = 32;

unsigned mipmapLevels = 1;
	for (unsigned side = atlasWidth * m_tileSize; side > 1; side /= 2) ++mipmapLevels;
	std::vector<SurfacePtr> atlas;
	for (unsigned level = 0; level < mipmapLevels; ++level)
		atlas.emplace_back(nullptr, &SDL_FreeSurface);

	atlas[0].reset(SDL_CreateRGBSurface(0, atlasWidth * m_tileSize, atlasHeight * m_tileSize, 32, rmask, gmask, bmask, amask));
	if (!atlas[0]) {
		throw EXCEPTION("Failed to create surface:", SDL_GetError());
	}

	u16 i = 0;
	for (auto &it : m_textures) {
		SDL_Rect outRect;
		outRect.x = (i % atlasWidth) * m_tileSize;
		outRect.y = (i / atlasWidth) * m_tileSize;
		outRect.w = m_tileSize;
		outRect.h = m_tileSize;

		SDL_UpperBlit(it.get(), nullptr, atlas[0].get(), &outRect);

		++i;
	}

	std::vector<int> size(mipmapLevels);
	size[0] = atlas[0]->h * atlas[0]->pitch;
	int totalSize = size[0];

	// Generate mipmaps
	for (unsigned i = 1; i < mipmapLevels; ++i) {
		int width = (atlasWidth * m_tileSize) / (1 << i);
		int height = (atlasHeight * m_tileSize) / (1 << i);

		atlas[i].reset(SDL_CreateRGBSurface(0, width, height, 32, rmask, gmask, bmask, amask));
		if (!atlas[i])
			throw EXCEPTION("Failed to create surface:", SDL_GetError());

		size[i] = atlas[i]->h * atlas[i]->pitch;
		totalSize += size[i];

		u8 *src = (u8 *)atlas[i - 1]->pixels;
		u8 *dst = (u8 *)atlas[i]->pixels;
		for (int j = 0; j < size[i] / 4; ++j) {
			int srcX = (j % width) * 2;
			int srcY = (j / width) * 2;
			int srcPos1 = srcX + srcY * width * 2;
			int srcPos2 = (srcX + 1) + srcY * width * 2;
			int srcPos3 = srcX + (srcY + 1) * width * 2;
			int srcPos4 = (srcX + 1) + (srcY + 1) * width * 2;
			dst[j * 4 + 0] = (u8)(((int)src[srcPos1 * 4 + 0] + src[srcPos2 * 4 + 0] + src[srcPos3 * 4 + 0] + src[srcPos4 * 4 + 0]) / 4);
			dst[j * 4 + 1] = (u8)(((int)src[srcPos1 * 4 + 1] + src[srcPos2 * 4 + 1] + src[srcPos3 * 4 + 1] + src[srcPos4 * 4 + 1]) / 4);
			dst[j * 4 + 2] = (u8)(((int)src[srcPos1 * 4 + 2] + src[srcPos2 * 4 + 2] + src[srcPos3 * 4 + 2] + src[srcPos4 * 4 + 2]) / 4);
			dst[j * 4 + 3] = (u8)(((int)src[srcPos1 * 4 + 3] + src[srcPos2 * 4 + 3] + src[srcPos3 * 4 + 3] + src[srcPos4 * 4 + 3]) / 4);
		}
	}

	m_textures.clear();

	const bgfx::Memory *mem = bgfx::alloc(totalSize);
	int offset = 0;
	for (unsigned i = 0; i < mipmapLevels; ++i) {
		std::memcpy(mem->data + offset, atlas[i]->pixels, size[i]);
		offset += size[i];
	}

	m_isReady = true;

	if (IMG_SavePNG(atlas[2].get(), "test_atlas.png") < 0)
		logError() << "Failed to save texture to: test_atlas.png. Reason:" << IMG_GetError();

	m_texture.free();
	m_texture.loadFromMemory(mem, atlas[0]->w, atlas[0]->h);
}
