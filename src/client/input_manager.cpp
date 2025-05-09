#include <tracy/Tracy.hpp>

#include "input_manager.hpp"

InputManager* InputManager::currentInstance = nullptr;

void InputManager::InputState::Set(bool down, bool held, bool up)
{
	m_isDown = down;
	m_isHeld = held;
	m_isUp = up;
}

InputManager::InputManager() 
{
	currentInstance = this;

	for (auto& key : m_keys)
		key.second.Set(false, false, false);
}


void InputManager::KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// action = 1 if pressed and 0 if released
	if (action == 2)	return;

	InputManager& IM = *currentInstance;

	InputState& input = IM.m_keys[(Keycode)key];
	input.Set(action, action, !action);
	IM.m_framePressedKeys.push_back((Keycode)key);
}

void InputManager::UpdateInputStates()
{
    ZoneScoped;

	for (auto& key : m_framePressedKeys)
	{
		InputState& input = m_keys[key];
		input.Set(false, input.Held(), false);
	}

	m_framePressedKeys.clear();
}

bool InputManager::GetKeyDown(Keycode key)
{
	return currentInstance->m_keys[key].Down();
}

bool InputManager::GetKey(Keycode key)
{
	return currentInstance->m_keys[key].Held();
}

bool InputManager::GetKeyUp(Keycode key)
{
	return currentInstance->m_keys[key].Up();
}