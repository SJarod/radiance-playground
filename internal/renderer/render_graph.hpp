#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

class RenderPhase;
class CameraABC;
class Light;
class ProbeGrid;
class SwapChain;

/**
 * @brief manages the relation ship between each phase (submit semaphores)
 *
 */
class RenderGraph
{
  private:
  // TODO : better way to retrieve each phase to register new render states
      bool m_shouldRenderOneTimePhases = true;
      std::vector<std::unique_ptr<RenderPhase>> m_oneTimeRenderPhases;
      std::vector<std::unique_ptr<RenderPhase>> m_renderPhases;

  public:
    void addOneTimeRenderPhase(std::unique_ptr<RenderPhase> renderPhase);
    void addRenderPhase(std::unique_ptr<RenderPhase> renderPhase);

    void processRenderPhaseChain(const std::vector<std::unique_ptr<RenderPhase>> &toProcess, 
        uint32_t imageIndex, VkRect2D renderArea, const CameraABC &mainCamera,
        const std::vector<std::shared_ptr<Light>> &lights, const std::unique_ptr<ProbeGrid> &probeGrid,
        const VkSemaphore *inWaitSemaphore, const VkSemaphore **outAcquireSemaphore);

    void processRendering(uint32_t imageIndex, VkRect2D renderArea, const CameraABC &mainCamera,
                          const std::vector<std::shared_ptr<Light>> &lights, const std::unique_ptr<ProbeGrid> &probeGrid);

    void swapAllRenderPhasesBackBuffers();

    void updateSwapchainOnRenderPhases(const SwapChain* swapchain);

  public:
    [[nodiscard]] VkSemaphore getFirstPhaseCurrentAcquireSemaphore() const;
    [[nodiscard]] VkSemaphore getLastPhaseCurrentRenderSemaphore() const;
    [[nodiscard]] std::vector<VkFence> getAllCurrentFences() const;
};