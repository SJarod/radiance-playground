#include <GLFW/glfw3.h>

#include "engine/transform.hpp"

#include "input_manager.hpp" 
#include "move_camera.hpp"
#include "imgui.h"

void MoveCamera::setFocus(bool newFocus) 
{
    m_isFocused = newFocus;

    int inputModeValue = m_isFocused ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL;
    glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, inputModeValue);
}

void MoveCamera::init(void *userData)
{
    auto data = (UserDataT *)userData;
    m_window = &data->window;
    m_mainCamera = &data->camera;
}

void MoveCamera::begin()
{
    setFocus(true);
    glfwGetCursorPos(m_window->getHandle(), &m_mousePos.first, &m_mousePos.second);
}

void MoveCamera::update(float deltaTime)
{
    if (InputManager::GetKeyDown(Keycode::ESCAPE))
    {
        setFocus(!m_isFocused);
    }

    double xpos, ypos;
    glfwGetCursorPos(m_window->getHandle(), &xpos, &ypos);
    std::pair<double, double> deltaMousePos;
    deltaMousePos.first = m_mousePos.first - xpos;
    deltaMousePos.second = m_mousePos.second - ypos;
    m_mousePos.first = xpos;
    m_mousePos.second = ypos;

    if (!m_isFocused)
        return;

    float pitch = (float)deltaMousePos.second * m_mainCamera->getSensitivity() * deltaTime;
    float yaw = (float)deltaMousePos.first * m_mainCamera->getSensitivity() * deltaTime;
    Transform cameraTransform = m_mainCamera->getTransform();

    cameraTransform.rotation =
        glm::quat(glm::vec3(-pitch, 0.f, 0.f)) * cameraTransform.rotation * glm::quat(glm::vec3(0.f, -yaw, 0.f));

    float xaxisInput = (glfwGetKey(m_window->getHandle(), GLFW_KEY_A) == GLFW_PRESS) -
                       (glfwGetKey(m_window->getHandle(), GLFW_KEY_D) == GLFW_PRESS);
    float zaxisInput = (glfwGetKey(m_window->getHandle(), GLFW_KEY_W) == GLFW_PRESS) -
                       (glfwGetKey(m_window->getHandle(), GLFW_KEY_S) == GLFW_PRESS);
    float yaxisInput = (glfwGetKey(m_window->getHandle(), GLFW_KEY_Q) == GLFW_PRESS) -
                       (glfwGetKey(m_window->getHandle(), GLFW_KEY_E) == GLFW_PRESS);
    glm::vec3 dir = glm::vec3(xaxisInput, yaxisInput, zaxisInput) * cameraTransform.rotation;
    if (!(xaxisInput == 0.f && zaxisInput == 0.f && yaxisInput == 0.f))
        dir = glm::normalize(dir);
    cameraTransform.position += m_mainCamera->getSpeed() * dir * deltaTime;

    m_mainCamera->setTransform(cameraTransform);
}