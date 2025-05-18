#pragma once

#include <memory>

#include "renderer/scene.hpp"

class Device;
class WindowGLFW;
class RenderGraph;
class Context;
class ProbeGrid;
class Model;

class SceneRC3D final : public SceneABC
{
  public:
    std::shared_ptr<ProbeGrid> m_grid0;
    std::shared_ptr<ProbeGrid> m_grid1;
    std::shared_ptr<ProbeGrid> m_grid2;

    std::shared_ptr<Model> m_screen;

  public:
    void load(std::weak_ptr<Context> cx, std::weak_ptr<Device> device, WindowGLFW *window, RenderGraph *renderGraph,
              uint32_t frameInFlightCount, uint32_t maxProbeCount) override;
};