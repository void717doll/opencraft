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
		m_arrowTexture.loadFromFile("mods/default/textures/items/refi_map_arrow.png");
		m_dotTexture.loadFromFile("mods/default/textures/items/refi_map_dot.png");
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
		m_sample = 0;
		m_tick = ~u64(0);
		if (!navigation::load(m_worldName, id, m_map)) { m_id = 0; m_visible = false; return; }
		m_sample = unsigned(navigation::marker(m_map,player.x(),player.y(),player.cameraYaw()).y)*128;
		upload();
	}
	float guiScale = float(std::max(1,int(Config::guiScale)));
	float pixelScale = std::min({2.f,(float(Config::screenWidth)-16)/140,(float(Config::screenHeight)-100)/140});
	pixelScale = std::max(0.25f,pixelScale);
	float half = 70 * pixelScale;
	float x = std::clamp(float(Config::screenWidth)*0.75f,half+8,float(Config::screenWidth)-half-8);
	float bottom = std::min(float(Config::screenHeight)*0.8f,float(Config::screenHeight)-40);
	setPosition((x-64*pixelScale)/guiScale,(bottom-134*pixelScale)/guiScale);
	setScale(pixelScale/guiScale,pixelScale/guiScale,1);
	m_paper.setPosRect(-6,-6,140,140);
	auto marker = navigation::marker(m_map,player.x(),player.y(),player.cameraYaw());
	const Texture &markerTexture = marker.outside ? m_dotTexture : m_arrowTexture;
	if (m_marker.texture() != &markerTexture) {
		m_marker.load(markerTexture);
		m_marker.setPosRect(-4,-4,8,8);
	}
	m_marker.setPosition(marker.x,marker.y);
	m_marker.setRotation(marker.rotation);
	m_markerVisible = player.dimension() == m_map.dimension && player.dimension() == 0;
	u64 tick = GameTime::getTicks();
	if (tick == m_tick || !m_markerVisible) return;
	m_tick = tick;
	bool changed = false;
	for (unsigned sample = 0; sample < 64; ++sample) {
		unsigned index = m_sample++ % (navigation::mapSize*navigation::mapSize);
		int column = int(index%128), row = int(index/128);
		if (m_map.scale == 8 && ((column-marker.x)*(column-marker.x)+(row-marker.y)*(row-marker.y)>144)) continue;
		int worldX = m_map.x+(column-64)*m_map.scale+m_map.scale/2;
		int worldY = m_map.y+(63-row)*m_map.scale+m_map.scale/2;
		auto color = navigation::sampleSurface(world,worldX,worldY);
		auto &pixel = m_map.colors[index];
		if (color && pixel != *color) {pixel = *color; changed = true;}
	}
	if (changed) { m_dirty = true; upload(); }
	if (tick - m_saved >= 100) { flush(); m_saved = tick; }
}
void PaperMap::draw(RenderTarget &target, RenderStates states) const {
	if (!m_visible || !m_ready) return;
	states.transform *= getTransform();
	target.draw(m_paper, states);
	target.draw(m_image, states);
	if (m_markerVisible) target.draw(m_marker, states);
}
