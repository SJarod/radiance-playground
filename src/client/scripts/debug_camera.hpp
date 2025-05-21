#pragma once

#include <utility>

#include "engine/scriptable.hpp"

class CameraABC;

class DebugCamera : public ScriptableABC
{
  public:
    struct UserDataT
    {
        CameraABC &camera;
    };

  private:
    CameraABC *m_mainCamera;

  public:
    void init(void *userData) override;
    void begin() override;
    void update(float deltaTime) override;
};