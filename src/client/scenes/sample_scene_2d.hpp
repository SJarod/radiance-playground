#pragma once

#include "renderer/scene.hpp"

class Device;

class SampleScene2D final : public SceneABC
{
  private:
    std::shared_ptr<Model> m_screen;

  public:
    void load(std::weak_ptr<Context> cx, std::weak_ptr<Device> device, WindowGLFW *window, RenderGraph *renderGraph,
              uint32_t frameInFlightCount, uint32_t maxProbeCount) override;
};