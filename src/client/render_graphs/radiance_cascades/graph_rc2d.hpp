#pragma once

#include "renderer/render_graph.hpp"

class RenderPhase;
class ComputePhase;

class GraphRC2D final : public RenderGraph
{
  private:
    void load(std::weak_ptr<Device> device, WindowGLFW *window, uint32_t frameInFlightCount,
                      uint32_t maxProbeCount) override;

  public:
    RenderPhase *m_opaquePhase;

    /**
     * @brief final image with direct lighting
     * post process can be applied in this phase using the right shader
     *
     */
    RenderPhase *m_finalImageDirect;
    /**
     * @brief compute shader for the radiance gathering
     *
     */
    ComputePhase *m_computePhase;
    /**
     * @brief final image combining direct and indirect lighting
     *
     */
    RenderPhase *m_finalImageDirectIndirect;

    RenderPhase *m_imguiPhase;

  public:
};