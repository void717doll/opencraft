#include <algorithm>
#include <bgfx/bgfx.h>
#include <SDL.h>
#include "client/graphics/RenderTarget.hpp"
#include "common/core/GameTime.hpp"
#include "common/world/MapSurface.hpp"
#include "common/core/Logger.hpp"
#include "client/core/Config.hpp"
#include "client/core/GameConfig.hpp"
#include "client/hud/PaperMap.hpp"
#include "client/world/ClientPlayer.hpp"
#include "client/world/ClientWorld.hpp"

namespace {
const u8 palette[][3] = {{214,195,150},{101,138,59},{115,115,115},{218,207,155},{60,91,185},
	{132,101,61},{238,241,242},{130,91,60},{64,111,43},{222,92,25},{116,51,48}};

}
PaperMap::~PaperMap() { flush(); }
void PaperMap::flush() {
	if (!m_dirty || !m_id) return;
	if (navigation::save(m_worldName, m_id, m_map)) m_dirty = false;
	else logError() << "could not save explored map" << m_id;
}
void PaperMap::upload() {
	std::array<u8, navigation::mapSize * navigation::mapSize * 4> pixels{};
	for (unsigned i = 0; i < m_map.colors.size(); ++i) {
		unsigned color = std::min<unsigned>(m_map.colors[i], 10);
		for (unsigned c = 0; c < 3; ++c) pixels[i * 4 + c] = palette[color][c];
		pixels[i * 4 + 3] = 255;
	}
	if (!m_ready) {
		SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(), 128, 128, 32, 128 * 4, SDL_PIXELFORMAT_RGBA32);
		if (!surface) return;
		m_texture.loadFromSurface(surface);
		SDL_FreeSurface(surface);
		m_image.load(m_texture);
		m_paperTexture.loadFromFile("mods/default/textures/items/refi_map_background.png");
		m_paper.load(m_paperTexture);
		m_ready = true;
	} else m_texture.updateRegion(0, 0, 0, 128, 128, pixels.data());
}
void PaperMap::update(ClientPlayer &player, ClientWorld &world) {
	m_visible = player.heldItemSlot() >= 0 && player.heldItemSlot() < player.inventory().width() && player.heldItemStack().amount()
		&& player.heldItemStack().stringID() == "default:map" && player.heldItemStack().damage();
	if (!m_visible) { flush(); return; }
	u16 id = player.heldItemStack().damage();
	if (id != m_id || m_worldName != GameConfig::worldName) {
		flush();
		m_id = id;
		m_worldName = GameConfig::worldName;
		m_map = {};
		if (!navigation::load(m_worldName, id, m_map)) { m_id = 0; m_visible = false; return; }
		upload();
	}
	float screenWidth = Config::screenWidth / float(Config::guiScale);
	float screenHeight = Config::screenHeight / float(Config::guiScale);
	float size = std::min(160.f, std::min(screenWidth - 20, screenHeight - 70));
	size = std::max(size, 32.f);
	setPosition((screenWidth - size) / 2, screenHeight - size - 30);
	setScale(size / 128, size / 128, 1);
	m_paper.setPosRect(-8, -8, 144, 144);
	float px = float((player.x() - m_map.x) / 8 + 64);
	float py = float(64 - (player.y() - m_map.y) / 8);
	m_marker.setPosition(std::clamp(px, 0.f, 126.f), std::clamp(py, 0.f, 126.f));
	u64 tick = GameTime::getTicks();
	if (tick == m_tick || player.dimension() != m_map.dimension || player.dimension()) return;
	m_tick = tick;
	int centerX = int(std::floor(px)), centerY = int(std::floor(py));
	int column = centerX - 12 + int(tick % 25);
	if (column < 0 || column >= 128) return;
	bool changed = false;
	for (int row = std::max(0, centerY - 12); row <= std::min(127, centerY + 12); ++row) {
		if ((column-centerX)*(column-centerX)+(row-centerY)*(row-centerY) > 144) continue;
		int x = m_map.x + (column - 64) * 8 + 4;
		int y = m_map.y - (row - 64) * 8 - 4;
		auto color = navigation::sampleSurface(world, x, y);
		auto &pixel = m_map.colors[row * 128 + column];
		if (color && pixel != *color) { pixel = *color; changed = true; }
	}
	if (changed) { m_dirty = true; upload(); }
	if (tick - m_saved >= 100) { flush(); m_saved = tick; }
}
void PaperMap::draw(RenderTarget &target, RenderStates states) const {
	if (!m_visible || !m_ready) return;
	states.transform *= getTransform();
	target.draw(m_paper, states);
	target.draw(m_image, states);
	target.draw(m_marker, states);
}
