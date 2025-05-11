#pragma once

#include "renderer/render_graph.hpp"

class RenderPhase;
class ComputePhase;

class RC3DGraph final : public RenderGraph
{
  private:
    void load(std::weak_ptr<Device> device, WindowGLFW *window, uint32_t frameInFlightCount,
              uint32_t maxProbeCount) override;

  public:
    RenderPhase *m_opaquePhase;
    RenderPhase *m_skyboxPhase;

    /**
     * @brief final image with direct lighting
     * post process can be applied in this phase using the right shader
     *
     */
    RenderPhase *m_finalImageDirect;
    ComputePhase *m_computePhase;
    RenderPhase *m_finalImageDirectIndirect;

    RenderPhase *m_imguiPhase;
    RenderPhase *m_probesDebugPhase;

  public:
};