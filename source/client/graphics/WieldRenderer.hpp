#ifndef WIELDRENDERER_HPP_
#define WIELDRENDERER_HPP_

#include <memory>
#include <string>

#include <SDL.h>

#include "client/graphics/Camera.hpp"
#include "client/graphics/Drawable.hpp"
#include "client/graphics/Shader.hpp"
#include "client/graphics/Texture.hpp"
#include "client/graphics/VertexBuffer.hpp"

class ClientPlayer;
class InventoryCube;
class Item;
class TextureAtlas;

class WieldRenderer : public Drawable {
	public:
		explicit WieldRenderer(ClientPlayer &player, bool offhand = false);
		~WieldRenderer();
		void onEvent(const SDL_Event &event);
		void update();
		void pause();

	private:
		void setItem(const Item *item);
		void buildItem(const Item &item, const std::string &filename, bool blockTexture);
		void draw(RenderTarget &target, RenderStates states) const override;

		ClientPlayer &m_player;
		bool m_offhand;
		TextureAtlas &m_atlas;
		Shader m_shader;
		Texture m_skin;
		Texture m_armorSkin;
		mutable Camera m_camera{72.f, 0.05f, 10.f};
		VertexBuffer m_armVertices;
		VertexBuffer m_armorArmVertices;
		VertexBuffer m_itemVertices;
		u32 m_itemVertexCount = 0;
		std::unique_ptr<InventoryCube> m_cube;
		std::string m_itemID;
		u64 m_swingStart = 0;
		u64 m_lastUpdate = 0;
		float m_equip = 1;
		bool m_swinging = false;
		bool m_attacking = false;
		bool m_using = false;
		float m_walkPhase = 0;
		float m_bob = 0;
		double m_lastX = 0;
		double m_lastY = 0;
};

#endif // WIELDRENDERER_HPP_
