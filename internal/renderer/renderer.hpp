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

    uint32_t acquireNextSwapChainImage();
    void presentBackBuffer(uint32_t imageIndex);

  public:
    void renderFrame(VkRect2D renderArea, const CameraABC &mainCamera,
                     const std::vector<std::shared_ptr<Light>> &lights);
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