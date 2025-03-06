#pragma once

#include <utility>

#include "engine/scriptable.hpp"

#include "engine/camera.hpp"
#include "wsi/window.hpp"

class MoveCamera : public ScriptableABC
{
  public:
    struct UserDataT
    {
        WindowGLFW &window;
        CameraABC &camera;
    };

  private:
    std::pair<double, double> m_mousePos;
    WindowGLFW *m_window;
    CameraABC *m_mainCamera;

    bool m_isFocused = true;

    void setFocus(bool newFocus);

  public:
    void init(void *userData) override;
    void begin() override;
    void update(float deltaTime) override;
};