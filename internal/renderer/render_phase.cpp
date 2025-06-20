#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

#include "graphics/buffer.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/swapchain.hpp"

#include "mesh.hpp"
#include "model.hpp"
#include "texture.hpp"

#include "engine/camera.hpp"
#include "engine/probe_grid.hpp"
#include "engine/uniform.hpp"

#include "render_state.hpp"

#include "render_phase.hpp"

#define alignup(x, alignment) ((x + alignment - 1) / alignment) * alignment

RenderPhase::~RenderPhase()
{
    if (!m_device.lock())
        return;

    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    vkQueueWaitIdle(devicePtr->getGraphicsQueue());

    for (uint32_t i = 0u; i < m_pooledBackBuffers.size(); i++)
    {
        const auto &backBuffers = m_pooledBackBuffers[i];
        for (uint32_t j = 0u; j < backBuffers.size(); j++)
        {
            const BackBufferT &backbuffer = backBuffers[j];
            vkDestroyFence(deviceHandle, backbuffer.inFlightFence, nullptr);
            vkDestroySemaphore(deviceHandle, backbuffer.renderSemaphore, nullptr);
            vkDestroySemaphore(deviceHandle, backbuffer.acquireSemaphore, nullptr);
        }
    }

    m_renderPass.reset();
}

void RenderPhase::registerRenderStateToAllPool(std::shared_ptr<RenderStateABC> renderState)
{
    for (int poolIndex = 0; poolIndex < m_pooledBackBuffers.size(); poolIndex++)
    {
        for (int i = 0; i < m_pooledBackBuffers[poolIndex].size(); ++i)
        {
            renderState->updateDescriptorSets(m_parentPhase, i, poolIndex);
        }

        m_pooledRenderStates[poolIndex].push_back(renderState);
    }
}

void RenderPhase::registerRenderStateToSpecificPool(std::shared_ptr<RenderStateABC> renderState,
                                                    uint32_t pooledFramebufferIndex)
{
    if (pooledFramebufferIndex >= m_pooledRenderStates.size())
    {
        std::cerr << "Failed to add renderstate to renderstate pool" << std::endl;
        return;
    }

    for (int poolIndex = 0; poolIndex < m_pooledBackBuffers.size(); poolIndex++)
    {
        for (int i = 0; i < m_pooledBackBuffers[poolIndex].size(); ++i)
        {
            renderState->updateDescriptorSets(m_parentPhase, i, pooledFramebufferIndex);
        }
    }

    m_pooledRenderStates[pooledFramebufferIndex].push_back(renderState);
}

void RenderPhase::recordBackBuffer(uint32_t imageIndex, uint32_t singleFrameRenderIndex,
                                   uint32_t pooledFramebufferIndex, VkRect2D renderArea, const CameraABC &camera,
                                   const std::vector<std::shared_ptr<Light>> &lights,
                                   const std::shared_ptr<ProbeGrid> &probeGrid)
{
    ZoneScoped;

    if (singleFrameRenderIndex > 0)
    {
        VkFence currentFence = getCurrentFence(pooledFramebufferIndex);
        VkResult res = vkWaitForFences(m_device.lock()->getHandle(), 1, &currentFence, VK_TRUE, UINT64_MAX);
        assert(res != VK_TIMEOUT);
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
        .color = {0.05f, 0.05f, 0.05f, 0.f},
    };
    VkClearValue clearDepth = {
        .depthStencil = {1.f, 0},
    };
    std::array<VkClearValue, 2> clearValues = {clearColor, clearDepth};

    assert(m_renderPass.has_value());
    renderArea.extent.width =
        std::min(renderArea.extent.width - renderArea.offset.x, m_renderPass.value()->getMinRenderArea().extent.width);
    renderArea.extent.height = std::min(renderArea.extent.height - renderArea.offset.y,
                                        m_renderPass.value()->getMinRenderArea().extent.height);

    VkRenderPassBeginInfo renderPassBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = m_renderPass.value()->getHandle(),
        .framebuffer = m_renderPass.value()->getFramebuffer(pooledFramebufferIndex, imageIndex),
        .renderArea = renderArea,
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data(),
    };

    const auto &renderStates = m_pooledRenderStates[pooledFramebufferIndex];
    for (int i = 0; i < renderStates.size(); ++i)
    {
        RenderStateABC *renderState = renderStates[i].get();
        renderState->updatePushConstants(commandBuffer, singleFrameRenderIndex, camera, lights);
        renderState->updateUniformBuffers(m_backBufferIndex, singleFrameRenderIndex, pooledFramebufferIndex, camera,
                                          lights, probeGrid, m_isCapturePhase);
        renderState->updateDescriptorSetsPerFrame(m_parentPhase, commandBuffer, m_backBufferIndex,
                                                  pooledFramebufferIndex);
    }

    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    for (int i = 0; i < renderStates.size(); ++i)
    {
        RenderStateABC *renderState = renderStates[i].get();

        if (const auto &pipeline = renderState->getPipeline())
        {
            pipeline->recordBind(commandBuffer, renderArea);
        }

        for (uint32_t subObjectIndex = 0u; subObjectIndex < renderState->getSubObjectCount(); subObjectIndex++)
        {
            renderState->recordBackBufferDescriptorSetsCommands(commandBuffer, subObjectIndex, m_backBufferIndex,
                                                                pooledFramebufferIndex);
            renderState->recordBackBufferDrawObjectCommands(commandBuffer, subObjectIndex);
        }
    }

    vkCmdEndRenderPass(commandBuffer);

    res = vkEndCommandBuffer(commandBuffer);
    if (res != VK_SUCCESS)
        std::cerr << "Failed to record command buffer : " << res << std::endl;

    // keep track of this newly rendered image
    m_lastFramebuffer = std::optional<VkFramebuffer>(renderPassBeginInfo.framebuffer);
    m_lastFramebufferImageResource = m_renderPass.value()->getImageResource(imageIndex);
    m_lastFramebufferImageView =
        std::optional<VkImageView>(m_renderPass.value()->getImageView(pooledFramebufferIndex, imageIndex));
}

void RenderPhase::submitBackBuffer(const VkSemaphore *waitSemaphoreOverride, uint32_t pooledFramebufferIndex) const
{
    ZoneScoped;

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
    {
        std::cerr << "Failed to submit draw command buffer : " << res << std::endl;
        assert(false);
        abort();
    }
}

void RenderPhase::swapBackBuffers(uint32_t pooledFramebufferIndex)
{
    m_backBufferIndex = (m_backBufferIndex + 1) % m_pooledBackBuffers[pooledFramebufferIndex].size();
}

std::unique_ptr<RenderPhase> RenderPhaseBuilder<RenderTypeE::RASTER>::build()
{
    assert(m_device.lock());

    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    // back buffers
    uint32_t poolSize = 1u;
    if (m_product->m_renderPass.has_value())
        poolSize = m_product->m_renderPass.value()->getFramebufferPoolSize();

    m_product->m_pooledRenderStates.resize(poolSize);
    m_product->m_pooledBackBuffers.resize(poolSize);
    for (uint32_t poolIndex = 0u; poolIndex < poolSize; poolIndex++)
    {
        m_product->m_pooledBackBuffers[poolIndex].resize(m_bufferingType);

        auto &backBuffers = m_product->m_pooledBackBuffers[poolIndex];

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

            // manually checking the phase type (the inheritance architecture does not allow to check in the derived
            // class)
            std::string phaseType = "RenderPhase";
            if (const RayTracePhase *product = dynamic_cast<RayTracePhase *>(m_product.get()))
                phaseType = "RayTracePhase";
            devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = VK_OBJECT_TYPE_COMMAND_BUFFER,
                .objectHandle = (uint64_t)(backBuffers[i].commandBuffer),
                .pObjectName = std::string(m_phaseName + " " + phaseType + " : " + std::to_string(i)).c_str(),
            });
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
            devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = VK_OBJECT_TYPE_SEMAPHORE,
                .objectHandle = (uint64_t)(backBuffers[i].acquireSemaphore),
                .pObjectName = std::string(m_phaseName + " acquire semaphore").c_str(),
            });

            res = vkCreateSemaphore(deviceHandle, &semaphoreCreateInfo, nullptr, &backBuffers[i].renderSemaphore);
            if (res != VK_SUCCESS)
            {
                std::cerr << "Failed to create semaphore : " << res << std::endl;
                return nullptr;
            }
            devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = VK_OBJECT_TYPE_SEMAPHORE,
                .objectHandle = (uint64_t)(backBuffers[i].renderSemaphore),
                .pObjectName = std::string(m_phaseName + " render semaphore").c_str(),
            });

            res = vkCreateFence(deviceHandle, &fenceCreateInfo, nullptr, &backBuffers[i].inFlightFence);
            if (res != VK_SUCCESS)
            {
                std::cerr << "Failed to create fence : " << res << std::endl;
                return nullptr;
            }
            devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = VK_OBJECT_TYPE_FENCE,
                .objectHandle = (uint64_t)(backBuffers[i].inFlightFence),
                .pObjectName = std::string(m_phaseName + " fence").c_str(),
            });
        }
    }

    return std::move(m_product);
}

void RenderPhase::updateSwapchainOnRenderPass(const SwapChain *newSwapchain)
{
    if (!m_renderPass.has_value())
        return;

    const std::vector<std::vector<VkImageView>> &imageViewPool = {newSwapchain->getImageViews()};
    const std::vector<VkImageView> &depthImageViewPool = {newSwapchain->getDepthImageView()};
    if (m_renderPass.value()->getHasDepthAttachment())
        m_renderPass.value()->buildFramebuffers(imageViewPool, depthImageViewPool, newSwapchain->getExtent(),
                                                m_renderPass.value()->getLayerCount(), true);
    else
        m_renderPass.value()->buildFramebuffers(imageViewPool, std::nullopt, newSwapchain->getExtent(),
                                                m_renderPass.value()->getLayerCount(), true);
}

ComputePhase::~ComputePhase()
{
    if (!m_device.lock())
        return;

    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    vkQueueWaitIdle(devicePtr->getComputeQueue());

    for (uint32_t j = 0u; j < m_backBuffers.size(); j++)
    {
        const BackBufferT &backbuffer = m_backBuffers[j];
        vkDestroyFence(deviceHandle, backbuffer.inFlightFence, nullptr);
        vkDestroySemaphore(deviceHandle, backbuffer.renderSemaphore, nullptr);
        vkDestroySemaphore(deviceHandle, backbuffer.acquireSemaphore, nullptr);
    }
}

void ComputePhase::recordBackBuffer() const
{
    ZoneScoped;

    VkFence currentFence = getCurrentFence();
    VkResult res = vkWaitForFences(m_device.lock()->getHandle(), 1, &currentFence, VK_TRUE, UINT64_MAX);
    assert(res != VK_TIMEOUT);
    vkResetFences(m_device.lock()->getHandle(), 1, &currentFence);

    const VkCommandBuffer &commandBuffer = getCurrentBackBuffer().commandBuffer;

    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo commandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = nullptr,
    };
    res = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to begin recording command buffer : " << res << std::endl;
        return;
    }

    for (int i = 0; i < m_computeStates.size(); ++i)
    {
        ComputeState *computeState = m_computeStates[i].get();

        if (const auto &pipeline = computeState->getPipeline())
        {
            pipeline->recordBind(commandBuffer, {});
        }

        computeState->updateDescriptorSetsPerFrame(nullptr, commandBuffer, m_backBufferIndex);

        computeState->updateUniformBuffers(0);
        computeState->recordBackBufferComputeCommands(commandBuffer, m_backBufferIndex);
    }
    std::vector<VkDescriptorSet> descriptorSets;

    res = vkEndCommandBuffer(commandBuffer);
    if (res != VK_SUCCESS)
        std::cerr << "Failed to record command buffer : " << res << std::endl;
}

void ComputePhase::submitBackBuffer(const VkSemaphore *waitSemaphoreOverride) const
{
    ZoneScoped;

    const BackBufferT &currentBackBuffer = getCurrentBackBuffer();
    VkSemaphore waitSemaphores[] = {waitSemaphoreOverride ? *waitSemaphoreOverride
                                                          : currentBackBuffer.acquireSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
    VkSemaphore signalSemaphores[] = {getCurrentRenderSemaphore()};
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

    VkResult res = vkQueueSubmit(m_device.lock()->getGraphicsQueue(), 1, &submitInfo, getCurrentFence());
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to submit dispatch (compute) command buffer : " << res << std::endl;
        assert(false);
        abort();
    }
}

void ComputePhase::wait() const
{
    vkQueueWaitIdle(m_device.lock()->getComputeQueue());
}

void ComputePhase::swapBackBuffers()
{
    m_backBufferIndex = (m_backBufferIndex + 1) % m_backBuffers.size();
}

std::unique_ptr<ComputePhase> ComputePhaseBuilder::build()
{
    assert(m_device.lock());

    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    // back buffers
    m_product->m_backBuffers.resize(m_bufferingType);
    for (int i = 0; i < m_bufferingType; ++i)
    {
        {
            VkCommandBufferAllocateInfo commandBufferAllocInfo = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                .commandPool = devicePtr->getCommandPool(),
                .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                .commandBufferCount = 1U,
            };
            VkResult res = vkAllocateCommandBuffers(deviceHandle, &commandBufferAllocInfo,
                                                    &m_product->m_backBuffers[i].commandBuffer);
            if (res != VK_SUCCESS)
            {
                std::cerr << "Failed to allocate command buffers : " << res << std::endl;
                return nullptr;
            }

            devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = VK_OBJECT_TYPE_COMMAND_BUFFER,
                .objectHandle = (uint64_t)(m_product->m_backBuffers[i].commandBuffer),
                .pObjectName = std::string(m_phaseName + " ComputePhase : " + std::to_string(i)).c_str(),
            });
        }

        // synchronization

        VkSemaphoreCreateInfo semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        VkFenceCreateInfo fenceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        VkResult res = vkCreateSemaphore(deviceHandle, &semaphoreCreateInfo, nullptr,
                                         &m_product->m_backBuffers[i].acquireSemaphore);
        if (res != VK_SUCCESS)
        {
            std::cerr << "Failed to create semaphore : " << res << std::endl;
            return nullptr;
        }
        devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VK_OBJECT_TYPE_SEMAPHORE,
            .objectHandle = (uint64_t)(m_product->m_backBuffers[i].acquireSemaphore),
            .pObjectName = std::string(m_phaseName + " acquire semaphore").c_str(),
        });

        res = vkCreateSemaphore(deviceHandle, &semaphoreCreateInfo, nullptr,
                                &m_product->m_backBuffers[i].renderSemaphore);
        if (res != VK_SUCCESS)
        {
            std::cerr << "Failed to create semaphore : " << res << std::endl;
            return nullptr;
        }
        devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VK_OBJECT_TYPE_SEMAPHORE,
            .objectHandle = (uint64_t)(m_product->m_backBuffers[i].renderSemaphore),
            .pObjectName = std::string(m_phaseName + " render semaphore").c_str(),
        });

        res = vkCreateFence(deviceHandle, &fenceCreateInfo, nullptr, &m_product->m_backBuffers[i].inFlightFence);
        if (res != VK_SUCCESS)
        {
            std::cerr << "Failed to create fence : " << res << std::endl;
            return nullptr;
        }
        devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VK_OBJECT_TYPE_FENCE,
            .objectHandle = (uint64_t)(m_product->m_backBuffers[i].inFlightFence),
            .pObjectName = std::string(m_phaseName + " fence").c_str(),
        });
    }

    return std::move(m_product);
}

void ComputePhase::registerComputeState(std::shared_ptr<ComputeState> state)
{
    for (int i = 0; i < m_backBuffers.size(); ++i)
    {
        state->updateDescriptorSets(nullptr, i);
    }

    m_computeStates.push_back(state);
}

void RayTracePhase::recordBackBuffer(uint32_t imageIndex, uint32_t singleFrameRenderIndex,
                                     uint32_t pooledFramebufferIndex, VkRect2D renderArea, const CameraABC &camera,
                                     const std::vector<std::shared_ptr<Light>> &lights,
                                     const std::shared_ptr<ProbeGrid> &probeGrid)
{
    ZoneScoped;

    if (singleFrameRenderIndex > 0)
    {
        VkFence currentFence = getCurrentFence(pooledFramebufferIndex);
        VkResult res = vkWaitForFences(m_device.lock()->getHandle(), 1, &currentFence, VK_TRUE, UINT64_MAX);
        assert(res != VK_TIMEOUT);
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

    const auto &renderStates = m_pooledRenderStates[pooledFramebufferIndex];
    for (int i = 0; i < renderStates.size(); ++i)
    {
        RenderStateABC *renderState = renderStates[i].get();
        renderState->updatePushConstants(commandBuffer, singleFrameRenderIndex, camera, lights);
        renderState->updateUniformBuffers(m_backBufferIndex, singleFrameRenderIndex, pooledFramebufferIndex, camera,
                                          lights, probeGrid, m_isCapturePhase);
        renderState->updateDescriptorSetsPerFrame(m_parentPhase, commandBuffer, m_backBufferIndex,
                                                  pooledFramebufferIndex);
    }

    VkRenderPassBeginInfo renderPassBeginInfo;
    if (m_renderPass.has_value())
    {
        auto &rp = m_renderPass.value();
        VkClearValue clearColor = {
            .color = {0.05f, 0.05f, 0.05f, 0.f},
        };
        VkClearValue clearDepth = {
            .depthStencil = {1.f, 0},
        };
        std::vector<VkClearValue> clearValues(rp->getColorAttachmentCount() + (int)rp->getHasDepthAttachment());
        for (int i = 0; i < clearValues.size(); ++i)
        {
            clearValues[i] = clearColor;
        }
        if (rp->getHasDepthAttachment())
            clearValues.back() = clearDepth;

        renderArea.extent.width =
            std::min(renderArea.extent.width - renderArea.offset.x, rp->getMinRenderArea().extent.width);
        renderArea.extent.height =
            std::min(renderArea.extent.height - renderArea.offset.y, rp->getMinRenderArea().extent.height);

        VkRenderPassBeginInfo renderPassBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass = rp->getHandle(),
            .framebuffer = rp->getFramebuffer(pooledFramebufferIndex, imageIndex),
            .renderArea = renderArea,
            .clearValueCount = static_cast<uint32_t>(clearValues.size()),
            .pClearValues = clearValues.data(),
        };
        vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    for (int i = 0; i < renderStates.size(); ++i)
    {
        RenderStateABC *renderState = renderStates[i].get();

        if (const auto &pipeline = renderState->getPipeline())
        {
            pipeline->recordBind(commandBuffer, renderArea);
        }

        for (uint32_t subObjectIndex = 0u; subObjectIndex < renderState->getSubObjectCount(); subObjectIndex++)
        {
            renderState->recordBackBufferDescriptorSetsCommands(commandBuffer, subObjectIndex, m_backBufferIndex,
                                                                pooledFramebufferIndex);
            renderState->recordBackBufferDrawObjectCommands(commandBuffer, subObjectIndex);
        }
    }

    if (m_renderPass.has_value())
        vkCmdEndRenderPass(commandBuffer);

    res = vkEndCommandBuffer(commandBuffer);
    if (res != VK_SUCCESS)
        std::cerr << "Failed to record command buffer : " << res << std::endl;

    if (m_renderPass.has_value())
    {
        // keep track of this newly rendered image
        m_lastFramebuffer = std::optional<VkFramebuffer>(renderPassBeginInfo.framebuffer);
        m_lastFramebufferImageResource = m_renderPass.value()->getImageResource(imageIndex);
        m_lastFramebufferImageView =
            std::optional<VkImageView>(m_renderPass.value()->getImageView(pooledFramebufferIndex, imageIndex));
    }
}

RayTracePhase::AsGeom RayTracePhase::getAsGeometry(std::shared_ptr<Mesh> mesh) const
{
    // from
    // https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/vkrt_tutorial.md.html#accelerationstructure/bottom-levelaccelerationstructure

    // BLAS builder requires raw device addresses.
    VkDeviceAddress vertexAddress = mesh->getVertexBuffer()->getDeviceAddress();
    VkDeviceAddress indexAddress = mesh->getIndexBuffer()->getDeviceAddress();

    uint32_t maxPrimitiveCount = mesh->getPrimitiveCount();

    // Describe buffer as array of VertexObj.
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT; // vec3 vertex position data.
    triangles.vertexData.deviceAddress = vertexAddress;
    triangles.vertexStride = sizeof(Vertex);
    // Describe index data (16-bit unsigned int)
    triangles.indexType = VK_INDEX_TYPE_UINT16;
    triangles.indexData.deviceAddress = indexAddress;
    // Indicate identity transform by setting transformData to null device pointer.
    // triangles.transformData = {};
    triangles.maxVertex = mesh->getVertexCount() - 1;

    // Identify the above data as containing opaque triangles.
    VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    asGeom.geometry.triangles = triangles;

    // The entire array will be used to build the BLAS.
    VkAccelerationStructureBuildRangeInfoKHR offset;
    offset.firstVertex = 0;
    offset.primitiveCount = maxPrimitiveCount;
    offset.primitiveOffset = 0;
    offset.transformOffset = 0;

    return AsGeom(asGeom, offset);
}

#ifdef USE_NV_PRO_CORE
#include "graphics/context.hpp"

std::unique_ptr<RenderPhase> RenderPhaseBuilder<RenderTypeE::RAYTRACE>::build()
{
    m_product = RenderPhaseBuilder<RenderTypeE::RASTER>::build();

    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    auto product = static_cast<RayTracePhase *>(m_product.get());

    uint32_t count = devicePtr->getContext()->getInstanceExtensionCount();
    auto reqExtensions = devicePtr->getContext()->getInstanceExtensions();

    // Requesting Vulkan extensions and layers
    nvvk::ContextCreateInfo contextInfo;
    contextInfo.setVersion(1, 3);                       // Using Vulkan 1.2
    for (uint32_t ext_id = 0; ext_id < count; ext_id++) // Adding required extensions (surface, win32, linux, ..)
        contextInfo.addInstanceExtension(reqExtensions[ext_id]);
    contextInfo.addInstanceLayer("VK_LAYER_LUNARG_monitor", true);             // FPS in titlebar
    contextInfo.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, true); // Allow debug names
    contextInfo.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);           // Enabling ability to present rendering

    // #VKRay: Activate the ray tracing extension
    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    contextInfo.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, false,
                                   &accelFeature); // To build acceleration structures
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    contextInfo.addDeviceExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, false,
                                   &rtPipelineFeature);                             // To use vkCmdTraceRaysKHR
    contextInfo.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME); // Required by ray tracing pipeline

    // Creating Vulkan base application
    product->vkctx.initInstance(contextInfo);
    // Find all compatible devices
    auto compatibleDevices = product->vkctx.getCompatibleDevices(contextInfo);
    assert(!compatibleDevices.empty());
    // Use a compatible device
    product->vkctx.initDevice(compatibleDevices[0], contextInfo);

    product->m_alloc.init(devicePtr->getContextInstance(), deviceHandle, devicePtr->getPhysicalHandle());
    product->m_rtBuilder.setup(deviceHandle, &product->m_alloc, devicePtr->getGraphicsFamilyIndex().value());

    return std::move(m_product);
}
#endif

void RayTracePhase::updateDescriptorSets()
{
}

RayTracePhase::~RayTracePhase()
{
#ifdef USE_NV_PRO_CORE
    m_rtBuilder.destroy();
    m_alloc.deinit();
#endif
}

#ifdef USE_NV_PRO_CORE
auto objectToVkGeometryKHR(const std::shared_ptr<Mesh> &mesh)
{
    // BLAS builder requires raw device addresses.
    VkDeviceAddress vertexAddress = mesh->getVertexBuffer()->getDeviceAddress();
    VkDeviceAddress indexAddress = mesh->getIndexBuffer()->getDeviceAddress();

    uint32_t maxPrimitiveCount = mesh->getPrimitiveCount();

    // Describe buffer as array of VertexObj.
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT; // vec3 vertex position data.
    triangles.vertexData.deviceAddress = vertexAddress;
    triangles.vertexStride = sizeof(Vertex);
    // Describe index data (16-bit unsigned int)
    triangles.indexType = VK_INDEX_TYPE_UINT16;
    triangles.indexData.deviceAddress = indexAddress;
    // Indicate identity transform by setting transformData to null device pointer.
    // triangles.transformData = {};
    triangles.maxVertex = mesh->getVertexCount() - 1;

    // Identify the above data as containing opaque triangles.
    VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    asGeom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    asGeom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    asGeom.geometry.triangles = triangles;

    // The entire array will be used to build the BLAS.
    VkAccelerationStructureBuildRangeInfoKHR offset;
    offset.firstVertex = 0;
    offset.primitiveCount = maxPrimitiveCount;
    offset.primitiveOffset = 0;
    offset.transformOffset = 0;

    // Our blas is made from only one geometry, but could be made of many geometries
    nvvk::RaytracingBuilderKHR::BlasInput input;
    input.asGeometry.emplace_back(asGeom);
    input.asBuildOffsetInfo.emplace_back(offset);

    return input;
}
#endif

void RayTracePhase::generateBottomLevelAS()
{
    // from
    // https://nvpro-samples.github.io/vk_raytracing_tutorial_KHR/vkrt_tutorial.md.html#accelerationstructure/bottom-levelaccelerationstructure/helperdetails:raytracingbuilder::buildblas()

#ifdef USE_NV_PRO_CORE
    // BLAS - Storing each primitive in a geometry
    std::vector<nvvk::RaytracingBuilderKHR::BlasInput> allBlas;
    for (int i = 0; i < m_pooledRenderStates[0].size(); ++i)
    {
        if (auto state = std::dynamic_pointer_cast<ModelRenderState>(m_pooledRenderStates[0][i]))
        {
            auto meshes = state->getModel()->getMeshes();
            for (int j = 0; j < meshes.size(); ++j)
            {
                auto blas = objectToVkGeometryKHR(meshes[j]);

                // We could add more geometry in each BLAS, but we add only one for now
                allBlas.emplace_back(blas);
            }
        }
        else
        {
            assert(false);
        }
    }
    m_rtBuilder.buildBlas(allBlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
#else
    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    uint32_t minAlignment =
        devicePtr->getPhysicalDeviceASProperties()
            .minAccelerationStructureScratchOffsetAlignment; /*m_rtASProperties.minAccelerationStructureScratchOffsetAlignment*/

    uint32_t nbBlas{0};
    VkDeviceSize asTotalSize{0};    // Memory size of all allocated BLAS
    uint32_t nbCompactions{0};      // Nb of BLAS requesting compaction
    VkDeviceSize maxScratchSize{0}; // Largest scratch size

    std::vector<VkAccelerationStructureBuildSizesInfoKHR> sizeInfos;
    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> geometryBuildInfos;
    std::vector<std::vector<VkAccelerationStructureGeometryKHR>> geometries;
    std::vector<std::vector<VkAccelerationStructureBuildRangeInfoKHR>> rangeInfos;
    for (int i = 0; i < m_pooledRenderStates[0].size(); ++i)
    {
        if (auto state = std::dynamic_pointer_cast<ModelRenderState>(m_pooledRenderStates[0][i]))
        {
            auto meshes = state->getModel()->getMeshes();
            geometries.push_back({});
            rangeInfos.push_back({});
            geometries.back().reserve(meshes.size());
            rangeInfos.back().reserve(meshes.size());
            std::vector<uint32_t> pMaxPrimitiveCounts(meshes.size());
            for (int j = 0; j < meshes.size(); ++j)
            {
                AsGeom g = getAsGeometry(meshes[j]);
                geometries.back().push_back(VkAccelerationStructureGeometryKHR{g.first});
                rangeInfos.back().push_back(VkAccelerationStructureBuildRangeInfoKHR{g.second});
                pMaxPrimitiveCounts[j] = g.second.primitiveCount;
            }
            ++nbBlas;

            VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
                .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
                .srcAccelerationStructure = VK_NULL_HANDLE,
                .geometryCount = static_cast<uint32_t>(geometries.back().size()),
                .pGeometries = geometries.back().data(),
                .ppGeometries = nullptr,
                .scratchData =
                    VkDeviceOrHostAddressKHR{
                        .deviceAddress = 0,
                    },
            };

            VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
            };
            devicePtr->vkGetAccelerationStructureBuildSizesKHR(deviceHandle,
                                                               VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                               &buildInfo, pMaxPrimitiveCounts.data(), &sizeInfo);
            geometryBuildInfos.push_back(buildInfo);
            sizeInfos.push_back(sizeInfo);

            maxScratchSize = alignup(std::max(maxScratchSize, sizeInfos.back().buildScratchSize), minAlignment);
            asTotalSize += maxScratchSize;
        }
        else
        {
            // only Model Render State objects should be registered in a Ray Trace Phase
            // this proves the wrong usage of this object class
            assert(false);
        }
    }

    VkDeviceSize hintMaxBudget{256'000'000}; // 256 MB

    BufferBuilder bb;
    bb.setDevice(m_device);
    bb.setName("blas scratch buffer");
    bb.setProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    bb.setUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    VkDeviceSize totalScratch{0};
    for (int i = 0; i < sizeInfos.size(); ++i)
    {
        VkDeviceSize alignedSize = alignup(sizeInfos[i].buildScratchSize, minAlignment);
        totalScratch += alignedSize;
    }
    bb.setSize(totalScratch);
    // Allocate the scratch buffers holding the temporary data of the acceleration structure builder
    // 2) allocating the scratch buffer
    std::unique_ptr<Buffer> blasScratchBuffer = bb.build();
    // 3) getting the device address for the scratch buffer
    std::vector<VkDeviceAddress> scratchAddresses;
    VkDeviceAddress scratch0 = blasScratchBuffer->getDeviceAddress();
    VkDeviceAddress address = {};
    for (int i = 0; i < sizeInfos.size(); ++i)
    {
        scratchAddresses.push_back(scratch0 + address);
        VkDeviceSize alignedSize = alignup(sizeInfos[i].buildScratchSize, minAlignment);
        address += alignedSize;
    }

    VkDeviceSize budget = 0;
    m_blas.reserve(nbBlas);
    std::vector<VkAccelerationStructureBuildRangeInfoKHR *> ppRangeInfos;
    for (int i = 0; i < nbBlas; ++i)
    {
        VkAccelerationStructureKHR as;

        bb.restart();
        bb.setDevice(m_device);
        bb.setName("blas buffer");
        bb.setProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        bb.setUsage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        bb.setSize(alignup(sizeInfos[i].accelerationStructureSize, minAlignment));
        m_blasBuffers.push_back(bb.build());

        VkAccelerationStructureCreateInfoKHR createInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = m_blasBuffers.back()->getHandle(),
            .offset = 0,
            .size = alignup(sizeInfos[i].accelerationStructureSize, minAlignment),
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        };
        VkResult res = devicePtr->vkCreateAccelerationStructureKHR(deviceHandle, &createInfo, nullptr, &as);
        geometryBuildInfos[i].dstAccelerationStructure = as;
        geometryBuildInfos[i].scratchData = VkDeviceOrHostAddressKHR{
            .deviceAddress = scratchAddresses[i],
        };
        if (res != VK_SUCCESS)
        {
            std::cerr << "Failed to create Bottom-Level acceleration structure : " << res << std::endl;
            return;
        }
        m_blas.push_back(as);

        ppRangeInfos.push_back(rangeInfos[i].data());

        budget += sizeInfos[i].accelerationStructureSize;
        if (budget >= hintMaxBudget)
            break;
    }

    VkCommandBuffer cmd = devicePtr->cmdBeginOneTimeSubmit("Bottom Level Acceleration Structure build");

    devicePtr->vkCmdBuildAccelerationStructuresKHR(cmd, static_cast<uint32_t>(geometryBuildInfos.size()),
                                                   geometryBuildInfos.data(), ppRangeInfos.data());

    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0,
                         nullptr);

    devicePtr->cmdEndOneTimeSubmit(cmd);
#endif
}

void RayTracePhase::generateTopLevelAS()
{
#ifdef USE_NV_PRO_CORE
    std::vector<VkAccelerationStructureInstanceKHR> tlas;
    int offset = 0;
    for (int i = 0; i < m_pooledRenderStates[0].size(); ++i)
    {
        if (auto state = std::dynamic_pointer_cast<ModelRenderState>(m_pooledRenderStates[0][i]))
        {
            auto meshes = state->getModel()->getMeshes();
            for (int j = 0; j < meshes.size(); ++j)
            {
                int blasIndex = j + offset;
                VkAccelerationStructureInstanceKHR rayInst{};
                rayInst.transform = nvvk::toTransformMatrixKHR(
                    state->getModel()->getTransform().getTransformMatrix()); // Position of the instance
                rayInst.instanceCustomIndex = blasIndex;                     // gl_InstanceCustomIndexEXT
                rayInst.accelerationStructureReference = m_rtBuilder.getBlasDeviceAddress(blasIndex);
                rayInst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                rayInst.mask = 0xFF;                                //  Only be hit if rayMask & instance.mask != 0
                rayInst.instanceShaderBindingTableRecordOffset = 0; // We will use the same hit group for all objects
                tlas.emplace_back(rayInst);
            }
            offset += meshes.size();
        }
        else
        {
            assert(false);
        }
    }
    m_rtBuilder.buildTlas(tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
#else
    // TODO : fix the custom implementation of the top level acceleration structures
    
    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    // from https://web.engr.oregonstate.edu/~mjb/vulkan/Handouts/AccelerationStructures.2pp.pdf

    std::vector<VkAccelerationStructureInstanceKHR> vasis;
    std::vector<VkAccelerationStructureBuildSizesInfoKHR> sizeInfos;
    std::vector<std::vector<VkAccelerationStructureGeometryKHR>> geometries;
    std::vector<std::vector<VkAccelerationStructureBuildRangeInfoKHR>> rangeInfos;
    std::vector<VkAccelerationStructureBuildGeometryInfoKHR> geometryBuildInfos;
    for (int i = 0; i < m_pooledRenderStates[0].size(); ++i)
    {
        if (auto state = std::dynamic_pointer_cast<ModelRenderState>(m_pooledRenderStates[0][i]))
        {
            auto meshes = state->getModel()->getMeshes();
            geometries.push_back({});
            rangeInfos.push_back({});
            geometries.back().reserve(meshes.size());
            rangeInfos.back().reserve(meshes.size());
            std::vector<uint32_t> pMaxInstanceCount(m_pooledRenderStates[0].size());
            for (int j = 0; j < meshes.size(); ++j)
            {
                AsGeom g = getAsGeometry(meshes[j]);
                geometries.back().push_back(VkAccelerationStructureGeometryKHR{g.first});
                geometries.back().back().geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
            }
            rangeInfos.back().push_back(VkAccelerationStructureBuildRangeInfoKHR{
                .primitiveCount = 1,
                .primitiveOffset = 0,
                .firstVertex = 0,
                .transformOffset = 0,
            });

            // each mesh represent one instance (for now)
            pMaxInstanceCount[i] = 1;

            VkAccelerationStructureDeviceAddressInfoKHR vasdai = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
                .accelerationStructure = m_blas[i],
            };

            auto trs = state->getModel()->getTransform().getTransformMatrix();
            trs = glm::transpose(trs);
            VkTransformMatrixKHR matrix;
            memcpy(&matrix, &trs, sizeof(VkTransformMatrixKHR));
            VkAccelerationStructureInstanceKHR vasi = {
                .transform = matrix,
                .mask = 0xff,
                .instanceShaderBindingTableRecordOffset = 0,
                .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
                .accelerationStructureReference =
                    devicePtr->vkGetAccelerationStructureDeviceAddressKHR(deviceHandle, &vasdai),
            };
            vasis.push_back(vasi);

            std::vector<VkAccelerationStructureGeometryKHR *> pgeom(geometries.size());
            for (int j = 0; j < geometries.size(); ++j)
            {
                pgeom.push_back(geometries[j].data());
            }
            VkAccelerationStructureGeometryKHR **ppgeom = pgeom.data();
            VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
                .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
                .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
                .srcAccelerationStructure = VK_NULL_HANDLE,
                .geometryCount = 1,
                .pGeometries = nullptr,
                .ppGeometries = ppgeom,
                .scratchData =
                    VkDeviceOrHostAddressKHR{
                        .deviceAddress = 0,
                    },
            };
            geometryBuildInfos.push_back(buildInfo);

            VkAccelerationStructureGeometryInstancesDataKHR vasgid = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .arrayOfPointers = VK_FALSE,
                .data =
                    VkDeviceOrHostAddressConstKHR{
                        .hostAddress = &vasis.back(),
                    },
            };

            VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
            };
            devicePtr->vkGetAccelerationStructureBuildSizesKHR(deviceHandle,
                                                               VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                               &buildInfo, pMaxInstanceCount.data(), &sizeInfo);
            sizeInfos.push_back(sizeInfo);

            VkAccelerationStructureGeometryKHR vasg = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
                .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
                .geometry =
                    VkAccelerationStructureGeometryDataKHR{
                        .instances = vasgid,
                    },
            };
        }
        else
        {
            assert(false);
        }
    }

    VkDeviceSize hintMaxBudget{256'000'000}; // 256 MB

    uint32_t minAlignment =
        devicePtr->getPhysicalDeviceASProperties()
            .minAccelerationStructureScratchOffsetAlignment; /*m_rtASProperties.minAccelerationStructureScratchOffsetAlignment*/

    BufferBuilder bb;
    bb.setDevice(m_device);
    bb.setName("tlas scratch buffer");
    bb.setProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    bb.setUsage(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    VkDeviceSize totalScratch{0};
    for (int i = 0; i < sizeInfos.size(); ++i)
    {
        VkDeviceSize alignedSize = alignup(sizeInfos[i].buildScratchSize, minAlignment);
        totalScratch += alignedSize;
    }
    bb.setSize(totalScratch);
    // Allocate the scratch buffers holding the temporary data of the acceleration structure builder
    // 2) allocating the scratch buffer
    std::unique_ptr<Buffer> blasScratchBuffer = bb.build();
    // 3) getting the device address for the scratch buffer
    std::vector<VkDeviceAddress> scratchAddresses;
    VkDeviceAddress scratch0 = blasScratchBuffer->getDeviceAddress();
    VkDeviceAddress address = {};
    for (int i = 0; i < sizeInfos.size(); ++i)
    {
        scratchAddresses.push_back(scratch0 + address);
        VkDeviceSize alignedSize = alignup(sizeInfos[i].buildScratchSize, minAlignment);
        address += alignedSize;
    }

    VkDeviceSize budget = 0;
    m_tlas.reserve(sizeInfos.size());
    std::vector<VkAccelerationStructureBuildRangeInfoKHR *> ppRangeInfos;
    for (int i = 0; i < sizeInfos.size(); ++i)
    {
        VkAccelerationStructureKHR tlas;

        bb.restart();
        bb.setDevice(m_device);
        bb.setName("tlas buffer");
        bb.setProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        bb.setUsage(VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
        bb.setSize(alignup(sizeInfos[i].accelerationStructureSize, minAlignment));
        m_tlasBuffers.push_back(bb.build());

        VkAccelerationStructureCreateInfoKHR vasci = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = m_tlasBuffers.back()->getHandle(),
            .offset = 0,
            .size = sizeInfos[i].accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        };
        VkResult res = devicePtr->vkCreateAccelerationStructureKHR(deviceHandle, &vasci, nullptr, &tlas);
        geometryBuildInfos[i].dstAccelerationStructure = tlas;
        geometryBuildInfos[i].scratchData = VkDeviceOrHostAddressKHR{
            .deviceAddress = scratchAddresses[i],
        };
        if (res != VK_SUCCESS)
        {
            std::cerr << "Failed to create Top-Level acceleration structure : " << res << std::endl;
            return;
        }
        m_tlas.push_back(tlas);

        ppRangeInfos.push_back(rangeInfos[i].data());

        budget += sizeInfos[i].accelerationStructureSize;
        if (budget >= hintMaxBudget)
            break;
    }

    VkCommandBuffer cmd = devicePtr->cmdBeginOneTimeSubmit("Top Level Acceleration Structure build");

    devicePtr->vkCmdBuildAccelerationStructuresKHR(cmd, static_cast<uint32_t>(geometryBuildInfos.size()),
                                                   geometryBuildInfos.data(), ppRangeInfos.data());

    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0,
                         nullptr);

    devicePtr->cmdEndOneTimeSubmit(cmd);
#endif
}
