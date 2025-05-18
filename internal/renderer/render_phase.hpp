#pragma once

#include <cassert>
#include <memory>
#include <string>

#include <vulkan/vulkan.hpp>

#include "graphics/buffer.hpp"
#include "graphics/render_pass.hpp"

class Device;
class RenderPass;
class SwapChain;
class Pipeline;
class Mesh;
class Light;
class ProbeGrid;
class Texture;
class CameraABC;
class RenderStateABC;
class ComputeState;

enum class RenderTypeE
{
    RASTER = 0,
    RAYTRACE = 1,
    COUNT = 2,
};

// TODO : structure of array instead of array of structure
struct BackBufferT
{
    VkCommandBuffer commandBuffer;

    // TODO : make acquire semaphore optional (first phase may not need one)
    VkSemaphore acquireSemaphore;
    VkSemaphore renderSemaphore;
    VkFence inFlightFence;
};

template <RenderTypeE TType> class RenderPhaseBuilder;

class BasePhaseABC
{
  private:
    [[nodiscard]] virtual const BackBufferT &getCurrentBackBuffer(uint32_t pooledFramebufferIndex) const = 0;

  protected:
    std::weak_ptr<Device> m_device;

    BasePhaseABC() = default;

  public:
    virtual ~BasePhaseABC() = default;

    BasePhaseABC(const BasePhaseABC &) = delete;
    BasePhaseABC &operator=(const BasePhaseABC &) = delete;
    BasePhaseABC(BasePhaseABC &&) = delete;
    BasePhaseABC &operator=(BasePhaseABC &&) = delete;

    virtual void recordBackBuffer() const = 0;
    virtual void submitBackBuffer(const VkSemaphore *acquireSemaphoreOverride) const = 0;

    /**
     * @brief wait for this phase to complete
     *
     */
    virtual void wait() const = 0;

    virtual void swapBackBuffers() = 0;

  public:
    [[nodiscard]] virtual const VkSemaphore &getCurrentAcquireSemaphore(uint32_t pooledFramebufferIndex) const = 0;
    [[nodiscard]] virtual const VkSemaphore &getCurrentRenderSemaphore(uint32_t pooledFramebufferIndex) const = 0;
    [[nodiscard]] virtual const VkFence &getCurrentFence(uint32_t pooledFramebufferIndex) const = 0;
};

/**
 * @brief manages the command buffers and the render states and render passes
 *
 */
class RenderPhase : public BasePhaseABC
{
    friend RenderPhaseBuilder<RenderTypeE::RASTER>;

  protected:
    /**
     * @brief the parent phase field is not used (no need to be set), it was used to get the reference from the child
     * phase when updating the descriptor sets but users can directly access whatever phase they want when using a
     * capturing lambda
     *
     */
    [[deprecated]]
    const RenderPhase *m_parentPhase = nullptr;

    /**
     * @brief the render pass is optional because a rasterization phase can use the dynamic rendering extension or even
     * use a ray tracing pipline (no render pass) or ray queries (ray tracing in other shader stages) in the
     * rasterization pipeline
     *
     */
    std::optional<std::unique_ptr<RenderPass>> m_renderPass;

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

    [[nodiscard]] const BackBufferT &getCurrentBackBuffer(uint32_t pooledFramebufferIndex) const override
    {
        return m_pooledBackBuffers[pooledFramebufferIndex][m_backBufferIndex];
    }

    RenderPhase() = default;

  public:
    virtual ~RenderPhase();

    RenderPhase(const RenderPhase &) = delete;
    RenderPhase &operator=(const RenderPhase &) = delete;
    RenderPhase(RenderPhase &&) = delete;
    RenderPhase &operator=(RenderPhase &&) = delete;

    /**
     * @brief this RenderPhase class does not implement this function nor the functions below yet, it uses a system with
     * pools for rendering in a cubemap, thus the record back buffer function takes a rather large amount of arguments
     * (those arguments may not be necessary in the case where a single rendering is performed such as with a ray
     * tracing pipeline and no render pass)
     *
     */
    void recordBackBuffer() const override
    {
        assert(false);
    }
    void submitBackBuffer(const VkSemaphore *acquireSemaphoreOverride) const override
    {
        assert(false);
    }

    /**
     * @brief wait for this phase to complete
     *
     */
    void wait() const override
    {
        assert(false);
    }

    void swapBackBuffers() override
    {
        assert(false);
    }

    void registerRenderStateToAllPool(std::shared_ptr<RenderStateABC> renderState);
    void registerRenderStateToSpecificPool(std::shared_ptr<RenderStateABC> renderState,
                                           uint32_t pooledFramebufferIndex);

    /**
     * @brief TODO : move the arguments in a struct to facilitate the call of this function and the modularity with
     * other phases
     * it is not a const function as it will save the last rendered image in this object in order to access it from
     * other phases
     *
     * @param imageIndex
     * @param singleFrameRenderIndex
     * @param pooledFramebufferIndex
     * @param renderArea
     * @param camera
     * @param lights
     * @param probeGrid
     */
    virtual void recordBackBuffer(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex,
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
        assert(m_renderPass.has_value());
        return m_renderPass.value().get();
    }
    [[nodiscard]] std::pair<std::optional<VkImage>, VkImageView> getMostRecentRenderedImage() const
    {
        assert(m_lastFramebufferImageResource.has_value());
        return std::pair<std::optional<VkImage>, VkImageView>(m_lastFramebufferImageResource,
                                                              m_lastFramebufferImageView.value());
    }
};
typedef RenderPhase RasterPhase;
typedef RenderPhase CubePhase;

#ifdef USE_NV_PRO_CORE
#include "nvvk/context_vk.hpp"
#include "nvvk/raytraceKHR_vk.hpp"
#endif

class RayTracePhase final : public RenderPhase
{
    friend RenderPhaseBuilder<RenderTypeE::RASTER>;
    friend RenderPhaseBuilder<RenderTypeE::RAYTRACE>;

  private:
#ifdef USE_NV_PRO_CORE
    nvvk::Context vkctx{};

    nvvk::ResourceAllocatorDma m_alloc;
    nvvk::RaytracingBuilderKHR m_rtBuilder;
#else
    std::vector<VkAccelerationStructureKHR> m_blas;
    std::vector<std::unique_ptr<Buffer>> m_blasBuffers;

    std::vector<VkAccelerationStructureKHR> m_tlas;
    std::vector<std::unique_ptr<Buffer>> m_tlasBuffers;
#endif

    /**
     * @brief combination of Vulkan structures representing a mesh as a ray traceable geometry
     *
     */
    using AsGeom = std::pair<VkAccelerationStructureGeometryKHR, VkAccelerationStructureBuildRangeInfoKHR>;

    /**
     * @brief Get the As Geometry object
     * function implementation is from
     * https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/vkrt_tutorial.md.html#accelerationstructure/bottom-levelaccelerationstructure
     *
     * @return AsGeom
     */
    [[nodiscard]] AsGeom getAsGeometry(std::shared_ptr<Mesh> mesh) const;

    /**
     * @brief update the descriptor sets of all the states a once more to write the top level acceleration structure
     *
     */
    void updateDescriptorSets();

    RayTracePhase() = default;

  public:
    ~RayTracePhase();

    RayTracePhase(const RayTracePhase &) = delete;
    RayTracePhase &operator=(const RayTracePhase &) = delete;
    RayTracePhase(RayTracePhase &&) = delete;
    RayTracePhase &operator=(RayTracePhase &&) = delete;

    void generateBottomLevelAS();
    void generateTopLevelAS();

    void recordBackBuffer(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex,
                          VkRect2D renderArea, const CameraABC &camera,
                          const std::vector<std::shared_ptr<Light>> &lights,
                          const std::shared_ptr<ProbeGrid> &probeGrid) override;

  public:
    [[nodiscard]] inline const std::vector<VkAccelerationStructureKHR> getTLAS() const
    {
#ifdef USE_NV_PRO_CORE
        return std::vector<VkAccelerationStructureKHR>{m_rtBuilder.getAccelerationStructure()};
#else
        return m_tlas;
#endif
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

template <RenderTypeE TType> class RenderPhaseBuilder final : public PhaseBuilderABC
{
};

template <> class RenderPhaseBuilder<RenderTypeE::RASTER> : public PhaseBuilderABC
{
  private:
    void restart()
    {
        m_product = std::unique_ptr<RenderPhase>(new RenderPhase);
    }

  protected:
    std::unique_ptr<RenderPhase> m_product;

    std::weak_ptr<Device> m_device;

    uint32_t m_bufferingType = 2;

  public:
    RenderPhaseBuilder()
    {
        restart();
    }

    virtual ~RenderPhaseBuilder() = default;

    RenderPhaseBuilder(const RenderPhaseBuilder &) = delete;
    RenderPhaseBuilder &operator=(const RenderPhaseBuilder &) = delete;
    RenderPhaseBuilder(RenderPhaseBuilder &&) = delete;
    RenderPhaseBuilder &operator=(RenderPhaseBuilder &&) = delete;

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

    /**
     * @brief this build function is used for the rasterizer phase but used for the raytracing phase as well
     * the sole difference between the rasterizer phase and the ray tracing phase is that the latter has an optional
     * render pass (if the phase has a renderpass, it may by a rasterizer using ray queries, if not, the phase uses a
     * ray tracing pipeline or a dynamic rendering system)
     *
     * @return std::unique_ptr<RenderPhase>
     */
    virtual std::unique_ptr<RenderPhase> build();
};
template <> class RenderPhaseBuilder<RenderTypeE::RAYTRACE> final : public RenderPhaseBuilder<RenderTypeE::RASTER>
{
  private:
    void restart()
    {
        m_product = std::unique_ptr<RenderPhase>(new RayTracePhase);
    }

  public:
    RenderPhaseBuilder()
    {
        restart();
    }

#ifdef USE_NV_PRO_CORE
    std::unique_ptr<RenderPhase> build() override;
#endif
};

class ComputePhaseBuilder;

/**
 * @brief manages the command buffers for the compute shader
 *
 */
class ComputePhase final : public BasePhaseABC
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

    void recordBackBuffer() const override;
    void submitBackBuffer(const VkSemaphore *acquireSemaphoreOverride) const override;

    /**
     * @brief wait for this compute phase to complete
     *
     */
    void wait() const override;

    void swapBackBuffers() override;

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

class ComputePhaseBuilder final : public PhaseBuilderABC
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
