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
#include "client/gui/InventoryWidget.hpp"
#include "client/network/ClientCommandHandler.hpp"

void InventoryWidget::init(Inventory &inventory, u16 offset, u16 size) {
	m_inventory = &inventory;
	if (!m_slotBackground.width()) {
		m_slotBackground.load("texture-inventory_slot");
		m_slotBackground.setPosRect(0, 0, 18, 18);
	}

	m_offset = offset;
	m_size = size > 0 ? size : inventory.width() * inventory.height();
	m_searchResultCount = inventory.width() * inventory.height();

	loadItemWidgets(m_offset, m_size);

	m_width = inventory.width() * 18;
	m_height = inventory.height() * 18;

	m_inventoryWidth = inventory.width();
	m_inventoryHeight = inventory.height();
}

void InventoryWidget::scroll(float scrolling) {
	m_scrolling = std::clamp(scrolling, 0.f, 1.f);
	scrolling = m_scrolling;
	if (m_itemWidgets.size() < m_size)
		return;

	int rows = (m_searchResultCount + m_inventoryWidth - 1) / m_inventoryWidth;
	int extraRows = std::max(0, rows - int(m_size / m_inventoryWidth));
	u16 offset = u16(m_offset + std::round(extraRows * scrolling) * m_inventoryWidth);
	updateItemWidgets(offset, m_size, m_lastSearch);
}

void InventoryWidget::onEvent(const SDL_Event &event) {
	if (event.type == SDL_MOUSEWHEEL && m_isReadOnly && m_currentItemWidget) {
		int rows = (m_searchResultCount + m_inventoryWidth - 1) / m_inventoryWidth - m_size / m_inventoryWidth;
		if (rows > 0) scroll(m_scrolling - float(event.wheel.y) / rows);
	}
	if (event.type == SDL_MOUSEMOTION) {
		m_currentItemWidget = nullptr;
		for (std::size_t i = 0 ; i < m_itemWidgets.size() ; ++i) {
			if (m_itemWidgets[i].isPointInWidget(event.motion.x, event.motion.y)) {
				m_currentItemWidget = &m_itemWidgets[i];

				m_selectedItemBackground.setPosition(
					float(1 + i % m_inventoryWidth * 18),
					float(1 + i / m_inventoryWidth * 18)
				);
			}
		}
	}
}

void InventoryWidget::update() {
	bool hasChanged = false;

	for (auto &it : m_itemWidgets) {
		it.update();

		hasChanged = hasChanged || it.hasChanged();
		it.setChanged(false);
	}

	if (hasChanged)
		sendUpdatePacket();
}

void InventoryWidget::applySearch(const std::string &search) {
	if (search != m_lastSearch) {
		m_scrolling = 0;
		updateItemWidgets(m_offset, m_size, search);
		m_lastSearch = search;
	}
}

bool InventoryWidget::sendItemStackToDest(const ItemWidget *itemStack, AbstractInventoryWidget *dest, bool mergeOnly) {
	if (dest->doItemMatchFilter(itemStack->stack().item())) {
		ItemStack stackRet = dest->receiveItemStack(itemStack, this, mergeOnly);
		if (stackRet.amount() != itemStack->stack().amount()) {
			if (dest != this && !m_isReadOnly)
				m_inventory->setItemStack(itemStack->x(), itemStack->y(), stackRet.amount() ? stackRet : ItemStack::Empty);
			if (isOutput()) m_outputHandler();

			update();
			sendUpdatePacket();
			return true;
		}
	}

	return false;
}

ItemStack InventoryWidget::receiveItemStack(const ItemWidget *itemStack, AbstractInventoryWidget *src, bool mergeOnly) {
	if (m_isReadOnly || isOutput() || (mergeOnly && src == this)) return itemStack->stack();
	ItemStack stack = itemStack->stack();
	if (src->isOutput()) {
		// a recipe is taken as one batch, including when shift-clicking.
		Inventory preview = *m_inventory;
		if (preview.addItemStack(stack, m_offset, m_size, mergeOnly).amount()) return stack;
	}
	if (src == this)
		m_inventory->clearStack(itemStack->x(), itemStack->y());

	ItemStack stackRet = m_inventory->addItemStack(stack, m_offset, m_size, mergeOnly);

	if (stackRet.amount() != stack.amount())
		sendUpdatePacket();

	return stackRet;
}

bool InventoryWidget::takeOutput(ItemStack &held) {
	if (!isOutput() || !m_currentItemWidget || !m_currentItemWidget->isEnabled()) return false;
	const ItemStack output = m_currentItemWidget->stack();
	if (!output.amount() || (held.amount() && !held.sameItem(output))) return false;
	if (unsigned(held.amount()) + output.amount() > output.item().maxStackSize()) return false;
	if (!held.amount()) held = output;
	else held.setAmount(held.amount() + output.amount());
	m_currentItemWidget->setStack(ItemStack::Empty);
	m_outputHandler();
	return true;
}

void InventoryWidget::sendUpdatePacket() {
	if (m_inventory->name() == "minecart") {
		m_client.sendMinecartInvUpdate(*m_inventory);
	}
	else if (m_inventory->inBlock()) {
		m_client.sendBlockInvUpdate(*m_inventory);
	}
	else {
		m_client.sendPlayerInvUpdate();
	}
}

void InventoryWidget::draw(RenderTarget &target, RenderStates states) const {
	states.transform *= getTransform();

	for (const auto &item : m_itemWidgets) {
		RenderStates slotStates = states;
		slotStates.transform *= item.getTransform();
		target.draw(m_slotBackground, slotStates);
	}

	if (m_currentItemWidget)
		target.draw(m_selectedItemBackground, states);

	for (const auto &item : m_itemWidgets)
		target.draw(item, states);
}

void InventoryWidget::loadItemWidgets(u16 offset, u16 size) {
	m_itemWidgets.clear();

	u16 itemCounter = 0;
	for (u16 i = 0 ; itemCounter < size ; ++i) {
		u16 x = u16((i + offset) % m_inventory->width());
		u16 y = u16((i + offset) / m_inventory->width());
		if (x >= m_inventory->width() || y >= m_inventory->height())
			break;

		m_itemWidgets.emplace_back(*m_inventory, x, y, this);

		ItemWidget &widget = m_itemWidgets.back();
		widget.update();
		widget.setPosition((itemCounter % m_inventory->width()) * 18.f, (itemCounter / m_inventory->width()) * 18.f, 0);

		itemCounter++;
	}
}

void InventoryWidget::updateItemWidgets(u16 offset, u16 size, std::string search)
{
	u16 itemCounter = 0;
	m_searchResultCount = 0;
	std::transform(search.begin(), search.end(), search.begin(), [](unsigned char c) { return std::tolower(c); });
	for (u16 i = 0; i < m_inventory->width() * m_inventory->height(); ++i) {
		u16 x = i % m_inventory->width();
		u16 y = i / m_inventory->width();
		std::string label = m_inventory->getStack(x, y).item().label();
		std::transform(label.begin(), label.end(), label.begin(), [](unsigned char c) { return std::tolower(c); });
		if (search.empty() || label.find(search) != std::string::npos) {
			if (m_searchResultCount++ < offset || itemCounter >= size) continue;
			m_itemWidgets[itemCounter].changeItem(x, y);
			m_itemWidgets[itemCounter].update();

			itemCounter++;
		}
	}

	for (u16 i = itemCounter; i < m_itemWidgets.size(); ++i)
		m_itemWidgets[i].disable();
}
