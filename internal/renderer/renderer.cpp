#include <iostream>

#include <tracy/Tracy.hpp>
#include <vulkan/vulkan.h>

#include "engine/probe_grid.hpp"

#include "graphics/device.hpp"
#include "graphics/swapchain.hpp"

#include "render_graph.hpp"
#include "render_phase.hpp"

#include "renderer.hpp"

VkResult Renderer::acquireNextSwapChainImage(uint32_t &nextImageIndex)
{
    ZoneScoped;

    auto deviceHandle = m_device.lock()->getHandle();

    auto fences = m_renderGraph->getAllCurrentFences();
    vkWaitForFences(deviceHandle, static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, UINT64_MAX);

    auto acquireSemaphore = m_renderGraph->getFirstPhaseCurrentAcquireSemaphore();
    VkResult res = vkAcquireNextImageKHR(deviceHandle, m_swapchain->getHandle(), UINT64_MAX, acquireSemaphore,
                                         VK_NULL_HANDLE, &nextImageIndex);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to acquire next image : " << res << std::endl;
        return res;
    }

    vkResetFences(deviceHandle, static_cast<uint32_t>(fences.size()), fences.data());

    return res;
}

VkResult Renderer::presentBackBuffer(uint32_t imageIndex)
{
    ZoneScoped;

    VkSwapchainKHR swapchains[] = {m_swapchain->getHandle()};
    auto renderSemaphore = m_renderGraph->getLastPhaseCurrentRenderSemaphore();
    VkSemaphore waitSemaphores[] = {renderSemaphore};
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphores,
        .swapchainCount = 1,
        .pSwapchains = swapchains,
        .pImageIndices = &imageIndex,
        .pResults = nullptr,
    };

    VkResult res = vkQueuePresentKHR(m_device.lock()->getPresentQueue(), &presentInfo);
    if (res != VK_SUCCESS)
        std::cerr << "Failed to present : " << res << std::endl;

    return res;
}

VkResult Renderer::renderFrame(VkRect2D renderArea, const CameraABC &mainCamera,
                               const std::vector<std::shared_ptr<Light>> &lights,
                               const std::shared_ptr<ProbeGrid> &probeGrid)
{
    ZoneScoped;

    uint32_t imageIndex;
    VkResult res = acquireNextSwapChainImage(imageIndex);
    if (res != VK_SUCCESS)
        return res;

    m_renderGraph->processRendering(imageIndex, renderArea, mainCamera, lights, probeGrid);
    res = presentBackBuffer(imageIndex);
    if (res != VK_SUCCESS)
        return res;

    m_renderGraph->swapAllRenderPhasesBackBuffers();

    return res;
}

std::unique_ptr<Renderer> RendererBuilder::build()
{
    return std::move(m_product);
}
