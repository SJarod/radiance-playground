#pragma once

#include <vector>
#include <unordered_map>

#include "keycode.hpp"

class InputManager
{
private:
	static InputManager* currentInstance;

	class InputState
	{
	private:
		bool	m_isDown = false;
		bool	m_isHeld = false;
		bool	m_isUp = false;

	public:
		void Set(bool down, bool held, bool up);

		//Return true if input is pressed
		bool Down() { return m_isDown; }

		//Return true if input is held
		bool Held() { return m_isHeld; }

		//Return true if input is released
		bool Up() { return m_isUp; }
	};

	std::vector<Keycode> keycodes =
	{
		Keycode::UNKNOWN,
		Keycode::SPACE,
		Keycode::APOSTROPHE,
		Keycode::COMMA,
		Keycode::MINUS,
		Keycode::PERIOD,
		Keycode::SLASH,

		Keycode::NUM_0,
		Keycode::NUM_1,
		Keycode::NUM_2,
		Keycode::NUM_3,
		Keycode::NUM_4,
		Keycode::NUM_5,
		Keycode::NUM_6,
		Keycode::NUM_7,
		Keycode::NUM_8,
		Keycode::NUM_9,

		Keycode::SEMICOLON,
		Keycode::EQUAL,

		Keycode::A,
		Keycode::B,
		Keycode::C,
		Keycode::D,
		Keycode::E,
		Keycode::F,
		Keycode::G,
		Keycode::H,
		Keycode::I,
		Keycode::J,
		Keycode::K,
		Keycode::L,
		Keycode::M,
		Keycode::N,
		Keycode::O,
		Keycode::P,
		Keycode::Q,
		Keycode::R,
		Keycode::S,
		Keycode::T,
		Keycode::U,
		Keycode::V,
		Keycode::W,
		Keycode::X,
		Keycode::Y,
		Keycode::Z,

		Keycode::LEFT_BRACKET,
		Keycode::BACKSLASH,
		Keycode::RIGHT_BRACKET,
		Keycode::GRAVE_ACCENT,

		Keycode::ESCAPE,
		Keycode::ENTER,
		Keycode::TAB,
		Keycode::BACKSPACE,
		Keycode::INSERT,
		Keycode::DEL,
		Keycode::RIGHT,
		Keycode::LEFT,
		Keycode::DOWN,
		Keycode::UP,
		Keycode::PAGE_UP,
		Keycode::PAGE_DOWN,
		Keycode::HOME,
		Keycode::END,
		Keycode::CAPS_LOCK,
		Keycode::SCROLL_LOCK,
		Keycode::NUM_LOCK,
		Keycode::PRINT_SCREEN,
		Keycode::PAUSE,

		Keycode::F1,
		Keycode::F2,
		Keycode::F3,
		Keycode::F4,
		Keycode::F5,
		Keycode::F6,
		Keycode::F7,
		Keycode::F8,
		Keycode::F9,
		Keycode::F10,
		Keycode::F11,
		Keycode::F12,
		Keycode::F13,
		Keycode::F14,
		Keycode::F15,
		Keycode::F16,
		Keycode::F17,
		Keycode::F18,
		Keycode::F19,
		Keycode::F20,
		Keycode::F21,
		Keycode::F22,
		Keycode::F23,
		Keycode::F24,
		Keycode::F25,

		Keycode::KP_0,
		Keycode::KP_1,
		Keycode::KP_2,
		Keycode::KP_3,
		Keycode::KP_4,
		Keycode::KP_5,
		Keycode::KP_6,
		Keycode::KP_7,
		Keycode::KP_8,
		Keycode::KP_9,

		Keycode::KP_DECIMAL,
		Keycode::KP_DIVIDE,
		Keycode::KP_MULTIPLY,
		Keycode::KP_SUBTRACT,
		Keycode::KP_ADD,
		Keycode::KP_ENTER,
		Keycode::KP_EQUAL,

		Keycode::LEFT_SHIFT,
		Keycode::LEFT_CONTROL,
		Keycode::LEFT_ALT,
		Keycode::LEFT_SUPER,
		Keycode::RIGHT_SHIFT,
		Keycode::RIGHT_CONTROL,
		Keycode::RIGHT_ALT,
		Keycode::RIGHT_SUPER,

		Keycode::MENU,

		Keycode::LEFT_CLICK,
		Keycode::RIGHT_CLICK,
		Keycode::MIDDLE_CLICK
	};

	std::vector<Keycode> m_framePressedKeys;
	std::unordered_map<Keycode, InputState> m_keys;

public:
	InputManager();
	
	void UpdateInputStates();
	
	static bool GetKeyDown(Keycode key);
	static bool GetKey(Keycode key);
	static bool GetKeyUp(Keycode key);

	static void KeyCallback(struct GLFWwindow* window, int key, int scancode, int action, int mods);
};