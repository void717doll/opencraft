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
#include <glm/gtc/matrix_transform.hpp>

#include "common/core/ApplicationStateStack.hpp"
#include "common/core/Color.hpp"
#include "common/core/Debug.hpp"
#include "common/core/LuaWidget.hpp"
#include "common/core/Registry.hpp"
#include "common/core/input/GameKey.hpp"
#include "common/network/Network.hpp"
#include "common/network/NetworkUtils.hpp"
#include "common/world/Player.hpp"
#include "common/world/Chest.hpp"

#include "client/core/Config.hpp"
#include "client/core/Mouse.hpp"
#include "client/core/input/GamePad.hpp"
#include "client/graphics/NinePatch.hpp"
#include "client/gui/InventoryWidget.hpp"
#include "client/gui/ProgressBarWidget.hpp"
#include "client/gui/ScrollBarWidget.hpp"
#include "client/gui/TextButton.hpp"
#include "client/network/ClientCommandHandler.hpp"
#include "client/states/LuaGUIState.hpp"
#include "client/world/ClientPlayer.hpp"
#include "client/world/ClientWorld.hpp"

bool LuaGUIState::isActive = false;

LuaGUIState::LuaGUIState(ClientCommandHandler &client, ClientPlayer &player, ClientWorld &world, sf::Packet &packet, ApplicationState *parent)
	: InterfaceState(parent), m_client(client), m_player(player), m_world(world)
{
	Mouse::setCursorGrabbed(false);
	Mouse::setCursorVisible(true);
	Mouse::resetToWindowCenter();

	m_mainWidget.setScale(Config::guiScale, Config::guiScale);

	packet >> m_width >> m_height >> m_isCentered >> m_keyID;

	if (m_isCentered)
		centerMainWidget();

	while (!packet.endOfPacket())
		loadGUI(packet);
	for (auto &entry : m_inventories)
		if (entry.second.name() == "minecart") m_client.setMinecartInventory(&entry.second);
	if (m_inventoryWidgets.count("armor_helmet") && !m_inventoryWidgets.count("inv_catalog")) m_playerPreview = std::make_unique<PlayerModel>(m_player);

	isActive = true;
}

LuaGUIState::~LuaGUIState() {
	for (auto &entry : m_inventories) m_client.clearMinecartInventory(&entry.second);
}

void LuaGUIState::onEvent(const SDL_Event &event) {
	if (!blockInventoriesValid()) return;
	if (event.type == SDL_QUIT) finishInventory();
	InterfaceState::onEvent(event);

	if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED && m_isCentered)
		centerMainWidget();

	if (event.type == SDL_KEYDOWN && (event.key.keysym.sym == SDLK_ESCAPE
	 || (m_keyID >= 0 && event.key.keysym.sym == Registry::getInstance().getKey(m_keyID).keycode()))) {
		bool ignoreExit = false;
		for (auto &it : m_textInputs) {
			if (it.second.hasFocus()) {
				ignoreExit = true;

				if (event.key.keysym.sym == SDLK_ESCAPE)
					it.second.setFocus(false);
			}
		}

		if (!ignoreExit) {
			finishInventory();
			Mouse::setCursorGrabbed(true);
			Mouse::setCursorVisible(false);
			Mouse::resetToWindowCenter();

			isActive = false;

			m_stateStack->pop();
			return;
		}
	}

	for (auto &it : m_widgets)
		it->onEvent(event);

	if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
	 && m_currentInventoryWidget && !m_currentInventoryWidget->shiftDestination().empty()
	 && m_mouseItemWidget.currentItemWidget() && GamePad::isKeyPressed(GameKey::Shift)) {
		const ItemWidget *source = m_mouseItemWidget.currentItemWidget();
		bool finished = false;
		// fill existing stacks across every destination before using empty slots.
		for (bool mergeOnly : {true, false}) {
			for (const std::string &shiftDestination : m_currentInventoryWidget->shiftDestination()) {
				AbstractInventoryWidget *dest = nullptr;
				auto crafting = m_craftingWidgets.find(shiftDestination);
				if (crafting != m_craftingWidgets.end()) dest = &crafting->second;
				auto inventory = m_inventoryWidgets.find(shiftDestination);
				if (!dest && inventory != m_inventoryWidgets.end()) dest = &inventory->second;
				if (!dest) {
					logWarning() << "Destination not found:" << shiftDestination;
					continue;
				}
				if (m_currentInventoryWidget->sendItemStackToDest(source, dest, mergeOnly)
				 && (!source->stack().amount() || m_currentInventoryWidget->isReadOnly() || m_currentInventoryWidget->isOutput())) {
					finished = true;
					break;
				}
			}
			if (finished) break;
		}
	}
	else {
		for (auto &it : m_inventoryWidgets)
			it.second.onEvent(event);

		for (auto &it : m_craftingWidgets)
			it.second.onEvent(event);

		m_mouseItemWidget.onEvent(event);
	}

	for (auto &it : m_textInputs)
		it.second.onEvent(event);

	// keep the result current between input events, including shift-clicks.
	for (auto &it : m_craftingWidgets)
		it.second.update();
}

void LuaGUIState::update() {
	InterfaceState::update();
	if (!m_player.health() || !blockInventoriesValid()) {
		finishInventory();
		Mouse::setCursorGrabbed(true);
		Mouse::setCursorVisible(false);
		Mouse::resetToWindowCenter();
		isActive = false;
		m_stateStack->pop();
		return;
	}

	for (auto &it : m_textInputInventories) {
		auto inv = m_inventoryWidgets.find(it.second);
		if (inv != m_inventoryWidgets.end()) {
			inv->second.applySearch(m_textInputs.at(it.first).string());
		}
		else
			logError() << "Can't find linked inventory" << it.second << "for text input" << it.first;
	}

	for (auto &it : m_widgets)
		it->update();

	for (auto &it : m_craftingWidgets) {
		it.second.update();
	}

	for (auto &it : m_inventoryWidgets) {
		it.second.update();
	}

	ItemWidget *currentItemWidget = nullptr;
	m_currentInventoryWidget = nullptr;
	for (auto &it : m_inventoryWidgets) {
		if (!currentItemWidget && ((currentItemWidget = it.second.currentItemWidget()))) {
			m_currentInventoryWidget = &it.second;
		}
	}
	for (auto &it : m_craftingWidgets) {
		if (!currentItemWidget && ((currentItemWidget = it.second.currentItemWidget()))) {
			m_currentInventoryWidget = it.second.currentInventoryWidget();
		}
	}

	m_mouseItemWidget.setCurrentInventoryWidget(m_currentInventoryWidget);
	m_mouseItemWidget.updateCurrentItem(currentItemWidget);
}

bool LuaGUIState::blockInventoriesValid() const {
	for (const auto &entry : m_inventories) {
		if (entry.second.name() == "minecart" && !m_client.minecartInReach(u32(entry.second.blockPos().x))) return false;
	}
	for (const auto &source : m_blockInventories) {
		const auto &pos = source.position;
		const auto *data = m_world.getBlockData(pos.x, pos.y, pos.z);
		if (!data || &data->inventory != source.inventory || m_world.getBlock(pos.x, pos.y, pos.z) != source.block) return false;
		if (Registry::getInstance().getBlock(source.block).hasGroup("group:om_chest")) {
			const auto halves = chest::halves(m_world, pos);
			if (halves.size() != m_blockInventories.size() || !chest::canOpen(m_world, halves)) return false;
			for (const auto &half : halves) {
				if (std::none_of(m_blockInventories.begin(), m_blockInventories.end(), [&](const auto &entry) {
					return entry.position == half;
				})) return false;
			}
		}
	}
	return true;
}

void LuaGUIState::finishInventory() {
	if (m_isFinished) return;
	m_isFinished = true;
	m_mouseItemWidget.stopDragging();
	std::vector<ItemStack> drops;
	auto release = [&](const ItemStack &stack) {
		if (!stack.amount()) return;
		ItemStack remaining = m_player.health() ? m_player.inventory().addItemStack(stack) : stack;
		if (remaining.amount()) drops.push_back(remaining);
	};
	release(m_mouseItemWidget.getStack());
	m_mouseItemWidget.setStack(ItemStack::Empty);
	auto crafting = m_inventories.find("_temp");
	if (crafting != m_inventories.end()) {
		for (const auto &stack : crafting->second.items()) release(stack);
		for (u16 y = 0; y < crafting->second.height(); ++y)
			for (u16 x = 0; x < crafting->second.width(); ++x) crafting->second.clearStack(x, y);
	}
	for (auto &entry : m_inventories) {
		if (entry.second.name() != "minecart") continue;
		if (m_client.minecartInReach(u32(entry.second.blockPos().x))) m_client.sendMinecartInvUpdate(entry.second);
		m_client.clearMinecartInventory(&entry.second);
	}
	m_client.sendPlayerInvUpdate();
	for (const auto &stack : drops) m_client.sendPlayerDropStack(stack);
}

void LuaGUIState::draw(RenderTarget &target, RenderStates states) const {
	if (m_parent)
		target.draw(*(DrawableState *)m_parent, states);
	if (!blockInventoriesValid()) return;

	prepareDraw(target, states);

	states.transform *= m_mainWidget.getTransform();
	states.viewMatrix = Transform::Identity;

	for (auto &it : m_drawables)
		target.draw(*it, states);
	if (m_playerPreview) {
		RenderStates preview = states;
		preview.transform.translate(57, 75, -40);
		preview.transform.scale(28, 28, 28);
		preview.transform.rotateX(90);
		preview.transform.rotateZ(-20);
		target.draw(*m_playerPreview, preview);
	}

	for (auto &it : m_widgets)
		target.draw(*it, states);

	for (auto &it : m_inventoryWidgets)
		target.draw(it.second, states);

	for (auto &it : m_craftingWidgets)
		target.draw(it.second, states);

	for (auto &it : m_textInputs)
		target.draw(it.second, states);

	target.draw(m_mouseItemWidget, states);
}

void LuaGUIState::loadGUI(sf::Packet &packet) {
	u8 type;
	std::string name;
	packet >> type >> name;

	s32 x, y;
	if (type != LuaWidget::Inventory)
		packet >> x >> y;

	if (type == LuaWidget::Image)
		loadImage(name, x, y, packet);
	else if (type == LuaWidget::TextButton)
		loadTextButton(name, x, y, packet);
	else if (type == LuaWidget::InventoryWidget)
		loadInventoryWidget(name, x, y, packet);
	else if (type == LuaWidget::ProgressBarWidget)
		loadProgressBarWidget(name, x, y, packet);
	else if (type == LuaWidget::CraftingWidget)
		loadCraftingWidget(name, x, y, packet);
	else if (type == LuaWidget::ScrollBarWidget)
		loadScrollBarWidget(name, x, y, packet);
	else if (type == LuaWidget::TextInput)
		loadTextInput(name, x, y, packet);
	else if (type == LuaWidget::Inventory)
		loadInventory(name, packet);
}

void LuaGUIState::loadImage(const std::string &, s32 x, s32 y, sf::Packet &packet) {
	std::string textureFilename;
	FloatRect clipRect;
	u16 width, height, border;
	packet >> textureFilename >> clipRect.x >> clipRect.y >> clipRect.sizeX >> clipRect.sizeY
		>> width >> height >> border;

	Texture &texture = loadTexture(textureFilename);
	if (border && width && height) {
		auto *panel = new NinePatch(texture, border);
		panel->setPosition(float(x), float(y));
		panel->setSize(width, height);
		m_drawables.emplace_back(panel);
		return;
	}

	auto *image = new Image(texture);
	image->setPosition((float)x, (float)y);
	image->setClipRect(clipRect.x, clipRect.y, (u16)clipRect.sizeX, (u16)clipRect.sizeY);
	if (width && height) image->setPosRect(0, 0, width, height);
	m_drawables.emplace_back(image);
}

void LuaGUIState::loadTextButton(const std::string &, s32 x, s32 y, sf::Packet &packet) {
	std::string text;
	packet >> text;

	auto *button = new TextButton(&m_mainWidget);
	button->setPosition((float)x, (float)y);
	// button->setCallback(it.on_click);
	button->setText(text);
	m_widgets.emplace_back(button);
}

void LuaGUIState::loadInventoryWidget(const std::string &name, s32 x, s32 y, sf::Packet &packet) {
	std::string inventory, shiftDestination, filter;
	u16 width, height;
	u16 offset, count;
	bool isReadOnly, isOutput;
	packet >> width >> height >> shiftDestination >> offset >> count >> inventory
		>> filter >> isReadOnly >> isOutput;

	Inventory *widgetInventory = nullptr;
	if (inventory == "player") {
		std::string playerName, inventoryName;
		packet >> playerName >> inventoryName;

		if (inventoryName == "armor") widgetInventory = &m_player.armor();
		else if (inventoryName == "offhand") widgetInventory = &m_player.offhand();
		else widgetInventory = &m_player.inventory();
	}
	else if (inventory == "block") {
		Vector3i block;
		packet >> block;

		BlockData *data = m_world.getBlockData(block.x, block.y, block.z);
		if (!data) {
			logError() << "No inventory found at" << block.x << block.y << block.z;
			return;
		}

		widgetInventory = &data->inventory;
		m_blockInventories.push_back({block, widgetInventory, m_world.getBlock(block.x, block.y, block.z)});
	}
	else if (inventory == "temp") {
		std::string inventoryName;
		packet >> inventoryName;

		if (inventoryName.empty()) {
			m_inventories.emplace("_temp", Inventory{width, height});
			widgetInventory = &m_inventories.at("_temp");
		}
		else {
			auto it = m_inventories.find(inventoryName);
			if (it == m_inventories.end())
				logError() << "Unable to find inventory" << inventoryName << "for widget" << name;

			widgetInventory = &it->second;
		}
	}

	if (inventory == "catalog" && m_player.isCreative()) {
		std::vector<const Item *> items;
		for (const auto &item : Registry::getInstance().items()) {
			if (!item->id() || item->hasGroup("group:om_creative_hidden")) continue;
			if (item->isBlock() && Registry::getInstance().getBlockFromStringID(item->stringID()).getState(0).drawType() == BlockDrawType::Liquid) continue;
			items.push_back(item.get());
		}
		std::sort(items.begin(), items.end(), [](const Item *a, const Item *b) { return a->label() < b->label(); });
		m_inventories.emplace("_catalog", Inventory{9, u16(std::max<std::size_t>(5, (items.size() + 8) / 9))});
		widgetInventory = &m_inventories.at("_catalog");
		for (std::size_t i = 0; i < items.size(); ++i)
			widgetInventory->setStack(u16(i % 9), u16(i / 9), items[i]->stringID(), items[i]->maxStackSize());
		isReadOnly = true;
	}

	if (widgetInventory) {
		m_inventoryWidgets.emplace(name, InventoryWidget{m_client, isReadOnly && !isOutput, &m_mainWidget});

		auto &inventoryWidget = m_inventoryWidgets.at(name);
		inventoryWidget.setPosition((float)x, (float)y);
		inventoryWidget.init(*widgetInventory, offset, count);
		inventoryWidget.setShiftDestination(shiftDestination);
		inventoryWidget.setFilter(filter);
		if (isOutput) {
			inventoryWidget.setOutputHandler([&inventoryWidget] {
				inventoryWidget.sendUpdatePacket();
			});
		}
	}
	else {
		logError() << "Inventory widget" << name << " is invalid";
	}
}

void LuaGUIState::loadCraftingWidget(const std::string &name, s32 x, s32 y, sf::Packet &packet) {
	std::string inventory, shiftDestination;
	s32 resultX, resultY;
	packet >> resultX >> resultY >> shiftDestination >> inventory;

	u16 offset = 0, size;
	Inventory *craftingInventory = nullptr;
	if (inventory == "block") {
		Vector3i block;
		packet >> block >> offset >> size;

		BlockData *data = m_world.getBlockData(block.x, block.y, block.z);
		if (!data) {
			logError() << "No inventory found at" << block.x << block.y << block.z;
		}
		else {
			craftingInventory = &data->inventory;
		}
	}
	else if (inventory == "temp") {
		packet >> size;

		m_inventories.emplace("_temp", Inventory{size, size});
		craftingInventory = &m_inventories.at("_temp");
	}

	if (craftingInventory) {
		m_craftingWidgets.emplace(name, CraftingWidget{m_client, *craftingInventory, &m_mainWidget});

		auto &craftingWidget = m_craftingWidgets.at(name);
		craftingWidget.setShiftDestination(shiftDestination);
		craftingWidget.init(offset, size);
		craftingWidget.craftingInventoryWidget().setPosition((float)x, (float)y);
		craftingWidget.craftingResultInventoryWidget().setPosition((float)resultX, (float)resultY);
	}
	else {
		logError() << "Crafting inventory is invalid";
	}
}

void LuaGUIState::loadProgressBarWidget(const std::string &, s32 x, s32 y, sf::Packet &packet) {
	u8 type;
	Vector3i block;
	std::string meta, maxMeta;
	u32 maxValue;
	std::string textureFilename;
	FloatRect clipRect;
	packet >> type >> block.x >> block.y >> block.z >> meta >> maxMeta >> maxValue >> textureFilename
		>> clipRect.x >> clipRect.y >> clipRect.sizeX >> clipRect.sizeY;

	BlockData *data = m_world.getBlockData(block.x, block.y, block.z);
	if (!data) {
		logError() << "No inventory found at" << block.x << block.y << block.z;
		return;
	}

	Texture &texture = loadTexture(textureFilename);

	ProgressBarWidget *widget = new ProgressBarWidget(texture, *data, ProgressBarType(type));
	if (!maxMeta.empty())
		widget->init(clipRect, Vector2i{x, y}, meta, maxMeta);
	else
		widget->init(clipRect, Vector2i{x, y}, meta, maxValue);

	m_widgets.emplace_back(widget);
}

void LuaGUIState::loadScrollBarWidget(const std::string &, s32 x, s32 y, sf::Packet &packet) {
	std::string textureFilename;
	FloatRect clipRect;
	u16 minY, maxY;
	std::string widget;
	packet >> textureFilename >> clipRect >> minY >> maxY >> widget;

	Texture &texture = loadTexture(textureFilename);

	ScrollBarWidget *scrollBarWidget = new ScrollBarWidget(&m_mainWidget);
	scrollBarWidget->setPosition((float)x, (float)y);
	scrollBarWidget->init(texture, clipRect, minY, maxY, m_inventoryWidgets.at(widget));

	m_widgets.emplace_back(scrollBarWidget);
}

void LuaGUIState::loadTextInput(const std::string &name, s32 x, s32 y, sf::Packet &packet) {
	u16 width, height;
	std::string placeholder, inventory;
	Color placeholderColor;
	packet >> width >> height >> placeholder >> placeholderColor >> inventory;

	TextInput textInput{&m_mainWidget};
	textInput.setPosition((float)x, (float)y);
	textInput.setFocus(false);
	textInput.setBackgroundSize(width, height);
	textInput.setPlaceholder(placeholder);
	textInput.setPlaceholderColor(placeholderColor);

	m_textInputs.emplace(name, std::move(textInput));
	m_textInputInventories.emplace(name, inventory);
}

void LuaGUIState::loadInventory(const std::string &name, sf::Packet &packet) {
	m_inventories.emplace(name, Inventory{});

	packet >> m_inventories.at(name);
}

Texture &LuaGUIState::loadTexture(const std::string &textureFilename) {
	auto it = m_textures.find(textureFilename);
	if (it == m_textures.end()) {
		m_textures.emplace(textureFilename, Texture{}).first->second.loadFromFile(textureFilename);
		return m_textures.at(textureFilename);
	}
	else {
		return it->second;
	}
}

void LuaGUIState::centerMainWidget() {
	float scale = std::max(1.f, std::min(float(Config::guiScale), std::floor(std::min(
		float(Config::screenWidth - 16) / m_width, float(Config::screenHeight - 16) / m_height))));
	m_mainWidget.setScale(scale, scale);
	float x = floorf(Config::screenWidth / 2.f - m_width * scale / 2.f + 0.5f);
	float y = floorf(Config::screenHeight / 2.f - m_height * scale / 2.f + 0.5f);
	m_mainWidget.setPosition(x, y);
}
