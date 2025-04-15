#pragma once

#include <memory>

class RenderGraph;
class SwapChain;
class Device;

class RendererBuilder;

/**
 * @brief manages the swapchain
 *
 */
class Renderer
{
    friend RendererBuilder;

  private:
    std::weak_ptr<Device> m_device;
    const SwapChain *m_swapchain;

    std::unique_ptr<RenderGraph> m_renderGraph;

    /**
     * @brief buffering type
     *
     */
    int m_framesInFlight = 2;

    Renderer() = default;

    VkResult acquireNextSwapChainImage(uint32_t &nextImageIndex);
    VkResult presentBackBuffer(uint32_t imageIndex);

  public:
    VkResult renderFrame(VkRect2D renderArea, const CameraABC &mainCamera,
                         const std::vector<std::shared_ptr<Light>> &lights, const std::vector<std::unique_ptr<Probe>> &probes);

  public:
    [[nodiscard]] int getFrameInFlightCount() const
    {
        return m_framesInFlight;
    }

  public:
    void setSwapChain(const SwapChain *swapchain)
    {
        m_swapchain = swapchain;
        m_renderGraph->updateSwapchainOnRenderPhases(swapchain);
    }
};

class RendererBuilder
{
  private:
    std::unique_ptr<Renderer> m_product;

    void restart()
    {
        m_product = std::unique_ptr<Renderer>(new Renderer);
    }

  public:
    RendererBuilder()
    {
        restart();
    }

    void setDevice(std::weak_ptr<Device> device)
    {
        m_product->m_device = device;
    }

    void setSwapChain(const SwapChain *swapchain)
    {
        m_product->m_swapchain = swapchain;
    }

    void setRenderGraph(std::unique_ptr<RenderGraph> renderGraph)
    {
        m_product->m_renderGraph = std::move(renderGraph);
    }

    std::unique_ptr<Renderer> build();
};