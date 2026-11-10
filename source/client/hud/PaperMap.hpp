#ifndef PAPERMAP_HPP_
#define PAPERMAP_HPP_

#include "common/world/NavigationMap.hpp"
#include "client/graphics/Image.hpp"
#include "client/graphics/RectangleShape.hpp"

class ClientPlayer;
class ClientWorld;
class PaperMap : public Drawable, public Transformable {
	public:
		~PaperMap();
		void update(ClientPlayer &player, ClientWorld &world);
		bool visible() const { return m_visible; }
	private:
		void flush();
		void upload();
		void draw(RenderTarget &target, RenderStates states) const override;
		navigation::Map m_map;
		std::string m_worldName;
		u16 m_id = 0;
		u64 m_tick = ~u64(0), m_saved = 0;
		bool m_visible = false, m_dirty = false, m_ready = false;
		Texture m_texture;
		Texture m_paperTexture;
		Image m_image;
		Image m_paper;
		RectangleShape m_marker{2, 2, Color::Red};
};
#endif
