#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <vulkan/vulkan.h>

class RenderPhase;
class CameraABC;
class Light;
class ProbeGrid;
class SwapChain;
class BasePhaseABC;
class Device;
class WindowGLFW;

class RenderGraphLoader;

/**
 * @brief manages the relation ship between each phase (submit semaphores)
 *
 */
class RenderGraph
{
    friend RenderGraphLoader;

  protected:
    bool m_shouldRenderOneTimePhases = true;
    /**
     * @brief phases that are called once at the begining of the processing
     *
     */
    std::vector<std::unique_ptr<BasePhaseABC>> m_oneTimeRenderPhases;

    // TODO : rename m_phases
    /**
     * @brief actual phases (RenderPhase, ComputePhase)
     *
     */
    std::vector<std::unique_ptr<BasePhaseABC>> m_renderPhases;

    /**
     * @brief a map of raw pointers, pointing towards the render phase resources
     * not yet implemented
     *
     */
    [[deprecated]] std::unordered_map<std::string, BasePhaseABC *> m_phasePtrs;

    /**
     * @brief RenderGraphs can be created using a unique_ptr or whatever data structure
     * It can also be loaded by a function from a derived class
     *
     */
    virtual void load(std::weak_ptr<Device> device, WindowGLFW *window, uint32_t frameInFlightCount,
                      uint32_t maxProbeCount)
    {
    }

  public:
    virtual ~RenderGraph() = default;

    RenderGraph() = default;
    RenderGraph(const RenderGraph &) = delete;
    RenderGraph &operator=(const RenderGraph &) = delete;
    RenderGraph(RenderGraph &&) = delete;
    RenderGraph &operator=(RenderGraph &&) = delete;

    [[deprecated]] void addOneTimeRenderPhase(std::unique_ptr<RenderPhase> renderPhase);
    [[deprecated]] void addRenderPhase(std::unique_ptr<RenderPhase> renderPhase);
    void addPhase(std::unique_ptr<BasePhaseABC> phase);

    void processRenderPhaseChain(std::vector<std::unique_ptr<BasePhaseABC>> &toProcess, uint32_t imageIndex,
                                 VkRect2D renderArea, const CameraABC &mainCamera,
                                 const std::vector<std::shared_ptr<Light>> &lights,
                                 const std::shared_ptr<ProbeGrid> &probeGrid, const VkSemaphore *inWaitSemaphore,
                                 const VkSemaphore **outAcquireSemaphore);

    void processRendering(uint32_t imageIndex, VkRect2D renderArea, const CameraABC &mainCamera,
                          const std::vector<std::shared_ptr<Light>> &lights,
                          const std::shared_ptr<ProbeGrid> &probeGrid);

    void swapAllRenderPhasesBackBuffers();

    void updateSwapchainOnRenderPhases(const SwapChain *swapchain);

  public:
    [[nodiscard]] VkSemaphore getFirstPhaseCurrentAcquireSemaphore() const;
    [[nodiscard]] VkSemaphore getLastPhaseCurrentRenderSemaphore() const;
    [[nodiscard]] std::vector<VkFence> getAllCurrentFences() const;
};

class RenderGraphLoader
{
  public:
    template <typename TGraph>
    static std::unique_ptr<RenderGraph> load(std::weak_ptr<Device> device, WindowGLFW *window,
                                             uint32_t frameInFlightCount, uint32_t maxProbeCount)
    {
        static_assert(std::is_base_of_v<RenderGraph, TGraph> == true);
        std::unique_ptr<RenderGraph> out = std::make_unique<TGraph>();
        out->load(device, window, frameInFlightCount, maxProbeCount);
        return std::move(out);
    }
};