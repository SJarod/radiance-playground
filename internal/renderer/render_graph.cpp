#include <cassert>

#include "engine/camera.hpp"
#include "engine/probe_grid.hpp"
#include "render_phase.hpp"
#include "renderer/light.hpp"

#include "render_graph.hpp"

void RenderGraph::addOneTimeRenderPhase(std::unique_ptr<RenderPhase> renderPhase)
{
    m_oneTimeRenderPhases.push_back(std::move(renderPhase));
}

void RenderGraph::addRenderPhase(std::unique_ptr<RenderPhase> renderPhase)
{
    m_renderPhases.push_back(std::move(renderPhase));
}

void RenderGraph::addPhase(std::unique_ptr<BasePhaseABC> phase)
{
    m_renderPhases.push_back(std::move(phase));
}

void RenderGraph::processRenderPhaseChain(const std::vector<std::unique_ptr<BasePhaseABC>> &toProcess,
                                          uint32_t imageIndex, VkRect2D renderArea, const CameraABC &mainCamera,
                                          const std::vector<std::shared_ptr<Light>> &lights,
                                          const std::shared_ptr<ProbeGrid> &probeGrid,
                                          const VkSemaphore *inWaitSemaphore, const VkSemaphore **outAcquireSemaphore)
{
    const VkSemaphore *lastAcquireSemaphore = inWaitSemaphore;
    for (int i = 0; i < toProcess.size(); ++i)
    {
        if (const RenderPhase *currentPhase = dynamic_cast<RenderPhase *>(toProcess[i].get()))
        {
            for (uint32_t singleFrameRenderIndex = 0u;
                 singleFrameRenderIndex < currentPhase->getSingleFrameRenderCount(); singleFrameRenderIndex++)
            {
                for (uint32_t poolIndex = 0u; poolIndex < currentPhase->getRenderPass()->getFramebufferPoolSize();
                     poolIndex++)
                {
                    currentPhase->recordBackBuffer(imageIndex, singleFrameRenderIndex, poolIndex, renderArea,
                                                   mainCamera, lights, probeGrid);

                    currentPhase->submitBackBuffer(lastAcquireSemaphore, poolIndex);
                    lastAcquireSemaphore = &currentPhase->getCurrentRenderSemaphore(poolIndex);
                }
            }
        }
        else if (const ComputePhase *phase = dynamic_cast<ComputePhase *>(toProcess[i].get()))
        {
            phase->recordBackBuffer();

            phase->submitBackBuffer(lastAcquireSemaphore);
            lastAcquireSemaphore = &phase->getCurrentRenderSemaphore();
        }
    }

    if (outAcquireSemaphore)
        *outAcquireSemaphore = lastAcquireSemaphore;
}

void RenderGraph::processRendering(uint32_t imageIndex, VkRect2D renderArea, const CameraABC &mainCamera,
                                   const std::vector<std::shared_ptr<Light>> &lights,
                                   const std::shared_ptr<ProbeGrid> &probeGrid)
{
    const VkSemaphore *lastAcquireSemaphore = nullptr;
    if (m_shouldRenderOneTimePhases)
    {
        processRenderPhaseChain(m_oneTimeRenderPhases, imageIndex, renderArea, mainCamera, lights, probeGrid, nullptr,
                                &lastAcquireSemaphore);

        m_shouldRenderOneTimePhases = false;
    }

    processRenderPhaseChain(m_renderPhases, imageIndex, renderArea, mainCamera, lights, probeGrid, lastAcquireSemaphore,
                            nullptr);
}

void RenderGraph::updateSwapchainOnRenderPhases(const SwapChain *swapchain)
{
    for (unsigned int i = 0; i < m_renderPhases.size(); ++i)
    {
        if (RenderPhase *currentPhase = dynamic_cast<RenderPhase *>(m_renderPhases[i].get()))
            currentPhase->updateSwapchainOnRenderPass(swapchain);
    }
}

void RenderGraph::swapAllRenderPhasesBackBuffers()
{
    for (auto &phase : m_renderPhases)
    {
        if (RenderPhase *currentPhase = dynamic_cast<RenderPhase *>(phase.get()))
        {
            for (uint32_t poolIndex = 0u; poolIndex < currentPhase->getRenderPass()->getFramebufferPoolSize();
                 poolIndex++)
            {
                currentPhase->swapBackBuffers(poolIndex);
            }
        }
        else if (ComputePhase *currentPhase = dynamic_cast<ComputePhase *>(phase.get()))
        {
            currentPhase->swapBackBuffers();
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

    uint32_t pooledFramebufferIndex = 0u;
    if (const RenderPhase *currentPhase = dynamic_cast<RenderPhase *>(m_renderPhases.back().get()))
        pooledFramebufferIndex = currentPhase->getRenderPass()->getFramebufferPoolSize() - 1u;

    return m_renderPhases.back()->getCurrentRenderSemaphore(pooledFramebufferIndex);
}

std::vector<VkFence> RenderGraph::getAllCurrentFences() const
{
    assert(m_renderPhases.size() != 0);
    std::vector<VkFence> fences;
    fences.reserve(m_renderPhases.size() + m_oneTimeRenderPhases.size());

    if (m_shouldRenderOneTimePhases)
    {
        for (const auto &phase : m_oneTimeRenderPhases)
        {
            if (RenderPhase *currentPhase = dynamic_cast<RenderPhase *>(phase.get()))
            {
                if (currentPhase->getSingleFrameRenderCount() > 0u)
                {
                    for (uint32_t poolIndex = 0u; poolIndex < currentPhase->getRenderPass()->getFramebufferPoolSize();
                         poolIndex++)
                    {
                        fences.push_back(currentPhase->getCurrentFence(poolIndex));
                    }
                }
            }
        }
    }
    for (const auto &phase : m_renderPhases)
    {
        if (RenderPhase *currentPhase = dynamic_cast<RenderPhase *>(phase.get()))
        {
            if (currentPhase->getSingleFrameRenderCount() > 0u)
            {
                for (uint32_t poolIndex = 0u; poolIndex < currentPhase->getRenderPass()->getFramebufferPoolSize();
                     poolIndex++)
                {
                    fences.push_back(currentPhase->getCurrentFence(poolIndex));
                }
            }
        }
    }
    return fences;
}
