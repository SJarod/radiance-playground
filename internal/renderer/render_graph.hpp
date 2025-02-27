#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

class RenderPhase;
class CameraABC;
class Light;

/**
 * @brief manages the relation ship between each phase (submit semaphores)
 *
 */
class RenderGraph
{
  public:
    std::vector<std::unique_ptr<RenderPhase>> m_renderPhases;

  public:
    void processRendering(uint32_t imageIndex, VkRect2D renderArea, const CameraABC &mainCamera,
                          const std::vector<std::shared_ptr<Light>> &lights);

    void swapAllRenderPhasesBackBuffers();

    [[nodiscard]] VkSemaphore getFirstPhaseCurrentAcquireSemaphore() const;
    [[nodiscard]] VkSemaphore getLastPhaseCurrentRenderSemaphore() const;
    [[nodiscard]] std::vector<VkFence> getAllCurrentFences() const;
};