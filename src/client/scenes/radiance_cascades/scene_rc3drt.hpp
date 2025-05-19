#pragma once

#include <memory>

#include "renderer/scene.hpp"

class Device;
class WindowGLFW;
class RenderGraph;
class Context;
class ProbeGrid;
class Model;
class Buffer;

class SceneRC3DRT final : public SceneABC
{
  public:
    std::shared_ptr<ProbeGrid> m_grid0;
    std::shared_ptr<ProbeGrid> m_grid1;
    std::shared_ptr<ProbeGrid> m_grid2;

    std::shared_ptr<Model> m_screen;

    std::unique_ptr<Buffer> m_pointLightSSBO;
    std::vector<void*> m_pointLightSSBOMapped;
    std::unique_ptr<Buffer> m_dirLightSSBO;
    std::vector<void*> m_dirLightSSBOMapped;

  public:
    void load(std::weak_ptr<Context> cx, std::weak_ptr<Device> device, WindowGLFW *window, RenderGraph *renderGraph,
              uint32_t frameInFlightCount, uint32_t maxProbeCount) override;
};