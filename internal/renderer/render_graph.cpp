#include <cassert>

#include "engine/camera.hpp"
#include "render_phase.hpp"
#include "renderer/light.hpp"

#include "render_graph.hpp"

void RenderGraph::processRendering(uint32_t imageIndex, VkRect2D renderArea, const CameraABC &mainCamera,
                                   const std::vector<std::shared_ptr<Light>> &lights)
{
    const VkSemaphore *acquireSemaphore = nullptr;
    for (int i = 0; i < m_renderPhases.size(); ++i)
    {
        m_renderPhases[i]->recordBackBuffer(imageIndex, renderArea, mainCamera, lights);

        m_renderPhases[i]->submitBackBuffer(acquireSemaphore);
        acquireSemaphore = &m_renderPhases[i]->getCurrentRenderSemaphore();
    }
}

void RenderGraph::swapAllRenderPhasesBackBuffers()
{
    for (auto &phase : m_renderPhases)
    {
        phase->swapBackBuffers();
    }
}

VkSemaphore RenderGraph::getFirstPhaseCurrentAcquireSemaphore() const
{
    assert(m_renderPhases.size() != 0);
    return m_renderPhases.front()->getCurrentAcquireSemaphore();
}

VkSemaphore RenderGraph::getLastPhaseCurrentRenderSemaphore() const
{
    assert(m_renderPhases.size() != 0);
    return m_renderPhases.back()->getCurrentRenderSemaphore();
}

std::vector<VkFence> RenderGraph::getAllCurrentFences() const
{
    assert(m_renderPhases.size() != 0);
    std::vector<VkFence> fences;
    fences.reserve(m_renderPhases.size());
    for (const auto &phase : m_renderPhases)
    {
        fences.push_back(phase->getCurrentFence());
    }
    return fences;
}
