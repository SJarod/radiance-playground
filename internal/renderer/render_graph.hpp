#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

class RenderPhase;
class CameraABC;
class Light;
class ProbeGrid;
class SwapChain;
class BasePhaseABC;

/**
 * @brief manages the relation ship between each phase (submit semaphores)
 *
 */
class RenderGraph
{
  private:
    // TODO : better way to retrieve each phase to register new render states
    bool m_shouldRenderOneTimePhases = true;
    std::vector<std::unique_ptr<BasePhaseABC>> m_oneTimeRenderPhases;
    // TODO : rename m_phases
    std::vector<std::unique_ptr<BasePhaseABC>> m_renderPhases;

  public:
    [[deprecated]] void addOneTimeRenderPhase(std::unique_ptr<RenderPhase> renderPhase);
    [[deprecated]] void addRenderPhase(std::unique_ptr<RenderPhase> renderPhase);
    void addPhase(std::unique_ptr<BasePhaseABC> phase);

    void processRenderPhaseChain(const std::vector<std::unique_ptr<BasePhaseABC>> &toProcess, uint32_t imageIndex,
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