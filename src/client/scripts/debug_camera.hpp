#pragma once

#include <memory>
#include <utility>

#include "engine/scriptable.hpp"

class CameraABC;
class Model;

class DebugCamera : public ScriptableABC
{
  public:
    struct UserDataT
    {
        CameraABC &camera;
        std::shared_ptr<Model> sphere;
    };

  private:
    CameraABC *m_mainCamera;
        std::shared_ptr<Model> m_sphere;


  public:
    void init(void *userData) override;
    void begin() override;
    void update(float deltaTime) override;
};