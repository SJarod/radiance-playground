#pragma once

#include <memory>

#include "graphics/render_pass.hpp"

class Device;
class RenderPass;
class SwapChain;
class Pipeline;
class Mesh;
class Light;
class ProbeGrid;
class Texture;
class Buffer;
class CameraABC;
class RenderStateABC;

// TODO : structure of array instead of array of structure
struct BackBufferT
{
    VkCommandBuffer commandBuffer;

    // TODO : make acquire semaphore optional (first phase may not need one)
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

    struct impl;
    std::unique_ptr<impl> pImpl;

    RenderPhase() = default;

  private:
    [[nodiscard]] const BackBufferT &getCurrentBackBuffer(uint32_t pooledFramebufferIndex) const;

  public:
    ~RenderPhase();

    RenderPhase(const RenderPhase &) = delete;
    RenderPhase &operator=(const RenderPhase &) = delete;
    RenderPhase(RenderPhase &&) = delete;
    RenderPhase &operator=(RenderPhase &&) = delete;

    void registerRenderStateToAllPool(std::shared_ptr<RenderStateABC> renderState);
    void registerRenderStateToSpecificPool(std::shared_ptr<RenderStateABC> renderState,
                                           uint32_t pooledFramebufferIndex);

    void recordBackBuffer(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex, VkRect2D renderArea, const CameraABC &camera,
                          const std::vector<std::shared_ptr<Light>> &lights, const std::shared_ptr<ProbeGrid> &probeGrid) const;
    void submitBackBuffer(const VkSemaphore *acquireSemaphoreOverride, uint32_t pooledFramebufferIndex) const;

    void swapBackBuffers(uint32_t pooledFramebufferIndex);

    void updateSwapchainOnRenderPass(const SwapChain *newSwapchain);

  public:
    [[nodiscard]] const int getSingleFrameRenderCount() const;

    [[nodiscard]] const VkSemaphore &getCurrentAcquireSemaphore(uint32_t pooledFramebufferIndex) const;
    [[nodiscard]] const VkSemaphore &getCurrentRenderSemaphore(uint32_t pooledFramebufferIndex) const;
    [[nodiscard]] const VkFence &getCurrentFence(uint32_t pooledFramebufferIndex) const;
    [[nodiscard]] const RenderPass *getRenderPass() const;
};

class RenderPhaseBuilder
{
  private:
    std::unique_ptr<RenderPhase> m_product;

    std::weak_ptr<Device> m_device;

    /**
     * @brief number of frames in flight defined in the renderer
     *
     */
    uint32_t m_bufferingType = 2;

    void restart();

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
    void setBufferingType(uint32_t type)
    {
        m_bufferingType = type;
    }
    void setRenderPass(std::unique_ptr<RenderPass> renderPass);
    void setSingleFrameRenderCount(uint32_t renderCount);
    void setCaptureEnable(bool enable);

    std::unique_ptr<RenderPhase> build();
};