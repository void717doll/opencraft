#include <algorithm>
#include "common/core/ApplicationStateStack.hpp"
#include "common/world/Block.hpp"
#include "client/core/Config.hpp"
#include "client/core/Mouse.hpp"
#include "client/network/Client.hpp"
#include "client/world/ClientWorld.hpp"
#include "client/states/SignEditorState.hpp"

SignEditorState::SignEditorState(Client &client, ClientWorld &world, Vector3i position,
	const std::array<std::string, 4> &lines, ApplicationState *parent)
	: InterfaceState(parent), m_client(client), m_world(world), m_position(position) {
	Mouse::setCursorGrabbed(false);
	Mouse::setCursorVisible(true);
	Mouse::resetToWindowCenter();
	m_title.setString("Edit Sign");
	m_hint.setString("Enter: save   Tab: next line   Esc: cancel");
	m_board.setFillColor(Color::fromRGBA32(126, 94, 52));
	for (int i = 0; i < 4; ++i) {
		m_lines[i].setString(lines[i]);
		m_lines[i].setCharacterLimit(15);
		m_lines[i].setBackgroundSize(120, 12);
		m_lines[i].setBackgroundColor(Color::fromRGBA32(0, 0, 0, 90));
		m_lines[i].setFocus(i == 0);
	}
	m_done.setText("Done");
	m_done.setCallback([this](TextButton &) { save(); });
	updateWidgetPosition();
}

void SignEditorState::close() {
	Mouse::setCursorGrabbed(true);
	Mouse::setCursorVisible(false);
	Mouse::resetToWindowCenter();
	m_stateStack->pop();
}

void SignEditorState::save() {
	Network::Packet packet;
	packet << Network::Command::SignUpdate << m_position;
	for (const auto &line : m_lines) packet << line.string();
	m_client.send(packet);
	close();
}

void SignEditorState::onEvent(const SDL_Event &event) {
	InterfaceState::onEvent(event);
	if (m_stateStack->empty() || &m_stateStack->top() != this) return;
	if (event.type == SDL_KEYDOWN) {
		if (event.key.keysym.sym == SDLK_ESCAPE) { close(); return; }
		if (event.key.keysym.sym == SDLK_RETURN || event.key.keysym.sym == SDLK_KP_ENTER) { save(); return; }
		if (event.key.keysym.sym == SDLK_TAB || event.key.keysym.sym == SDLK_DOWN || event.key.keysym.sym == SDLK_UP) {
			m_selected = (m_selected + (event.key.keysym.sym == SDLK_UP ? 3 : 1)) % 4;
			for (int i = 0; i < 4; ++i) m_lines[i].setFocus(i == m_selected);
			return;
		}
	}
	for (int i = 0; i < 4; ++i) {
		m_lines[i].onEvent(event);
		if (m_lines[i].hasFocus()) m_selected = i;
	}
	m_done.onEvent(event);
}

void SignEditorState::update() {
	InterfaceState::update();
	const auto *state = m_world.getBlockState(m_position.x, m_position.y, m_position.z);
	if ((!state || !state->block().hasGroup("group:om_sign")) && !m_stateStack->empty() && &m_stateStack->top() == this)
		close();
}

void SignEditorState::updateWidgetPosition() {
	float scale = float(std::max(1, std::min({int(Config::guiScale), int(Config::screenWidth) / 300, int(Config::screenHeight) / 180})));
	float x = Config::screenWidth / 2.f, y = Config::screenHeight / 2.f;
	m_board.setPosition(x - 72 * scale, y - 38 * scale);
	m_board.setSize(144 * scale, 72 * scale);
	m_title.setScale(scale, scale);
	m_title.updateVertexBuffer();
	m_title.setPosition(x - m_title.getSize().x * scale / 2, y - 62 * scale);
	m_hint.setScale(scale, scale);
	m_hint.updateVertexBuffer();
	m_hint.setPosition(x - m_hint.getSize().x * scale / 2, y + 43 * scale);
	for (int i = 0; i < 4; ++i) {
		m_lines[i].setScale(scale, scale);
		m_lines[i].setPosition(x - 60 * scale, y + (-29 + i * 15) * scale);
	}
	m_done.setScale(scale, scale);
	m_done.setPosition(x - m_done.getGlobalBounds().sizeX / 2, y + 61 * scale);
}

void SignEditorState::draw(RenderTarget &target, RenderStates states) const {
	if (auto *parent = dynamic_cast<Drawable *>(m_parent)) target.draw(*parent, states);
	prepareDraw(target, states);
	target.draw(m_board, states);
	target.draw(m_title, states);
	target.draw(m_hint, states);
	for (const auto &line : m_lines) target.draw(line, states);
	target.draw(m_done, states);
}
