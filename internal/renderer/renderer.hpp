#pragma once

#include <memory>

#include "graphics/render_pass.hpp"

class Device;
class SwapChain;
class Pipeline;
class Mesh;
class Texture;
class Buffer;
class Camera;
class RenderStateABC;

struct BackBufferT
{
    VkCommandBuffer commandBuffer;

    VkSemaphore acquireSemaphore;
    VkSemaphore renderSemaphore;
    VkFence inFlightFence;
};

class RendererBuilder;

class Renderer
{
    friend RendererBuilder;

  private:
    std::weak_ptr<Device> m_device;
    const SwapChain *m_swapchain;

    int m_bufferingType = 2;

    std::unique_ptr<RenderPass> m_renderPass;

    std::vector<std::shared_ptr<RenderStateABC>> m_renderStates;

    int m_backBufferIndex = 0;
    std::vector<BackBufferT> m_backBuffers;

    Renderer() = default;

  public:
    ~Renderer();

    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    Renderer(Renderer &&) = delete;
    Renderer &operator=(Renderer &&) = delete;

    void registerRenderState(std::shared_ptr<RenderStateABC> renderState);

    uint32_t acquireBackBuffer();

    void recordRenderers(uint32_t imageIndex, const Camera &camera, const PointLight &pointLight);

    void submitBackBuffer();
    void presentBackBuffer(uint32_t imageIndex);

    void swapBuffers();

  public:
    [[nodiscard]] const RenderPass *getRenderPass() const
    {
        return m_renderPass.get();
    }
};

class RendererBuilder
{
  private:
    std::unique_ptr<Renderer> m_product;

    std::weak_ptr<Device> m_device;
    const SwapChain *m_swapchain;

    void restart()
    {
        m_product = std::unique_ptr<Renderer>(new Renderer);
        m_product->m_bufferingType = 2U;
    }

  public:
    RendererBuilder()
    {
        restart();
    }

    void setDevice(std::weak_ptr<Device> device)
    {
        m_device = device;
        m_product->m_device = device;
    }
    void setSwapChain(const SwapChain *swapchain)
    {
        m_swapchain = swapchain;
        m_product->m_swapchain = swapchain;
    }
    void setBufferingType(uint32_t type)
    {
        m_product->m_bufferingType = type;
    }

    std::unique_ptr<Renderer> build();
};