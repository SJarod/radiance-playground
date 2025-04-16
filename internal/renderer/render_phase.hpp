#pragma once

#include <memory>

#include "graphics/render_pass.hpp"

class Device;
class RenderPass;
class SwapChain;
class Pipeline;
class Mesh;
class Light;
class Probe;
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
    const RenderPhase *m_parentPhase = nullptr;

    std::unique_ptr<RenderPass> m_renderPass;

    std::vector<std::vector<std::shared_ptr<RenderStateABC>>> m_pooledRenderStates;

    uint32_t m_singleFrameRendeerCount = 1u;

    int m_backBufferIndex = 0;
    std::vector<std::vector<BackBufferT>> m_pooledBackBuffers;

    bool m_isCapturePhase = false;

    RenderPhase() = default;

  private:
    [[nodiscard]] const BackBufferT &getCurrentBackBuffer(uint32_t pooledFramebufferIndex) const
    {
        return m_pooledBackBuffers[pooledFramebufferIndex][m_backBufferIndex];
    }

  public:
    ~RenderPhase();

    RenderPhase(const RenderPhase &) = delete;
    RenderPhase &operator=(const RenderPhase &) = delete;
    RenderPhase(RenderPhase &&) = delete;
    RenderPhase &operator=(RenderPhase &&) = delete;

    void registerRenderStateToAllPool(std::shared_ptr<RenderStateABC> renderState);
    void registerRenderStateToSpecificPool(std::shared_ptr<RenderStateABC> renderState, uint32_t pooledFramebufferIndex);

    void recordBackBuffer(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex, VkRect2D renderArea, const CameraABC &camera,
                          const std::vector<std::shared_ptr<Light>> &lights, const std::vector<std::unique_ptr<Probe>> &probes) const;
    void submitBackBuffer(const VkSemaphore *acquireSemaphoreOverride, uint32_t pooledFramebufferIndex) const;

    void swapBackBuffers(uint32_t pooledFramebufferIndex);
    
    void updateSwapchainOnRenderPass(const SwapChain* newSwapchain);

  public:
    [[nodiscard]] const int getSingleFrameRenderCount() const
    {
          return m_singleFrameRendeerCount;
    }

    [[nodiscard]] const VkSemaphore &getCurrentAcquireSemaphore(uint32_t pooledFramebufferIndex) const
    {
        return getCurrentBackBuffer(pooledFramebufferIndex).acquireSemaphore;
    }
    [[nodiscard]] const VkSemaphore &getCurrentRenderSemaphore(uint32_t pooledFramebufferIndex) const
    {
        return getCurrentBackBuffer(pooledFramebufferIndex).renderSemaphore;
    }
    [[nodiscard]] const VkFence &getCurrentFence(uint32_t pooledFramebufferIndex) const
    {
        return getCurrentBackBuffer(pooledFramebufferIndex).inFlightFence;
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
    void setParentPhase(const RenderPhase *parentPhase)
    {
        m_product->m_parentPhase = parentPhase;
    }
    void setRenderPass(std::unique_ptr<RenderPass> renderPass)
    {
        m_product->m_renderPass = std::move(renderPass);
    }
    void setBufferingType(uint32_t type)
    {
        m_bufferingType = type;
    }
    void setSingleFrameRenderCount(uint32_t renderCount)
    {
        m_product->m_singleFrameRendeerCount = renderCount;
    }
    void setCaptureEnable(bool enable)
    {
        m_product->m_isCapturePhase = enable;
    }

    std::unique_ptr<RenderPhase> build();
};