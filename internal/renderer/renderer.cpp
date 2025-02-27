#include <iostream>

#include <vulkan/vulkan.h>

#include "graphics/device.hpp"
#include "graphics/swapchain.hpp"

#include "render_graph.hpp"
#include "render_phase.hpp"

#include "renderer.hpp"

uint32_t Renderer::acquireNextSwapChainImage()
{
    auto deviceHandle = m_device.lock()->getHandle();

    auto fences = m_renderGraph->getAllCurrentFences();
    vkWaitForFences(deviceHandle, static_cast<uint32_t>(fences.size()), fences.data(), VK_TRUE, UINT64_MAX);
    vkResetFences(deviceHandle, static_cast<uint32_t>(fences.size()), fences.data());

    auto acquireSemaphore = m_renderGraph->getFirstPhaseCurrentAcquireSemaphore();
    uint32_t imageIndex;
    VkResult res = vkAcquireNextImageKHR(deviceHandle, m_swapchain->getHandle(), UINT64_MAX, acquireSemaphore,
                                         VK_NULL_HANDLE, &imageIndex);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to acquire next image : " << res << std::endl;
        return -1;
    }

    return imageIndex;
}

void Renderer::presentBackBuffer(uint32_t imageIndex)
{
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
}

void Renderer::renderFrame(VkRect2D renderArea, const CameraABC &mainCamera,
                           const std::vector<std::shared_ptr<Light>> &lights)
{
    uint32_t imageIndex = acquireNextSwapChainImage();

    m_renderGraph->processRendering(imageIndex, renderArea, mainCamera, lights);
    presentBackBuffer(imageIndex);

    m_renderGraph->swapAllRenderPhasesBackBuffers();
}

std::unique_ptr<Renderer> RendererBuilder::build()
{
    return std::move(m_product);
}
