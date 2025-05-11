#pragma once

#include <cassert>
#include <memory>
#include <string>

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
class ComputeState;

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

class BasePhaseABC
{
  protected:
    std::weak_ptr<Device> m_device;

    BasePhaseABC() = default;

  private:
    virtual [[nodiscard]] const BackBufferT &getCurrentBackBuffer(uint32_t pooledFramebufferIndex) const = 0;

  public:
    virtual ~BasePhaseABC() = default;

    BasePhaseABC(const BasePhaseABC &) = delete;
    BasePhaseABC &operator=(const BasePhaseABC &) = delete;
    BasePhaseABC(BasePhaseABC &&) = delete;
    BasePhaseABC &operator=(BasePhaseABC &&) = delete;

    virtual [[nodiscard]] const VkSemaphore &getCurrentAcquireSemaphore(uint32_t pooledFramebufferIndex) const = 0;
    virtual [[nodiscard]] const VkSemaphore &getCurrentRenderSemaphore(uint32_t pooledFramebufferIndex) const = 0;
    virtual [[nodiscard]] const VkFence &getCurrentFence(uint32_t pooledFramebufferIndex) const = 0;
};

/**
 * @brief manages the command buffers and the render states and render passes
 *
 */
class RenderPhase : public BasePhaseABC
{
    friend RenderPhaseBuilder;

  private:
    [[deprecated]]
    const RenderPhase *m_parentPhase = nullptr;

    std::unique_ptr<RenderPass> m_renderPass;

    std::vector<std::vector<std::shared_ptr<RenderStateABC>>> m_pooledRenderStates;

    uint32_t m_singleFrameRenderCount = 1u;

    int m_backBufferIndex = 0;
    std::vector<std::vector<BackBufferT>> m_pooledBackBuffers;

    bool m_isCapturePhase = false;

    /**
     * @brief the most recent frame buffer in which a render was made
     *
     */
    std::optional<VkFramebuffer> m_lastFramebuffer = nullptr;
    /**
     * @brief same as last frame buffer but with an image resource
     *
     */
    std::optional<VkImage> m_lastFramebufferImageResource = nullptr;
    /**
     * @brief same as last frame buffer but with an image view resource
     *
     */
    std::optional<VkImageView> m_lastFramebufferImageView = nullptr;

    RenderPhase() = default;

  private:
    [[nodiscard]] const BackBufferT &getCurrentBackBuffer(uint32_t pooledFramebufferIndex) const override
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
    void registerRenderStateToSpecificPool(std::shared_ptr<RenderStateABC> renderState,
                                           uint32_t pooledFramebufferIndex);

    void recordBackBuffer(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex,
                          VkRect2D renderArea, const CameraABC &camera,
                          const std::vector<std::shared_ptr<Light>> &lights,
                          const std::shared_ptr<ProbeGrid> &probeGrid);
    void submitBackBuffer(const VkSemaphore *acquireSemaphoreOverride, uint32_t pooledFramebufferIndex) const;

    void swapBackBuffers(uint32_t pooledFramebufferIndex);

    void updateSwapchainOnRenderPass(const SwapChain *newSwapchain);

  public:
    [[nodiscard]] const int getSingleFrameRenderCount() const
    {
        return m_singleFrameRenderCount;
    }

    [[nodiscard]] const VkSemaphore &getCurrentAcquireSemaphore(uint32_t pooledFramebufferIndex) const override
    {
        return getCurrentBackBuffer(pooledFramebufferIndex).acquireSemaphore;
    }
    [[nodiscard]] const VkSemaphore &getCurrentRenderSemaphore(uint32_t pooledFramebufferIndex) const override
    {
        return getCurrentBackBuffer(pooledFramebufferIndex).renderSemaphore;
    }
    [[nodiscard]] const VkFence &getCurrentFence(uint32_t pooledFramebufferIndex) const override
    {
        return getCurrentBackBuffer(pooledFramebufferIndex).inFlightFence;
    }
    [[nodiscard]] const RenderPass *getRenderPass() const
    {
        return m_renderPass.get();
    }
    [[nodiscard]] std::pair<std::optional<VkImage>, VkImageView> getMostRecentRenderedImage() const
    {
        assert(m_lastFramebufferImageResource.has_value());
        return std::pair<std::optional<VkImage>, VkImageView>(m_lastFramebufferImageResource,
                                                              m_lastFramebufferImageView.value());
    }
};

class PhaseBuilderABC
{
  protected:
    std::string m_phaseName = "Unnamed";

  public:
    virtual ~PhaseBuilderABC() = default;

    PhaseBuilderABC() = default;
    PhaseBuilderABC(const PhaseBuilderABC &) = delete;
    PhaseBuilderABC &operator=(const PhaseBuilderABC &) = delete;
    PhaseBuilderABC(PhaseBuilderABC &&) = delete;
    PhaseBuilderABC &operator=(PhaseBuilderABC &&) = delete;

    inline void setPhaseName(std::string name)
    {
        m_phaseName = name;
    }
};

class RenderPhaseBuilder : public PhaseBuilderABC
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
    [[deprecated]]
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
        m_product->m_singleFrameRenderCount = renderCount;
    }
    void setCaptureEnable(bool enable)
    {
        m_product->m_isCapturePhase = enable;
    }

    std::unique_ptr<RenderPhase> build();
};

class ComputePhaseBuilder;

/**
 * @brief manages the command buffers for the compute shader
 *
 */
class ComputePhase : public BasePhaseABC
{
    friend ComputePhaseBuilder;

  private:
    int m_backBufferIndex = 0;
    std::vector<BackBufferT> m_backBuffers;

    std::vector<std::shared_ptr<ComputeState>> m_computeStates;

    ComputePhase() = default;

    [[nodiscard]] const BackBufferT &getCurrentBackBuffer(uint32_t pooledFramebufferIndex = -1) const override
    {
        return m_backBuffers[m_backBufferIndex];
    }

  public:
    ~ComputePhase();

    ComputePhase(const ComputePhase &) = delete;
    ComputePhase &operator=(const ComputePhase &) = delete;
    ComputePhase(ComputePhase &&) = delete;
    ComputePhase &operator=(ComputePhase &&) = delete;

    void registerComputeState(std::shared_ptr<ComputeState> state);

    void recordBackBuffer() const;
    void submitBackBuffer(const VkSemaphore *acquireSemaphoreOverride) const;

    void swapBackBuffers();

  public:
    [[nodiscard]] const VkSemaphore &getCurrentAcquireSemaphore(uint32_t pooledFramebufferIndex = -1) const override
    {
        return getCurrentBackBuffer().acquireSemaphore;
    }
    [[nodiscard]] const VkSemaphore &getCurrentRenderSemaphore(uint32_t pooledFramebufferIndex = -1) const override
    {
        return getCurrentBackBuffer().renderSemaphore;
    }
    [[nodiscard]] const VkFence &getCurrentFence(uint32_t pooledFramebufferIndex = -1) const override
    {
        return getCurrentBackBuffer().inFlightFence;
    }
};

class ComputePhaseBuilder : public PhaseBuilderABC
{
  private:
    std::unique_ptr<ComputePhase> m_product;

    std::weak_ptr<Device> m_device;

    uint32_t m_bufferingType = 2;

    void restart()
    {
        m_product = std::unique_ptr<ComputePhase>(new ComputePhase);
    }

  public:
    ComputePhaseBuilder()
    {
        restart();
    }

    void setDevice(std::weak_ptr<Device> device)
    {
        m_device = device;
        m_product->m_device = device;
    }
    void setBufferingType(uint32_t type)
    {
        m_bufferingType = type;
    }

    std::unique_ptr<ComputePhase> build();
};
