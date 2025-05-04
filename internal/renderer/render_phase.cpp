#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "graphics/buffer.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/swapchain.hpp"

#include "mesh.hpp"
#include "texture.hpp"

#include "engine/camera.hpp"
#include "engine/probe_grid.hpp"
#include "engine/uniform.hpp"

#include "render_state.hpp"

#include "render_phase.hpp"

struct RenderPhase::impl
{
    std::unique_ptr<RenderPass> m_renderPass;

    std::vector<std::vector<std::shared_ptr<RenderStateABC>>> m_pooledRenderStates;

    uint32_t m_singleFrameRenderCount = 1u;

    int m_backBufferIndex = 0;
    std::vector<std::vector<BackBufferT>> m_pooledBackBuffers;

    bool m_isCapturePhase = false;
};

RenderPhase::~RenderPhase()
{
    if (m_device.expired())
        return;
    auto deviceHandle = m_device.lock()->getHandle();

    // TODO : wait queue instead of device
    vkDeviceWaitIdle(deviceHandle);

    for (uint32_t i = 0u; i < pImpl->m_pooledBackBuffers.size(); i++)
    {
        const auto &backBuffers = pImpl->m_pooledBackBuffers[i];
        for (uint32_t j = 0u; j < backBuffers.size(); j++)
        {
            const BackBufferT &backbuffer = backBuffers[j];
            vkDestroyFence(deviceHandle, backbuffer.inFlightFence, nullptr);
            vkDestroySemaphore(deviceHandle, backbuffer.renderSemaphore, nullptr);
            vkDestroySemaphore(deviceHandle, backbuffer.acquireSemaphore, nullptr);
        }
    }

    pImpl->m_renderPass.reset();

    pImpl.reset();
}

const BackBufferT &RenderPhase::getCurrentBackBuffer(uint32_t pooledFramebufferIndex) const
{
    return pImpl->m_pooledBackBuffers[pooledFramebufferIndex][pImpl->m_backBufferIndex];
}

const int RenderPhase::getSingleFrameRenderCount() const
{
    return pImpl->m_singleFrameRenderCount;
}

const VkSemaphore &RenderPhase::getCurrentAcquireSemaphore(uint32_t pooledFramebufferIndex) const
{
    return getCurrentBackBuffer(pooledFramebufferIndex).acquireSemaphore;
}
const VkSemaphore &RenderPhase::getCurrentRenderSemaphore(uint32_t pooledFramebufferIndex) const
{
    return getCurrentBackBuffer(pooledFramebufferIndex).renderSemaphore;
}
const VkFence &RenderPhase::getCurrentFence(uint32_t pooledFramebufferIndex) const
{
    return getCurrentBackBuffer(pooledFramebufferIndex).inFlightFence;
}
const RenderPass *RenderPhase::getRenderPass() const
{
    return pImpl->m_renderPass.get();
}

void RenderPhase::registerRenderStateToAllPool(std::shared_ptr<RenderStateABC> renderState)
{
    for (int poolIndex = 0; poolIndex < pImpl->m_renderPass->getFramebufferPoolSize(); poolIndex++)
    {
        for (int imageIndex = 0; imageIndex < pImpl->m_renderPass->getImageCount(poolIndex); ++imageIndex)
        {
            renderState->updateDescriptorSets(m_parentPhase, imageIndex);
        }

        pImpl->m_pooledRenderStates[poolIndex].push_back(renderState);
    }
}

void RenderPhase::registerRenderStateToSpecificPool(std::shared_ptr<RenderStateABC> renderState,
                                                    uint32_t pooledFramebufferIndex)
{
    if (pooledFramebufferIndex >= pImpl->m_pooledRenderStates.size())
    {
        std::cerr << "Failed to add renderstate to renderstate pool" << std::endl;
        return;
    }

    for (int poolIndex = 0; poolIndex < pImpl->m_renderPass->getFramebufferPoolSize(); poolIndex++)
    {
        for (int imageIndex = 0; imageIndex < pImpl->m_renderPass->getImageCount(poolIndex); ++imageIndex)
        {
            renderState->updateDescriptorSets(m_parentPhase, imageIndex);
        }
    }

    pImpl->m_pooledRenderStates[pooledFramebufferIndex].push_back(renderState);
}

void RenderPhase::recordBackBuffer(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex, VkRect2D renderArea, const CameraABC &camera,
                                   const std::vector<std::shared_ptr<Light>> &lights, const std::shared_ptr<ProbeGrid> &probeGrid) const
{
    if (singleFrameRenderIndex > 0)
    {
        VkFence currentFence = getCurrentFence(pooledFramebufferIndex);
        vkWaitForFences(m_device.lock()->getHandle(), 1, &currentFence, VK_TRUE, UINT64_MAX);
        vkResetFences(m_device.lock()->getHandle(), 1, &currentFence);
    }

    const VkCommandBuffer &commandBuffer = getCurrentBackBuffer(pooledFramebufferIndex).commandBuffer;

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

    renderArea.extent.width =
        std::min(renderArea.extent.width - renderArea.offset.x, pImpl->m_renderPass->getMinRenderArea().extent.width);
    renderArea.extent.height =
        std::min(renderArea.extent.height - renderArea.offset.y, pImpl->m_renderPass->getMinRenderArea().extent.height);

    VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = pImpl->m_renderPass->getHandle(),
        .framebuffer = pImpl->m_renderPass->getFramebuffer(pooledFramebufferIndex, imageIndex),
        .renderArea = renderArea,
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data(),
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    const auto &renderStates = pImpl->m_pooledRenderStates[pooledFramebufferIndex];
    for (int i = 0; i < renderStates.size(); ++i)
    {
        RenderStateABC *renderState = renderStates[i].get();

        if (const auto &pipeline = renderState->getPipeline())
        {
            pipeline->recordBind(commandBuffer, imageIndex, renderArea);
        }

        renderState->updatePushConstants(commandBuffer, imageIndex, singleFrameRenderIndex, camera, lights);
        renderState->updateUniformBuffers(pImpl->m_backBufferIndex, singleFrameRenderIndex, pooledFramebufferIndex,
                                          camera, lights, probeGrid, pImpl->m_isCapturePhase);
        renderState->updateDescriptorSetsPerFrame(m_parentPhase, imageIndex, pImpl->m_backBufferIndex);
        for (uint32_t subObjectIndex = 0u; subObjectIndex < renderState->getSubObjectCount(); subObjectIndex++)
        {
            renderState->recordBackBufferDescriptorSetsCommands(commandBuffer, subObjectIndex,
                                                                pImpl->m_backBufferIndex);
            renderState->recordBackBufferDrawObjectCommands(commandBuffer, subObjectIndex);
        }
    }

    vkCmdEndRenderPass(commandBuffer);

    res = vkEndCommandBuffer(commandBuffer);
    if (res != VK_SUCCESS)
        std::cerr << "Failed to record command buffer : " << res << std::endl;
}

void RenderPhase::submitBackBuffer(const VkSemaphore *waitSemaphoreOverride, uint32_t pooledFramebufferIndex) const
{
    const BackBufferT &currentBackBuffer = getCurrentBackBuffer(pooledFramebufferIndex);
    VkSemaphore waitSemaphores[] = {waitSemaphoreOverride ? *waitSemaphoreOverride
                                                          : currentBackBuffer.acquireSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signalSemaphores[] = {getCurrentRenderSemaphore(pooledFramebufferIndex)};
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .commandBufferCount = 1,
        .pCommandBuffers = &currentBackBuffer.commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = signalSemaphores,
    };

    VkResult res =
        vkQueueSubmit(m_device.lock()->getGraphicsQueue(), 1, &submitInfo, getCurrentFence(pooledFramebufferIndex));
    if (res != VK_SUCCESS)
        std::cerr << "Failed to submit draw command buffer : " << res << std::endl;
}

void RenderPhase::swapBackBuffers(uint32_t pooledFramebufferIndex)
{
    pImpl->m_backBufferIndex =
        (pImpl->m_backBufferIndex + 1) % pImpl->m_pooledBackBuffers[pooledFramebufferIndex].size();
}

std::unique_ptr<RenderPhase> RenderPhaseBuilder::build()
{
    assert(m_product->pImpl->m_renderPass);
    assert(m_device.lock());

    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    // back buffers
    const uint32_t poolSize = m_product->pImpl->m_renderPass->getFramebufferPoolSize();

    m_product->pImpl->m_pooledRenderStates.resize(poolSize);
    m_product->pImpl->m_pooledBackBuffers.resize(poolSize);
    for (uint32_t poolIndex = 0u; poolIndex < poolSize; poolIndex++)
    {
        m_product->pImpl->m_pooledBackBuffers[poolIndex].resize(m_bufferingType);

        auto &backBuffers = m_product->pImpl->m_pooledBackBuffers[poolIndex];

        VkCommandBufferAllocateInfo commandBufferAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = devicePtr->getCommandPool(),
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1U,
        };
        for (int i = 0; i < m_bufferingType; ++i)
        {
            VkResult res =
                vkAllocateCommandBuffers(deviceHandle, &commandBufferAllocInfo, &backBuffers[i].commandBuffer);
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
            VkResult res =
                vkCreateSemaphore(deviceHandle, &semaphoreCreateInfo, nullptr, &backBuffers[i].acquireSemaphore);
            if (res != VK_SUCCESS)
            {
                std::cerr << "Failed to create semaphore : " << res << std::endl;
                return nullptr;
            }

            res = vkCreateSemaphore(deviceHandle, &semaphoreCreateInfo, nullptr, &backBuffers[i].renderSemaphore);
            if (res != VK_SUCCESS)
            {
                std::cerr << "Failed to create semaphore : " << res << std::endl;
                return nullptr;
            }

            res = vkCreateFence(deviceHandle, &fenceCreateInfo, nullptr, &backBuffers[i].inFlightFence);
            if (res != VK_SUCCESS)
            {
                std::cerr << "Failed to create fence : " << res << std::endl;
                return nullptr;
            }
        }
    }

    return std::move(m_product);
}

void RenderPhase::updateSwapchainOnRenderPass(const SwapChain *newSwapchain)
{
    const std::vector<std::vector<VkImageView>> &imageViewPool = {newSwapchain->getImageViews()};
    const std::vector<VkImageView> &depthImageViewPool = {newSwapchain->getDepthImageView()};
    pImpl->m_renderPass->buildFramebuffers(imageViewPool, depthImageViewPool, newSwapchain->getExtent(), true);
}

void RenderPhaseBuilder::restart()
{
    m_product = std::unique_ptr<RenderPhase>(new RenderPhase);
    m_product->pImpl = std::make_unique<RenderPhase::impl>();
}

void RenderPhaseBuilder::setRenderPass(std::unique_ptr<RenderPass> renderPass)
{
    m_product->pImpl->m_renderPass = std::move(renderPass);
}
void RenderPhaseBuilder::setSingleFrameRenderCount(uint32_t renderCount)
{
    m_product->pImpl->m_singleFrameRenderCount = renderCount;
}
void RenderPhaseBuilder::setCaptureEnable(bool enable)
{
    m_product->pImpl->m_isCapturePhase = enable;
}