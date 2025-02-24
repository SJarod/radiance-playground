#pragma once

#include <glm/glm.hpp>

#include "transform.hpp"

class CameraABC
{
  protected:
    Transform m_transform;

    float m_near = 0.1f;
    float m_far = 1000.f;

    bool m_bYFlip = true;

    float m_speed = 1.f;
    float m_sensitivity = 0.8f;

  public:
    [[nodiscard]] glm::mat4 getViewMatrix() const;
    virtual [[nodiscard]] glm::mat4 getProjectionMatrix() const = 0;

    [[nodiscard]] const Transform &getTransform() const
    {
        return m_transform;
    }
    [[nodiscard]] const float &getSpeed() const
    {
        return m_speed;
    }
    [[nodiscard]] inline const float &getSensitivity() const
    {
        return m_sensitivity;
    }

  public:
    void setYFlip(const bool bFlip)
    {
        m_bYFlip = bFlip;
    }
    void setTransform(const Transform &transform)
    {
        m_transform = transform;
    }
};

class PerspectiveCamera : public CameraABC
{
  private:
    float m_yFov = 45.f;
    float m_aspectRatio = 16.f / 9.f;

  public:
    [[nodiscard]] glm::mat4 getProjectionMatrix() const override;
};

class OrthographicCamera : public CameraABC
{
  private:
    float m_left = -1.f;
    float m_right = 1.f;
    float m_bottom = -1.f;
    float m_top = 1.f;

  public:
    [[nodiscard]] glm::mat4 getProjectionMatrix() const override;
};