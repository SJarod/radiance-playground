#pragma once

#undef near
#undef far

#include <glm/glm.hpp>

#include "transform.hpp"

class CameraABC
{
  protected:
    Transform m_transform;

    float m_near = 0.1f;
    float m_far = 1000.f;

    bool m_bYFlip = true;

  public:
    [[nodiscard]] glm::mat4 getViewMatrix() const;
    [[nodiscard]] virtual glm::mat4 getProjectionMatrix() const = 0;

    [[nodiscard]] inline const Transform &getTransform() const
    {
        return m_transform;
    }

  public:
    inline void setYFlip(const bool bFlip)
    {
        m_bYFlip = bFlip;
    }
    inline void setNear(const float near)
    {
        m_near = near;
    }
    inline void setFar(const float far)
    {
        m_far = far;
    }
    inline void setTransform(const Transform &transform)
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

  public:
    inline void setFovY(float fov)
    {
        m_yFov = fov;
    }
    inline void setAspectRatio(float ar)
    {
        m_aspectRatio = ar;
    }
};

class OrthographicCamera : public CameraABC
{
  private:
    float m_left = -16.f / 9.f;
    float m_right = 16.f / 9.f;
    float m_bottom = -1.f;
    float m_top = 1.f;

  public:
    [[nodiscard]] glm::mat4 getProjectionMatrix() const override;
};
