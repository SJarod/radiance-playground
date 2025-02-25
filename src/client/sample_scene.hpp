#pragma once

#include "renderer/scene.hpp"

class Device;
class WindowGLFW;

class SampleScene : public SceneABC
{
  public:
    SampleScene(std::weak_ptr<Device> device, WindowGLFW *window);
};