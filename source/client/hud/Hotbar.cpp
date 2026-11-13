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
#include "client/core/Config.hpp"
#include "client/core/KeyboardHandler.hpp"
#include "client/core/input/GamePad.hpp"
#include "common/core/input/GameKey.hpp"
#include "common/core/GameClock.hpp"
#include "client/core/Config.hpp"
#include "client/hud/Hotbar.hpp"
#include "client/network/ClientCommandHandler.hpp"
#include "client/world/ClientPlayer.hpp"

Hotbar::Hotbar(ClientPlayer &player, ClientCommandHandler &client, Widget *parent)
	: Widget(182, 22, parent), m_player(player), m_client(client)
{
	m_background.load("texture-hotbar");
	m_background.setClipRect(0, 0, 182, 22);
	m_background.setPosition(0, 0, 0);

	m_cursor.load("texture-hotbar_selected");
	m_cursor.setClipRect(0, 0, 24, 24);
	m_cursor.setPosition(-1, -1, 0);
}

void Hotbar::onEvent(const SDL_Event &event) {
	if (m_cursorPos == -1) return;

	int slot = m_cursorPos;
	if (event.type == SDL_KEYDOWN && !event.key.repeat) {
		auto *keyboard = static_cast<KeyboardHandler *>(GamePad::getInputHandler());
		for (int i = 0; i < 9; ++i) {
			if (event.key.keysym.sym == keyboard->getKeycode(GameKey::Hotbar1 + i)) {
				slot = i;
				break;
			}
		}
	}
	if (event.type == SDL_MOUSEWHEEL) {
		if (event.wheel.y < 0) {
			slot = (m_cursorPos + 1) % 9;
		}
		else if (event.wheel.y > 0) {
			slot = (m_cursorPos == 0) ? 8 : m_cursorPos - 1;
		}
	}
	if (slot != m_cursorPos) {
		m_cursorPos = slot;
		m_player.setHeldItemSlot((u8)slot);
		m_client.sendPlayerHeldItemChanged((u8)slot, currentItem().id());
		m_cursor.setPosition(-1.f + 20.f * (float)m_cursorPos, -1.f);
	}
}

void Hotbar::update() {
	if (m_player.heldItemSlot() == -1) return;
	if (m_cursorPos != m_player.heldItemSlot()) {
		m_cursorPos = m_player.heldItemSlot();
		m_cursor.setPosition(-1.f + 20.f * (float)m_cursorPos, -1.f);
	}

	for (u16 i = 0 ; i < 9 ; ++i) {
		if (m_items.size() <= i) {
			m_items.emplace_back(m_player.inventory(), i, 0);

			ItemWidget &widget = m_items.back();
			widget.setPosition(5.f + 20.f * (float)i - 3.f, 2.f);
		}

		m_items[i].update();
	}

	updateItemName();
}

void Hotbar::updateItemName() {
	const ItemStack &stack = m_player.heldItemStack();
	u64 now = GameClock::getInstance().getTicks();
	std::string id = stack.amount() ? stack.stringID() : "";
	if (m_namedSlot != m_player.heldItemSlot() || m_namedItem != id) {
		m_namedSlot = m_player.heldItemSlot();
		m_namedItem = id;
		m_itemNameUntil = now + 3000;
		const std::string &label = stack.item().label();
		auto start = label.find_first_not_of(" \t\r\n");
		m_itemName.setString(start == std::string::npos ? "" : label.substr(start, label.find_last_not_of(" \t\r\n") - start + 1));
	}

	m_durabilityVisible = Config::isItemDurabilityVisible && stack.amount() && stack.maxDamage();
	if (m_durabilityVisible) {
		u16 maximum = stack.maxDamage();
		u16 remaining = maximum - std::min(stack.damage(), maximum);
		m_durability.setString(std::to_string(remaining) + " / " + std::to_string(maximum));
		m_durability.updateVertexBuffer();
		m_durability.setPosition(std::floor((width() - m_durability.getSize().x) / 2.f), m_player.isCreative() ? -22.f : -30.f);
	}

	m_itemNameVisible = !id.empty() && now < m_itemNameUntil;
	if (m_itemNameVisible) {
		float alpha = std::min(1.f, float(m_itemNameUntil - now) / 1000.f);
		m_itemName.setShadowColor(Color(0.25f, 0.25f, 0.25f, alpha));
		m_itemName.setColor(Color(1, 1, 1, alpha));
		m_itemName.updateVertexBuffer();
		m_itemName.setPosition(std::floor((width() - m_itemName.getSize().x) / 2.f), (m_durabilityVisible ? -33.f : -22.f) - (m_player.isCreative() ? 0.f : 8.f));
	}
}

const Item &Hotbar::currentItem() const {
	return m_player.inventory().getStack((u16)m_cursorPos, 0).item();
}

void Hotbar::draw(RenderTarget &target, RenderStates states) const {
	states.transform *= getTransform();

	target.draw(m_background, states);
	target.draw(m_cursor, states);

	for (auto &it : m_items)
		target.draw(it, states);
	if (m_itemNameVisible)
		target.draw(m_itemName, states);
	if (m_durabilityVisible)
		target.draw(m_durability, states);
}
