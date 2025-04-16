#include <cassert>

#include "engine/camera.hpp"
#include "render_phase.hpp"
#include "renderer/light.hpp"
#include "engine/probe_grid.hpp"

#include "render_graph.hpp"

void RenderGraph::addOneTimeRenderPhase(std::unique_ptr<RenderPhase> renderPhase)
{
    m_oneTimeRenderPhases.push_back(std::move(renderPhase));
}

void RenderGraph::addRenderPhase(std::unique_ptr<RenderPhase> renderPhase)
{
    m_renderPhases.push_back(std::move(renderPhase));
}

void RenderGraph::processRenderPhaseChain(const std::vector<std::unique_ptr<RenderPhase>> &toProcess, uint32_t imageIndex, VkRect2D renderArea, const CameraABC& mainCamera,
    const std::vector<std::shared_ptr<Light>> &lights, const std::vector<std::unique_ptr<Probe>> &probes, const VkSemaphore *inWaitSemaphore, const VkSemaphore **outAcquireSemaphore)
{
    const VkSemaphore* lastAcquireSemaphore = inWaitSemaphore;
    for (int i = 0; i < toProcess.size(); ++i)
    {
        const RenderPhase* currentPhase = toProcess[i].get();
        for (uint32_t singleFrameRenderIndex = 0u; singleFrameRenderIndex < currentPhase->getSingleFrameRenderCount(); singleFrameRenderIndex++)
        {
            for (uint32_t poolIndex = 0u; poolIndex < currentPhase->getRenderPass()->getFramebufferPoolSize(); poolIndex++)
            {
                currentPhase->recordBackBuffer(imageIndex, singleFrameRenderIndex, poolIndex, renderArea, mainCamera, lights, probes);

                currentPhase->submitBackBuffer(lastAcquireSemaphore, poolIndex);
                lastAcquireSemaphore = &currentPhase->getCurrentRenderSemaphore(poolIndex);
            }
        }
    }

    if (outAcquireSemaphore)
        *outAcquireSemaphore = lastAcquireSemaphore;
}

void RenderGraph::processRendering(uint32_t imageIndex, VkRect2D renderArea, const CameraABC &mainCamera,
                                   const std::vector<std::shared_ptr<Light>> &lights, const std::vector<std::unique_ptr<Probe>> &probes)
{
    const VkSemaphore *lastAcquireSemaphore = nullptr;
    if (m_shouldRenderOneTimePhases)
    {
        processRenderPhaseChain(m_oneTimeRenderPhases, imageIndex, renderArea, mainCamera, lights, probes, nullptr, &lastAcquireSemaphore);

        m_shouldRenderOneTimePhases = false;
    }

    processRenderPhaseChain(m_renderPhases, imageIndex, renderArea, mainCamera, lights, probes, lastAcquireSemaphore, nullptr);
}

void RenderGraph::updateSwapchainOnRenderPhases(const SwapChain* swapchain)
{
    for (unsigned int i = 0; i < m_renderPhases.size(); ++i)
    {
        m_renderPhases[i]->updateSwapchainOnRenderPass(swapchain);
    }
}

void RenderGraph::swapAllRenderPhasesBackBuffers()
{
    for (auto &phase : m_renderPhases)
    {
        for (uint32_t poolIndex = 0u; poolIndex < phase->getRenderPass()->getFramebufferPoolSize(); poolIndex++)
        {
            phase->swapBackBuffers(poolIndex);
        }
    }
}

VkSemaphore RenderGraph::getFirstPhaseCurrentAcquireSemaphore() const
{
    assert(m_renderPhases.size() != 0 || m_oneTimeRenderPhases.size() != 0);

    if (m_shouldRenderOneTimePhases)
    {
        if (m_oneTimeRenderPhases.size() > 0u)
            return m_oneTimeRenderPhases.front()->getCurrentAcquireSemaphore(0u);
    }

    return m_renderPhases.front()->getCurrentAcquireSemaphore(0u);
}

VkSemaphore RenderGraph::getLastPhaseCurrentRenderSemaphore() const
{
    assert(m_renderPhases.size() != 0);
    return m_renderPhases.back()->getCurrentRenderSemaphore(m_renderPhases.back()->getRenderPass()->getFramebufferPoolSize() - 1u);
}

std::vector<VkFence> RenderGraph::getAllCurrentFences() const
{
    assert(m_renderPhases.size() != 0);
    std::vector<VkFence> fences;
    fences.reserve(m_renderPhases.size() + m_oneTimeRenderPhases.size());

    if (m_shouldRenderOneTimePhases)
    {
        for (const auto& phase : m_oneTimeRenderPhases)
        {
            if (phase->getSingleFrameRenderCount() > 0u)
            {
                for (uint32_t poolIndex = 0u; poolIndex < phase->getRenderPass()->getFramebufferPoolSize(); poolIndex++)
                {
                    fences.push_back(phase->getCurrentFence(poolIndex));
                }
            }
        }
    }
    for (const auto &phase : m_renderPhases)
    {
        if (phase->getSingleFrameRenderCount() > 0u)
        {
            for (uint32_t poolIndex = 0u; poolIndex < phase->getRenderPass()->getFramebufferPoolSize(); poolIndex++)
            {
                fences.push_back(phase->getCurrentFence(poolIndex));
            }
        }
    }
    return fences;
}
