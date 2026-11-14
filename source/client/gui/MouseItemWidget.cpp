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
#include "common/core/EngineConfig.hpp"
#include "client/core/Config.hpp"
#include "client/gui/InventoryWidget.hpp"
#include "client/gui/MouseItemWidget.hpp"

MouseItemWidget::MouseItemWidget(Widget *parent) : ItemWidget(m_inventory, 0, 0, parent) {
	m_tooltipBackground.setFillColor(Color::fromRGBA32(0, 0, 0, 192));
	m_tooltipText.setColor(Color::White);
	m_tooltipText.setShadowColor(Color::fromRGBA32(63, 63, 63));
}

void MouseItemWidget::onEvent(const SDL_Event &event) {
	if (event.type == SDL_MOUSEMOTION) {
		updatePosition(event.motion.x, event.motion.y);
	}
	else if (event.type == SDL_MOUSEBUTTONDOWN) {
		updatePosition(event.button.x, event.button.y);
		if (event.button.button == SDL_BUTTON_MIDDLE && m_currentInventoryWidget
		 && m_currentInventoryWidget->isReadOnly() && m_currentItemWidget) {
			setStack(m_currentItemWidget->stack());
			return;
		}

		if (m_currentInventoryWidget && m_currentInventoryWidget->isOutput()
		 && (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT)) {
			stopDragging();
			ItemStack held = getStack();
			if (m_currentInventoryWidget->takeOutput(held)) setStack(held);
			return;
		}

		if (getStack().amount() == 0) {
			if (event.button.button == SDL_BUTTON_LEFT) {
				leftClickBehaviour();
			}
			else if (event.button.button == SDL_BUTTON_RIGHT) {
				rightClickBehaviour();
			}

			m_isDragging = false;
		}
		else if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT) {
			startDragging(event.button.button == SDL_BUTTON_LEFT);
		}
	}
	else if (event.type == SDL_MOUSEBUTTONUP) {
		updatePosition(event.button.x, event.button.y);

		if (m_isDragging && m_draggedSlots.size() <= 1) {
			if (event.button.button == SDL_BUTTON_LEFT) {
				leftClickBehaviour();
			}
			else if (event.button.button == SDL_BUTTON_RIGHT) {
				rightClickBehaviour();
			}
		}

		stopDragging();
	}
}

void MouseItemWidget::leftClickBehaviour() {
	if (m_currentInventoryWidget && m_currentInventoryWidget->isOutput()) return;
	if (m_currentInventoryWidget && m_currentInventoryWidget->currentItemWidget() && m_currentInventoryWidget->inventory()) {
		if (m_currentInventoryWidget->doItemMatchFilter(m_inventory.getStack(0, 0).item())) {
			ItemWidget *currentItemWidget = m_currentInventoryWidget->currentItemWidget();
			if (currentItemWidget->isEnabled()) {
				swapItems(*currentItemWidget, m_currentInventoryWidget->isReadOnly());

				m_currentInventoryWidget->sendUpdatePacket();
			}
		}
	}
}

void MouseItemWidget::rightClickBehaviour() {
	if (m_currentInventoryWidget && m_currentInventoryWidget->isOutput()) return;
	if (m_currentInventoryWidget && m_currentInventoryWidget->currentItemWidget() && m_currentInventoryWidget->inventory()) {
		if (m_currentInventoryWidget->isReadOnly() && !getStack().amount()) {
			auto *widget = m_currentInventoryWidget->currentItemWidget();
			if (widget->isEnabled() && widget->stack().amount()) {
				ItemStack item = widget->stack();
				item.setAmount(1);
				setStack(item);
			}
			return;
		}
		if (!m_currentInventoryWidget->isReadOnly()) {
			if (m_currentInventoryWidget->doItemMatchFilter(m_inventory.getStack(0, 0).item())) {
				ItemWidget *currentItemWidget = m_currentInventoryWidget->currentItemWidget();
				if (currentItemWidget->isEnabled()) {
					putItem(*currentItemWidget);
				}
			}

			m_currentInventoryWidget->sendUpdatePacket();
		}
	}
}

void MouseItemWidget::startDragging(bool isLeftClickDrag) {
	m_isDragging = true;
	m_isLeftClickDrag = isLeftClickDrag;
	m_draggedStack = getStack();
	m_dragOrigin = nullptr;

	if (m_currentInventoryWidget && !m_currentInventoryWidget->isReadOnly() && !m_currentInventoryWidget->isOutput()
	 && m_currentItemWidget && m_currentItemWidget->isEnabled()
	 && m_currentInventoryWidget->doItemMatchFilter(m_draggedStack.item())
	 && (m_currentItemWidget->stack().amount() == 0 || m_currentItemWidget->stack().item().stringID() == m_draggedStack.item().stringID())) {
		m_dragOrigin = m_currentItemWidget;
		m_draggedSlots[m_currentItemWidget] = m_currentItemWidget->stack();
	}
}

void MouseItemWidget::stopDragging() {
	m_isDragging = false;

	for (auto &it : m_draggedSlots)
		it.first->setChanged(true);

	m_draggedSlots.clear();
}

void MouseItemWidget::draggingBehaviour(ItemWidget *newItemWidget) {
	if (m_draggedStack.item().maxStackSize() == 1) return;
	if (newItemWidget->stack().amount() == 0 || newItemWidget->stack().item().stringID() == m_draggedStack.item().stringID()) {
		auto it = m_draggedSlots.find(newItemWidget);
		if (it == m_draggedSlots.end()) {
			if (m_draggedSlots.empty())
				m_dragOrigin = newItemWidget;

			m_draggedSlots.emplace(std::make_pair(newItemWidget, newItemWidget->stack()));
		}
	}

	if (m_draggedSlots.size() > 1) {
		if (m_isLeftClickDrag) {
			for (auto &it : m_draggedSlots) {
				u16 splitAmount = u16(m_draggedStack.amount() / m_draggedSlots.size());

				if (splitAmount > 0) {
					it.first->setStack(m_draggedStack.item().stringID(), it.second.amount() + splitAmount);
					it.first->update();
				}
			}

			u16 remainingAmount = (m_draggedStack.amount() == 1) ? 0 : m_draggedStack.amount() % std::min((u16)m_draggedSlots.size(), m_draggedStack.amount());

			setStack(remainingAmount ? m_draggedStack.item().stringID() : BLOCK_AIR, remainingAmount);
		}
		else if (m_draggedStack.amount() > 0 && (newItemWidget->stack().amount() == 0 || newItemWidget->stack().item().stringID() == m_draggedStack.item().stringID())) {
			if (m_dragOrigin) {
				m_dragOrigin->setStack(m_draggedStack.item().stringID(), m_dragOrigin->stack().amount() + 1);

				m_draggedStack.setAmount(m_draggedStack.amount() - 1);
				setStack(m_draggedStack.amount() ? m_draggedStack.item().stringID() : BLOCK_AIR, m_draggedStack.amount());

				m_dragOrigin = nullptr;
			}

			newItemWidget->setStack(m_draggedStack.item().stringID(), newItemWidget->stack().amount() + 1);

			m_draggedStack.setAmount(m_draggedStack.amount() - 1);
			setStack(m_draggedStack.amount() ? m_draggedStack.item().stringID() : BLOCK_AIR, m_draggedStack.amount());
		}
	}
}

void MouseItemWidget::updateCurrentItem(ItemWidget *currentItemWidget) {
	if (currentItemWidget && currentItemWidget->isEnabled()) {
		bool doItemMatchFilter = !m_currentInventoryWidget || (m_currentInventoryWidget->doItemMatchFilter(m_draggedStack.item()) && !m_currentInventoryWidget->isReadOnly() && !m_currentInventoryWidget->isOutput());
		if (m_isDragging && currentItemWidget != m_currentItemWidget && doItemMatchFilter)
			draggingBehaviour(currentItemWidget);

		m_currentItemWidget = (currentItemWidget->stack().item().id()) ? currentItemWidget : nullptr;
		const std::string &label = currentItemWidget->stack().item().label();
		auto start = label.find_first_not_of(" \t\r\n");
		std::string tooltip = start == std::string::npos ? "" : label.substr(start, label.find_last_not_of(" \t\r\n") - start + 1);
		const auto &stack = currentItemWidget->stack();
		if (Config::isItemDurabilityVisible && stack.amount() && stack.maxDamage())
			tooltip += "\nDurability: " + std::to_string(stack.maxDamage() - std::min(stack.damage(), stack.maxDamage())) + "/" + std::to_string(stack.maxDamage());
		m_tooltipText.setString(tooltip);
		m_tooltipText.updateVertexBuffer();
		float width = float(m_tooltipText.getSize().x);
		float x = 22, y = -2;
		float scaleX = m_parent->getScale().x, scaleY = m_parent->getScale().y;
		if (m_mouseX + (x - 10 + width + 3) * scaleX > Config::screenWidth) x -= width + 24;
		if (m_mouseY + (y - 13) * scaleY < 0) y = 22;
		m_tooltipBackground.setPosition(x - 3, y - 3);
		float height = float(m_tooltipText.getSize().y) + 6;
		if (m_tooltipBackground.width() != width + 6 || m_tooltipBackground.height() != height)
			m_tooltipBackground.setSize(width + 6, height);
		m_tooltipText.setPosition(x, y);
	}
	else {
		m_currentItemWidget = nullptr;
	}
}

void MouseItemWidget::swapItems(ItemWidget &widget, bool isReadOnly) {
	if (isReadOnly) {
		setStack(getStack().amount() ? ItemStack::Empty : widget.stack());
		return;
	}
	ItemStack held = stack();
	ItemStack target = widget.stack();
	if (!held.sameItem(target) || held.item().maxStackSize() == 1) {
		if (isReadOnly && held.amount()) return;
		widget.setStack(held);
		setStack(target);
		return;
	}
	unsigned maximum = held.item().maxStackSize();
	if (target.amount() >= maximum) return;
	u16 transferred = u16(std::min(unsigned(held.amount()), maximum - target.amount()));
	target.setAmount(target.amount() + transferred);
	held.setAmount(held.amount() - transferred);
	widget.setStack(target);
	setStack(held.amount() ? held : ItemStack::Empty);
}

void MouseItemWidget::putItem(ItemWidget &widget) {
	ItemStack held = stack();
	ItemStack target = widget.stack();
	if (!held.amount()) {
		held = target;
		held.setAmount((target.amount() + 1) / 2);
		target.setAmount(target.amount() / 2);
	} else if (!target.amount() || (held.sameItem(target) && target.amount() < target.item().maxStackSize())) {
		if (!target.amount()) { target = held; target.setAmount(0); }
		target.setAmount(target.amount() + 1);
		held.setAmount(held.amount() - 1);
	} else return;
	widget.setStack(target.amount() ? target : ItemStack::Empty);
	setStack(held.amount() ? held : ItemStack::Empty);
}

void MouseItemWidget::draw(RenderTarget &target, RenderStates states) const {
	if (getStack().amount() > 0)
		ItemWidget::draw(target, states);

	states.transform *= getTransform();

	if (!getStack().amount() && m_currentItemWidget && !m_tooltipText.string().empty()) {
		target.draw(m_tooltipBackground, states);
		target.draw(m_tooltipText, states);
	}
}

void MouseItemWidget::updatePosition(s32 x, s32 y) {
	m_mouseX = x;
	m_mouseY = y;
	float posX = (float)x - (m_parent->getPosition().x + 10 * m_parent->getScale().x);
	float posY = (float)y - (m_parent->getPosition().y + 10 * m_parent->getScale().y);

	setPosition(posX / m_parent->getScale().x, posY / m_parent->getScale().y, -20.f);
}
