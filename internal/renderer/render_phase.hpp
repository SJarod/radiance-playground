#pragma once

#include <memory>

#include "graphics/render_pass.hpp"

class Device;
class RenderPass;
class SwapChain;
class Pipeline;
class Mesh;
class Light;
class Texture;
class Buffer;
class CameraABC;
class RenderStateABC;

// TODO : structure of array instead of array of structure
struct BackBufferT
{
    VkCommandBuffer commandBuffer;

    // TODO : make acquire semaphore optional
    VkSemaphore acquireSemaphore;
    VkSemaphore renderSemaphore;
    VkFence inFlightFence;
};

class RenderPhaseBuilder;

/**
 * @brief manages the command buffers and the render states
 *
 */
class RenderPhase
{
    friend RenderPhaseBuilder;

  private:
    std::weak_ptr<Device> m_device;

    std::unique_ptr<RenderPass> m_renderPass;

    std::vector<std::shared_ptr<RenderStateABC>> m_renderStates;

    int m_backBufferIndex = 0;
    std::vector<BackBufferT> m_backBuffers;

    RenderPhase() = default;

  private:
    [[nodiscard]] const BackBufferT &getCurrentBackBuffer() const
    {
        return m_backBuffers[m_backBufferIndex];
    }

  public:
    ~RenderPhase();

    RenderPhase(const RenderPhase &) = delete;
    RenderPhase &operator=(const RenderPhase &) = delete;
    RenderPhase(RenderPhase &&) = delete;
    RenderPhase &operator=(RenderPhase &&) = delete;

    void registerRenderState(std::shared_ptr<RenderStateABC> renderState);

    void recordBackBuffer(uint32_t imageIndex, VkRect2D renderArea, const CameraABC &camera,
                          const std::vector<std::shared_ptr<Light>> &lights);
    void submitBackBuffer(const VkSemaphore *acquireSemaphoreOverride);

    void swapBackBuffers();

  public:
    [[nodiscard]] const VkSemaphore &getCurrentAcquireSemaphore() const
    {
        return getCurrentBackBuffer().acquireSemaphore;
    }
    [[nodiscard]] const VkSemaphore &getCurrentRenderSemaphore() const
    {
        return getCurrentBackBuffer().renderSemaphore;
    }
    [[nodiscard]] const VkFence &getCurrentFence() const
    {
        return getCurrentBackBuffer().inFlightFence;
    }
    [[nodiscard]] const RenderPass *getRenderPass() const
    {
        return m_renderPass.get();
    }
};

class RenderPhaseBuilder
{
  private:
    std::unique_ptr<RenderPhase> m_product;

    std::weak_ptr<Device> m_device;

    uint32_t m_bufferingType = 2;

    void restart()
    {
        m_product = std::unique_ptr<RenderPhase>(new RenderPhase);
    }

  public:
    RenderPhaseBuilder()
    {
        restart();
    }

    void setDevice(std::weak_ptr<Device> device)
    {
        m_device = device;
        m_product->m_device = device;
    }
    void setRenderPass(std::unique_ptr<RenderPass> renderPass)
    {
        m_product->m_renderPass = std::move(renderPass);
    }
    void setBufferingType(uint32_t type)
    {
        m_bufferingType = type;
    }

    std::unique_ptr<RenderPhase> build();
};