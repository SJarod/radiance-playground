#include <iostream>

#include <glm/gtc/matrix_transform.hpp>
#include <tracy/Tracy.hpp>

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
        std::cerr << "Failed to submit draw command buffer : " << res << std::endl;
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

            devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
                .objectType = VK_OBJECT_TYPE_COMMAND_BUFFER,
                .objectHandle = (uint64_t)(backBuffers[i].commandBuffer),
                .pObjectName = std::string(m_phaseName + " RenderPhase : " + std::to_string(i)).c_str(),
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
    m_renderPass.value()->buildFramebuffers(imageViewPool, depthImageViewPool, newSwapchain->getExtent(), true);
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
        std::cerr << "Failed to submit draw command buffer : " << res << std::endl;
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
        VkClearValue clearColor = {
            .color = {0.05f, 0.05f, 0.05f, 0.f},
        };
        VkClearValue clearDepth = {
            .depthStencil = {1.f, 0},
        };
        std::array<VkClearValue, 2> clearValues = {clearColor, clearDepth};

        renderArea.extent.width = std::min(renderArea.extent.width - renderArea.offset.x,
                                           m_renderPass.value()->getMinRenderArea().extent.width);
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
    // Describe index data (32-bit unsigned int)
    triangles.indexType = VK_INDEX_TYPE_UINT32;
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

void RayTracePhase::generateBottomLevelAS()
{
    // uint32_t blasCount = m_pooledRenderStates[0].size();

    // for (int i = 0; i <)

    //     VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
    //         .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
    //         .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    //         .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
    //         .dstAccelerationStructure = rtproduct->m_blas,
    //         .geometryCount = 1,
    //     };
    // VkAccelerationStructureBuildSizesInfoKHR sizeInfo;
    // vkGetAccelerationStructureBuildSizesKHR(deviceHandle, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
    // &buildInfo,
    //                                         &buildInfo.geometryCount, &sizeInfo);
    // VkAccelerationStructureCreateInfoKHR createInfo = {
    //     .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
    //     .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
    // };
    // vkCreateAccelerationStructureKHR(deviceHandle, &createInfo, nullptr, &rtproduct->m_blas);
}

void RayTracePhase::generateTopLevelAS()
{
}
