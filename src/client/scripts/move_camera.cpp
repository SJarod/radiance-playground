#include <GLFW/glfw3.h>

#include "engine/transform.hpp"

#include "imgui.h"
#include "input_manager.hpp"
#include "move_camera.hpp"

void MoveCamera::setFocus(bool newFocus)
{
    m_isFocused = newFocus;

    if (m_isFocused)
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
    else
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;

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
    setFocus(false);
    glfwGetCursorPos(m_window->getHandle(), &m_mousePos.first, &m_mousePos.second);
}

void MoveCamera::update(float deltaTime)
{
    double xpos, ypos;
    glfwGetCursorPos(m_window->getHandle(), &xpos, &ypos);
    std::pair<double, double> deltaMousePos;
    deltaMousePos.first = m_mousePos.first - xpos;
    deltaMousePos.second = m_mousePos.second - ypos;
    m_mousePos.first = xpos;
    m_mousePos.second = ypos;

    if (glfwGetMouseButton(m_window->getHandle(), GLFW_MOUSE_BUTTON_1) == GLFW_PRESS)
    {
        Transform cameraTransform = m_mainCamera->getTransform();

        glm::vec3 forward = glm::normalize(glm::vec3(0.f, 0.f, -1.f) * cameraTransform.rotation);
        glm::vec3 target = cameraTransform.position + forward * 10.f;
        glm::vec3 dir = glm::normalize(-target);

        float pitchDelta = (float)deltaMousePos.second * m_mainCamera->getSensitivity() * deltaTime;
        float yawDelta = (float)deltaMousePos.first * m_mainCamera->getSensitivity() * deltaTime;

        float pitch = glm::eulerAngles(cameraTransform.rotation).x;
        float yaw = glm::eulerAngles(cameraTransform.rotation).y;
        glm::quat QuatAroundX = glm::angleAxis(pitch + pitchDelta, glm::vec3(1.0, 0.0, 0.0));
        glm::quat QuatAroundY = glm::angleAxis(yaw + yawDelta, glm::vec3(0.0, 1.0, 0.0));
        glm::quat finalOrientation = QuatAroundX * QuatAroundY;

        cameraTransform.position = target + dir * 10.f;
        cameraTransform.rotation =
            glm::quat_cast(glm::lookAt(cameraTransform.position, target, glm::vec3(0.f, 1.f, 0.f)));

        m_mainCamera->setTransform(cameraTransform);
        return;
    }

    if (glfwGetMouseButton(m_window->getHandle(), GLFW_MOUSE_BUTTON_2) == GLFW_PRESS)
    {
        setFocus(true);
        m_cameraSpeedMultiplier += deltaTime * 5.f;
    }
    else
    {
        setFocus(false);
        m_cameraSpeedMultiplier = 1.f;
    }

    if (InputManager::GetKeyDown(Keycode::ESCAPE))
        setFocus(!m_isFocused);

    if (!m_isFocused)
        return;

    float isFastMovementActive = InputManager::GetKey(Keycode::LEFT_SHIFT) ? 3.f : 1.f;

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
    cameraTransform.position +=
        m_mainCamera->getSpeed() * dir * deltaTime * isFastMovementActive * m_cameraSpeedMultiplier;

    m_mainCamera->setTransform(cameraTransform);
}