#pragma once

#include "renderer/scene.hpp"

class Device;

class SampleScene2D : public SceneABC
{
  public:
    SampleScene2D(std::weak_ptr<Device> device, uint32_t framesInFlightCount);
};