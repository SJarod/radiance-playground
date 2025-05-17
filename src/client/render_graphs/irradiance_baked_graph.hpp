#pragma once

#include "renderer/render_graph.hpp"

class RenderPhase;
class Texture;

class BakedGraph final : public RenderGraph
{
  private:
    void load(std::weak_ptr<Device> device, WindowGLFW *window, uint32_t frameInFlightCount,
              uint32_t maxProbeCount) override;

  public:
    RenderPhase *m_opaqueCapturePhase;
    RenderPhase *m_skyboxCapturePhase;

    RenderPhase *m_irradianceConvolutionPhase;
    RayTracePhase *m_opaquePhase;
    RenderPhase *m_skyboxPhase;

    /**
     * @brief final image with direct lighting
     * post process can be applied in this phase using the right shader
     *
     */
    RenderPhase *m_finalImageDirect;

    RenderPhase *m_imguiPhase;
    RenderPhase *m_probesDebugPhase;

    std::vector<std::shared_ptr<Texture>> m_capturedEnvMaps;
    std::vector<std::shared_ptr<Texture>> m_irradianceMaps;

  public:
};