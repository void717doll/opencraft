#ifndef SIGNEDITORSTATE_HPP_
#define SIGNEDITORSTATE_HPP_

#include <array>
#include "client/gui/TextInput.hpp"
#include "client/gui/TextButton.hpp"
#include "client/states/InterfaceState.hpp"

class Client;
class ClientWorld;
class SignEditorState : public InterfaceState {
	public:
		SignEditorState(Client &client, ClientWorld &world, Vector3i position,
			const std::array<std::string, 4> &lines, ApplicationState *parent);
		void onEvent(const SDL_Event &event) override;
		void update() override;
	private:
		void close();
		void save();
		void updateWidgetPosition() override;
		void draw(RenderTarget &target, RenderStates states) const override;
		Client &m_client;
		ClientWorld &m_world;
		Vector3i m_position;
		std::array<TextInput, 4> m_lines;
		Text m_title;
		Text m_hint;
		TextButton m_done;
		RectangleShape m_board;
		int m_selected = 0;
};
#endif
