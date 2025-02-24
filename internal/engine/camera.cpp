#include <glm/gtc/matrix_transform.hpp>

#include "camera.hpp"

glm::mat4 CameraABC::getViewMatrix() const
{
    glm::mat4 identity = glm::identity<glm::mat4>();

    glm::mat4 t = glm::translate(identity, m_transform.position);
    glm::mat4 r = glm::mat4_cast(m_transform.rotation);

    return r * t;
}

glm::mat4 PerspectiveCamera::getProjectionMatrix() const
{
    glm::mat4 proj = glm::perspective(glm::radians(m_yFov), m_aspectRatio, m_near, m_far);
    if (m_bYFlip)
        proj[1][1] *= -1;
    return proj;
}

glm::mat4 OrthographicCamera::getProjectionMatrix() const
{
    // TODO : fix matrices with GLM (defines)
    glm::mat4 proj = glm::orthoLH_ZO(m_left, m_right, m_bottom, m_top, m_near, m_far);
    if (m_bYFlip)
        proj[1][1] *= -1;
    return proj;
}