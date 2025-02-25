#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "graphics/buffer.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/swapchain.hpp"

#include "mesh.hpp"
#include "texture.hpp"

#include "engine/camera.hpp"
#include "engine/uniform.hpp"

#include "render_state.hpp"

#include "renderer.hpp"

Renderer::~Renderer()
{
    if (!m_device.lock())
        return;

    auto deviceHandle = m_device.lock()->getHandle();

    vkDeviceWaitIdle(deviceHandle);

    for (int i = 0; i < m_bufferingType; ++i)
    {
        vkDestroyFence(deviceHandle, m_backBuffers[i].inFlightFence, nullptr);
        vkDestroySemaphore(deviceHandle, m_backBuffers[i].renderSemaphore, nullptr);
        vkDestroySemaphore(deviceHandle, m_backBuffers[i].acquireSemaphore, nullptr);
    }

    m_renderPass.reset();
}

void Renderer::registerRenderState(std::shared_ptr<RenderStateABC> renderState)
{
    m_renderStates.emplace_back(renderState);
}

uint32_t Renderer::acquireBackBuffer()
{
    auto deviceHandle = m_device.lock()->getHandle();

    vkWaitForFences(deviceHandle, 1, &m_backBuffers[m_backBufferIndex].inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(deviceHandle, 1, &m_backBuffers[m_backBufferIndex].inFlightFence);

    uint32_t imageIndex;
    VkResult res =
        vkAcquireNextImageKHR(deviceHandle, m_swapchain->getHandle(), UINT64_MAX,
                              m_backBuffers[m_backBufferIndex].acquireSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to acquire next image : " << res << std::endl;
        return -1;
    }

    return imageIndex;
}

void Renderer::recordRenderers(uint32_t imageIndex, const Camera &camera, const std::vector<std::shared_ptr<Light>> &lights)
{
    VkCommandBuffer &commandBuffer = m_backBuffers[m_backBufferIndex].commandBuffer;

    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo commandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = nullptr,
    };
    VkResult res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to begin recording command buffer : " << res << std::endl;
        return;
    }

    VkClearValue clearColor = {
        .color = {0.2f, 0.2f, 0.2f, 1.f},
    };
    VkClearValue clearDepth = {
        .depthStencil = {1.f, 0},
    };
    std::array<VkClearValue, 2> clearValues = {clearColor, clearDepth};
    VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = m_renderPass->getHandle(),
        .framebuffer = m_renderPass->getFramebuffer(imageIndex),
        .renderArea =
            {
                .offset = {0, 0},
                .extent = m_swapchain->getExtent(),
            },
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data(),
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    for (int i = 0; i < m_renderStates.size(); ++i)
    {
        m_renderStates[i]->updateUniformBuffers(imageIndex, camera, lights);

        m_renderStates[i]->getPipeline()->recordBind(commandBuffer, imageIndex);

        m_renderStates[i]->recordBackBufferDescriptorSetsCommands(commandBuffer, imageIndex);
        m_renderStates[i]->recordBackBufferDrawObjectCommands(commandBuffer);
    }

    vkCmdEndRenderPass(commandBuffer);

    res = vkEndCommandBuffer(commandBuffer);
    if (res != VK_SUCCESS)
        std::cerr << "Failed to record command buffer : " << res << std::endl;
}

void Renderer::submitBackBuffer()
{
    VkSemaphore waitSemaphores[] = {m_backBuffers[m_backBufferIndex].acquireSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {m_backBuffers[m_backBufferIndex].renderSemaphore};
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &m_backBuffers[m_backBufferIndex].commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signalSemaphores,
    };

    VkResult res = vkQueueSubmit(m_device.lock()->getGraphicsQueue(), 1, &submitInfo,
                                 m_backBuffers[m_backBufferIndex].inFlightFence);
    if (res != VK_SUCCESS)
        std::cerr << "Failed to submit draw command buffer : " << res << std::endl;
}

void Renderer::presentBackBuffer(uint32_t imageIndex)
{
    VkSwapchainKHR swapchains[] = {m_swapchain->getHandle()};
    VkSemaphore waitSemaphores[] = {m_backBuffers[m_backBufferIndex].renderSemaphore};
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

void Renderer::swapBuffers()
{
    m_backBufferIndex = (m_backBufferIndex + 1) % m_bufferingType;
}

std::unique_ptr<Renderer> RendererBuilder::build()
{
    assert(m_device.lock());
    assert(m_swapchain);

    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    RenderPassBuilder rpb;
    rpb.setDevice(m_device);
    rpb.setSwapChain(m_swapchain);
    rpb.addColorAttachment(m_swapchain->getImageFormat());
    rpb.addDepthAttachment(m_swapchain->getDepthImageFormat());
    m_product->m_renderPass = rpb.build();

    // back buffers

    m_product->m_backBuffers.resize(m_product->m_bufferingType);

    VkCommandBufferAllocateInfo commandBufferAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = devicePtr->getCommandPool(),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1U,
    };
    for (int i = 0; i < m_product->m_bufferingType; ++i)
    {
        VkResult res =
            vkAllocateCommandBuffers(deviceHandle, &commandBufferAllocInfo, &m_product->m_backBuffers[i].commandBuffer);
        if (res != VK_SUCCESS)
        {
            std::cerr << "Failed to allocate command buffers : " << res << std::endl;
            return nullptr;
        }
    }

    // synchronization

    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VkFenceCreateInfo fenceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    for (int i = 0; i < m_product->m_bufferingType; ++i)
    {
        VkResult res = vkCreateSemaphore(deviceHandle, &semaphoreCreateInfo, nullptr,
                                         &m_product->m_backBuffers[i].acquireSemaphore);
        if (res != VK_SUCCESS)
        {
            std::cerr << "Failed to create semaphore : " << res << std::endl;
            return nullptr;
        }

        res = vkCreateSemaphore(deviceHandle, &semaphoreCreateInfo, nullptr,
                                &m_product->m_backBuffers[i].renderSemaphore);
        if (res != VK_SUCCESS)
        {
            std::cerr << "Failed to create semaphore : " << res << std::endl;
            return nullptr;
        }

        res = vkCreateFence(deviceHandle, &fenceCreateInfo, nullptr, &m_product->m_backBuffers[i].inFlightFence);
        if (res != VK_SUCCESS)
        {
            std::cerr << "Failed to create fence : " << res << std::endl;
            return nullptr;
        }
    }

    auto result = std::move(m_product);
    return result;
}