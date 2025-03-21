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

#include "render_phase.hpp"

RenderPhase::~RenderPhase()
{
    if (!m_device.lock())
        return;

    auto deviceHandle = m_device.lock()->getHandle();

    // TODO : wait queue instead of device
    vkDeviceWaitIdle(deviceHandle);

    for (int i = 0; i < m_backBuffers.size(); ++i)
    {
        vkDestroyFence(deviceHandle, m_backBuffers[i].inFlightFence, nullptr);
        vkDestroySemaphore(deviceHandle, m_backBuffers[i].renderSemaphore, nullptr);
        vkDestroySemaphore(deviceHandle, m_backBuffers[i].acquireSemaphore, nullptr);
    }

    m_renderPass.reset();
}

void RenderPhase::registerRenderState(std::shared_ptr<RenderStateABC> renderState)
{
    for (int i = 0; i < m_renderPass->getImageCount(); ++i)
    {
        renderState->updateDescriptorSets(m_parentPhase, i);
    }
    m_renderStates.emplace_back(renderState);
}

void RenderPhase::recordBackBuffer(uint32_t imageIndex, uint32_t singleFrameRenderIndex, VkRect2D renderArea, const CameraABC &camera,
                                   const std::vector<std::shared_ptr<Light>> &lights) const
{
    if (singleFrameRenderIndex > 0)
    {
        VkFence currentFence = getCurrentFence();
        vkWaitForFences(m_device.lock()->getHandle(), 1, &currentFence, VK_TRUE, UINT64_MAX);
        vkResetFences(m_device.lock()->getHandle(), 1, &currentFence);
    }

    const VkCommandBuffer &commandBuffer = getCurrentBackBuffer().commandBuffer;

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
        .renderArea = renderArea,
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data(),
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    for (int i = 0; i < m_renderStates.size(); ++i)
    {
        m_renderStates[i]->updatePushConstants(commandBuffer, imageIndex, singleFrameRenderIndex, camera, lights);
        m_renderStates[i]->updateUniformBuffers(m_backBufferIndex, singleFrameRenderIndex, camera, lights);
        m_renderStates[i]->updateDescriptorSetsPerFrame(m_parentPhase, imageIndex);

        if (const auto &pipeline = m_renderStates[i]->getPipeline())
        {
            pipeline->recordBind(commandBuffer, imageIndex, renderArea);
        }

        m_renderStates[i]->recordBackBufferDescriptorSetsCommands(commandBuffer, m_backBufferIndex);
        m_renderStates[i]->recordBackBufferDrawObjectCommands(commandBuffer);
    }

    vkCmdEndRenderPass(commandBuffer);

    res = vkEndCommandBuffer(commandBuffer);
    if (res != VK_SUCCESS)
        std::cerr << "Failed to record command buffer : " << res << std::endl;
}

void RenderPhase::submitBackBuffer(const VkSemaphore *waitSemaphoreOverride) const
{
    VkSemaphore waitSemaphores[] = {waitSemaphoreOverride ? *waitSemaphoreOverride
                                                             : getCurrentBackBuffer().acquireSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {getCurrentRenderSemaphore()};
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &getCurrentBackBuffer().commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signalSemaphores,
    };

    VkResult res = vkQueueSubmit(m_device.lock()->getGraphicsQueue(), 1, &submitInfo, getCurrentFence());
    if (res != VK_SUCCESS)
        std::cerr << "Failed to submit draw command buffer : " << res << std::endl;
}

void RenderPhase::swapBackBuffers()
{
    m_backBufferIndex = (m_backBufferIndex + 1) % m_backBuffers.size();
}

std::unique_ptr<RenderPhase> RenderPhaseBuilder::build()
{
    assert(m_product->m_renderPass);
    assert(m_device.lock());

    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    // back buffers

    m_product->m_backBuffers.resize(m_bufferingType);

    VkCommandBufferAllocateInfo commandBufferAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = devicePtr->getCommandPool(),
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1U,
    };
    for (int i = 0; i < m_bufferingType; ++i)
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
    for (int i = 0; i < m_bufferingType; ++i)
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

    return std::move(m_product);
}

void RenderPhase::updateSwapchainOnRenderPass(const SwapChain* newSwapchain) 
{
    m_renderPass->buildFramebuffers(newSwapchain, true);
}
