#include <glm/glm.hpp>
#include <iostream>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

#include "engine/camera.hpp"
#include "engine/uniform.hpp"
#include "graphics/buffer.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/render_pass.hpp"
#include "light.hpp"
#include "mesh.hpp"
#include "render_phase.hpp"
#include "skybox.hpp"
#include "texture.hpp"

#include "render_state.hpp"

RenderStateABC::~RenderStateABC()
{
    if (!m_device.lock())
        return;

    vkDestroyDescriptorPool(m_device.lock()->getHandle(), m_descriptorPool, nullptr);

    m_pipeline.reset();
}

void RenderStateABC::updateUniformBuffers(uint32_t backBufferIndex, uint32_t singleFrameRenderIndex, const CameraABC &camera,
                                          const std::vector<std::shared_ptr<Light>> &lights)
{
    if (m_mvpUniformBuffersMapped.size() > 0)
    {
        MVP* mvpData = static_cast<MVP*>(m_mvpUniformBuffersMapped[backBufferIndex]);
        mvpData->proj = camera.getProjectionMatrix();
        mvpData->model = glm::identity<glm::mat4>();
        mvpData->view = camera.getViewMatrix();
    }

    PointLightContainer* pointLightContainer = nullptr;
    if (m_pointLightStorageBuffersMapped.size() > 0)
        pointLightContainer = static_cast<PointLightContainer*>(m_pointLightStorageBuffersMapped[backBufferIndex]);

    DirectionalLightContainer* directionalLightContainer = nullptr;
    if (m_directionalLightStorageBuffersMapped.size() > 0)
        directionalLightContainer = static_cast<DirectionalLightContainer*>(m_directionalLightStorageBuffersMapped[backBufferIndex]);

    int pointLightCount = 0;
    int directionalLightCount = 0;
    for (int i = 0; i < lights.size(); i++)
    {
        const Light *light = lights[i].get();
        if (pointLightContainer)
        {
            if (const PointLight* pointLight = dynamic_cast<const PointLight*>(light))
            {
                PointLightContainer::PointLight pointLightData{
                    .diffuseColor = pointLight->diffuseColor,
                    .diffusePower = pointLight->diffusePower,
                    .specularColor = pointLight->specularColor,
                    .specularPower = pointLight->specularPower,
                    .position = pointLight->position,
                };

                pointLightContainer->pointLights[pointLightCount] = pointLightData;
                pointLightCount++;
                continue;
            }
        }

        if (pointLightContainer)
        {
            if (const DirectionalLight* directionalLight = dynamic_cast<const DirectionalLight*>(light))
            {
                DirectionalLightContainer::DirectionalLight directionalLightData{
                    .diffuseColor = directionalLight->diffuseColor,
                    .diffusePower = directionalLight->diffusePower,
                    .specularColor = directionalLight->specularColor,
                    .specularPower = directionalLight->specularPower,
                    .direction = directionalLight->direction,
                };

                directionalLightContainer->directionalLights[directionalLightCount] = directionalLightData;
                directionalLightCount++;
                continue;
            }
        }
    }

    if (pointLightContainer)
        pointLightContainer->pointLightCount = pointLightCount;

    if (directionalLightCount)
        directionalLightContainer->directionalLightCount = directionalLightCount;
}

void RenderStateABC::updateDescriptorSetsPerFrame(const RenderPhase *parentPhase, uint32_t imageIndex)
{
    if (!m_descriptorSetUpdatePredPerFrame)
        return;

    for (const auto &set : m_descriptorSets)
    {
        m_descriptorSetUpdatePredPerFrame(parentPhase, imageIndex, set);
    }
}

void RenderStateABC::updateDescriptorSets(const RenderPhase *parentPhase, uint32_t imageIndex)
{
    if (!m_descriptorSetUpdatePred)
        return;

    for (const auto &set : m_descriptorSets)
    {
        m_descriptorSetUpdatePred(parentPhase, imageIndex, set);
    }
}

void RenderStateABC::recordBackBufferDescriptorSetsCommands(const VkCommandBuffer &commandBuffer,
                                                            uint32_t backBufferIndex)
{
    if (m_descriptorSets.size() == 0)
        return;

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getPipelineLayout(), 0, 1,
                            &m_descriptorSets[backBufferIndex], 0, nullptr);
}

void MeshRenderStateBuilder::setPipeline(std::shared_ptr<Pipeline> pipeline)
{
    m_product->m_pipeline = pipeline;
}
void MeshRenderStateBuilder::addPoolSize(VkDescriptorType poolSizeType)
{
    m_poolSizes.push_back(VkDescriptorPoolSize{
        .type = poolSizeType,
        .descriptorCount = m_frameInFlightCount,
    });
}

std::unique_ptr<RenderStateABC> MeshRenderStateBuilder::build()
{
    assert(m_device.lock());

    auto deviceHandle = m_device.lock()->getHandle();

    // descriptor pool
    VkDescriptorPoolCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = m_frameInFlightCount,
        .poolSizeCount = static_cast<uint32_t>(m_poolSizes.size()),
        .pPoolSizes = m_poolSizes.data(),
    };
    VkResult res = vkCreateDescriptorPool(deviceHandle, &createInfo, nullptr, &m_product->m_descriptorPool);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create descriptor pool : " << res << std::endl;
        return nullptr;
    }

    // descriptor set
    std::vector<VkDescriptorSetLayout> setLayouts(m_frameInFlightCount,
                                                  m_product->m_pipeline->getDescriptorSetLayout());
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_product->m_descriptorPool,
        .descriptorSetCount = m_frameInFlightCount,
        .pSetLayouts = setLayouts.data(),
    };
    m_product->m_descriptorSets.resize(m_frameInFlightCount);
    res = vkAllocateDescriptorSets(deviceHandle, &descriptorSetAllocInfo, m_product->m_descriptorSets.data());
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to allocate descriptor sets : " << res << std::endl;
        return nullptr;
    }

    // uniform buffers

    if (m_mvpDescriptorEnable)
    {
        m_product->m_mvpUniformBuffers.resize(m_frameInFlightCount);
        m_product->m_mvpUniformBuffersMapped.resize(m_frameInFlightCount);
        for (int i = 0; i < m_product->m_mvpUniformBuffers.size(); ++i)
        {
            BufferBuilder bb;
            BufferDirector bd;
            bd.createUniformBufferBuilder(bb);
            bb.setSize(sizeof(RenderStateABC::MVP));
            bb.setDevice(m_device);
            m_product->m_mvpUniformBuffers[i] = bb.build();

            vkMapMemory(deviceHandle, m_product->m_mvpUniformBuffers[i]->getMemory(), 0, sizeof(RenderStateABC::MVP), 0,
                        &m_product->m_mvpUniformBuffersMapped[i]);
        }
    }

    if (m_lightDescriptorEnable)
    {
        m_product->m_pointLightStorageBuffers.resize(m_frameInFlightCount);
        m_product->m_pointLightStorageBuffersMapped.resize(m_frameInFlightCount);
        for (int i = 0; i < m_product->m_pointLightStorageBuffers.size(); ++i)
        {
            BufferBuilder bb;
            BufferDirector bd;
            bd.createStorageBufferBuilder(bb);
            bb.setSize(sizeof(RenderStateABC::PointLightContainer));
            bb.setDevice(m_device);
            m_product->m_pointLightStorageBuffers[i] = bb.build();

            vkMapMemory(deviceHandle, m_product->m_pointLightStorageBuffers[i]->getMemory(), 0,
                        sizeof(RenderStateABC::PointLightContainer), 0,
                        &m_product->m_pointLightStorageBuffersMapped[i]);
        }

        m_product->m_directionalLightStorageBuffers.resize(m_frameInFlightCount);
        m_product->m_directionalLightStorageBuffersMapped.resize(m_frameInFlightCount);
        for (int i = 0; i < m_product->m_directionalLightStorageBuffers.size(); ++i)
        {
            BufferBuilder bb;
            BufferDirector bd;
            bd.createStorageBufferBuilder(bb);
            bb.setSize(sizeof(RenderStateABC::DirectionalLightContainer));
            bb.setDevice(m_device);
            m_product->m_directionalLightStorageBuffers[i] = bb.build();

            vkMapMemory(deviceHandle, m_product->m_directionalLightStorageBuffers[i]->getMemory(), 0,
                        sizeof(RenderStateABC::DirectionalLightContainer), 0,
                        &m_product->m_directionalLightStorageBuffersMapped[i]);
        }
    }

    for (int i = 0; i < m_product->m_descriptorSets.size(); ++i)
    {
        VkDescriptorBufferInfo mvpBufferInfo;
        UniformDescriptorBuilder udb;
        if (m_mvpDescriptorEnable)
        {
            mvpBufferInfo.buffer = m_product->m_mvpUniformBuffers[i]->getHandle();
            mvpBufferInfo.offset = 0;
            mvpBufferInfo.range = sizeof(RenderStateABC::MVP);
            udb.addSetWrites(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_product->m_descriptorSets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &mvpBufferInfo,
            });
        }

        VkDescriptorImageInfo diffuseImageInfo = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        if (m_textureDescriptorEnable)
        {
	        if (!m_texture.expired())
	        {
	            auto texPtr = m_texture.lock();
	            diffuseImageInfo.sampler = *texPtr->getSampler();
	            diffuseImageInfo.imageView = texPtr->getImageView();

                udb.addSetWrites(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = m_product->m_descriptorSets[i],
                    .dstBinding = 1,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &diffuseImageInfo,
                });
	        }
    	}
    	
    	VkDescriptorBufferInfo pointLightBufferInfo;
        VkDescriptorBufferInfo directionalLightBufferInfo;
        if (m_lightDescriptorEnable)
        {
            pointLightBufferInfo.buffer = m_product->m_pointLightStorageBuffers[i]->getHandle();
            pointLightBufferInfo.offset = 0;
            pointLightBufferInfo.range = sizeof(RenderStateABC::PointLightContainer);
            udb.addSetWrites(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_product->m_descriptorSets[i],
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &pointLightBufferInfo,
            });
            
			directionalLightBufferInfo.buffer = m_product->m_directionalLightStorageBuffers[i]->getHandle();
            directionalLightBufferInfo.offset = 0;
            directionalLightBufferInfo.range = sizeof(RenderStateABC::DirectionalLightContainer);
            udb.addSetWrites(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_product->m_descriptorSets[i],
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &directionalLightBufferInfo,
            });
        }
        VkDescriptorImageInfo envMapImageInfo = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        if (!m_environmentMap.expired())
        {
            auto texPtr = m_environmentMap.lock();
            envMapImageInfo.sampler = *texPtr->getSampler();
            envMapImageInfo.imageView = texPtr->getImageView();

            udb.addSetWrites(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_product->m_descriptorSets[i],
                .dstBinding = 4,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &envMapImageInfo,
            });
        }

        std::vector<VkWriteDescriptorSet> writes = udb.buildAndRestart()->getSetWrites();
        if (!writes.empty())
            vkUpdateDescriptorSets(deviceHandle, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    return std::move(m_product);
}

void MeshRenderState::updatePushConstants(const VkCommandBuffer& commandBuffer, uint32_t imageIndex, uint32_t singleFrameRenderCount, const CameraABC& camera, const std::vector<std::shared_ptr<Light>>& lights)
{
    if (m_pushViewPosition)
    {
        const Transform& cameraTransform = camera.getTransform();
        float data[3] = {
            cameraTransform.position.x,
            cameraTransform.position.y,
            cameraTransform.position.z
        };

        uint32_t offset = 0;
        uint32_t size = 16;

        vkCmdPushConstants(commandBuffer, m_pipeline->getPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, offset, size, data);
    }
}

void MeshRenderState::recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer)
{
    auto meshPtr = m_mesh.lock();

    VkBuffer vbos[] = {meshPtr->getVertexBufferHandle()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vbos, offsets);
    vkCmdBindIndexBuffer(commandBuffer, meshPtr->getIndexBufferHandle(), 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(commandBuffer, meshPtr->getIndexCount(), 1, 0, 0, 0);
}

std::unique_ptr<RenderStateABC> ImGuiRenderStateBuilder::build()
{
    assert(m_device.lock());

    auto deviceHandle = m_device.lock()->getHandle();

    // descriptor pool
    VkDescriptorPoolCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(m_poolSizes.size()),
        .pPoolSizes = m_poolSizes.data(),
    };

    VkResult res = vkCreateDescriptorPool(deviceHandle, &createInfo, nullptr, &m_product->m_descriptorPool);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create descriptor pool : " << res << std::endl;
        return nullptr;
    }

    auto result = std::move(m_product);
    return result;
}

void ImGuiRenderStateBuilder::setPipeline(std::shared_ptr<Pipeline> pipeline)
{
    m_product->m_pipeline = pipeline;
}
void ImGuiRenderStateBuilder::addPoolSize(VkDescriptorType poolSizeType)
{
    m_poolSizes.push_back(VkDescriptorPoolSize{
        .type = poolSizeType,
        .descriptorCount = 1,
    });
}

void ImGuiRenderState::recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer)
{
    ImGui::Render();
    ImDrawData *draw_data = ImGui::GetDrawData();

    ImGui_ImplVulkan_RenderDrawData(draw_data, commandBuffer);
}

void SkyboxRenderStateBuilder::setPipeline(std::shared_ptr<Pipeline> pipeline)
{
    m_product->m_pipeline = pipeline;
}
void SkyboxRenderStateBuilder::addPoolSize(VkDescriptorType poolSizeType)
{
    m_poolSizes.push_back(VkDescriptorPoolSize{
        .type = poolSizeType,
        .descriptorCount = m_frameInFlightCount,
    });
}

std::unique_ptr<RenderStateABC> SkyboxRenderStateBuilder::build()
{
    assert(m_device.lock());

    auto deviceHandle = m_device.lock()->getHandle();

    // descriptor pool
    VkDescriptorPoolCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = m_frameInFlightCount,
        .poolSizeCount = static_cast<uint32_t>(m_poolSizes.size()),
        .pPoolSizes = m_poolSizes.data(),
    };
    VkResult res = vkCreateDescriptorPool(deviceHandle, &createInfo, nullptr, &m_product->m_descriptorPool);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create descriptor pool : " << res << std::endl;
        return nullptr;
    }

    // descriptor set
    std::vector<VkDescriptorSetLayout> setLayouts(m_frameInFlightCount,
                                                  m_product->m_pipeline->getDescriptorSetLayout());
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_product->m_descriptorPool,
        .descriptorSetCount = m_frameInFlightCount,
        .pSetLayouts = setLayouts.data(),
    };
    m_product->m_descriptorSets.resize(m_frameInFlightCount);
    res = vkAllocateDescriptorSets(deviceHandle, &descriptorSetAllocInfo, m_product->m_descriptorSets.data());
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to allocate descriptor sets : " << res << std::endl;
        return nullptr;
    }

    // uniform buffers

    m_product->m_mvpUniformBuffers.resize(m_frameInFlightCount);
    m_product->m_mvpUniformBuffersMapped.resize(m_frameInFlightCount);
    for (int i = 0; i < m_product->m_mvpUniformBuffers.size(); ++i)
    {
        BufferBuilder bb;
        BufferDirector bd;
        bd.createUniformBufferBuilder(bb);
        bb.setSize(sizeof(RenderStateABC::MVP));
        bb.setDevice(m_device);
        m_product->m_mvpUniformBuffers[i] = bb.build();

        vkMapMemory(deviceHandle, m_product->m_mvpUniformBuffers[i]->getMemory(), 0, sizeof(RenderStateABC::MVP), 0,
                    &m_product->m_mvpUniformBuffersMapped[i]);
    }

    for (int i = 0; i < m_product->m_descriptorSets.size(); ++i)
    {
        VkDescriptorBufferInfo mvpBufferInfo = {
            .buffer = m_product->m_mvpUniformBuffers[i]->getHandle(),
            .offset = 0,
            .range = sizeof(RenderStateABC::MVP),
        };

        UniformDescriptorBuilder udb;
        udb.addSetWrites(VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_product->m_descriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &mvpBufferInfo,
        });

        if (m_textureDescriptorEnable)
        {
            VkDescriptorImageInfo imageInfo = {
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            if (m_texture.lock())
            {
                auto texPtr = m_texture.lock();
                imageInfo.sampler = *texPtr->getSampler();
                imageInfo.imageView = texPtr->getImageView();
            }
            udb.addSetWrites(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_product->m_descriptorSets[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &imageInfo,
            });
        }

        std::vector<VkWriteDescriptorSet> writes = udb.buildAndRestart()->getSetWrites();
        vkUpdateDescriptorSets(deviceHandle, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    return std::move(m_product);
}

void SkyboxRenderState::updateUniformBuffers(uint32_t imageIndex, uint32_t singleFrameRenderIndex, const CameraABC &camera,
                                             const std::vector<std::shared_ptr<Light>> &lights)
{
    MVP *mvpData = static_cast<MVP *>(m_mvpUniformBuffersMapped[imageIndex]);
    mvpData->proj = camera.getProjectionMatrix();
    mvpData->model = glm::identity<glm::mat4>();
    mvpData->view = camera.getViewMatrix();
}

void SkyboxRenderState::recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer)
{
    auto skyboxPtr = m_skybox.lock();

    VkBuffer vbos[] = {skyboxPtr->getVertexBufferHandle()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vbos, offsets);
    vkCmdDraw(commandBuffer, skyboxPtr->getVertexCount(), 1, 0, 0);
}


void EnvironmentCaptureRenderStateBuilder::setPipeline(std::shared_ptr<Pipeline> pipeline)
{
    m_product->m_pipeline = pipeline;
}
void EnvironmentCaptureRenderStateBuilder::addPoolSize(VkDescriptorType poolSizeType)
{
    m_poolSizes.push_back(VkDescriptorPoolSize{
        .type = poolSizeType,
        .descriptorCount = m_frameInFlightCount,
        });
}

std::unique_ptr<RenderStateABC> EnvironmentCaptureRenderStateBuilder::build()
{
    assert(m_device.lock());

    auto deviceHandle = m_device.lock()->getHandle();

    // descriptor pool
    VkDescriptorPoolCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = m_frameInFlightCount,
        .poolSizeCount = static_cast<uint32_t>(m_poolSizes.size()),
        .pPoolSizes = m_poolSizes.data(),
    };
    VkResult res = vkCreateDescriptorPool(deviceHandle, &createInfo, nullptr, &m_product->m_descriptorPool);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create descriptor pool : " << res << std::endl;
        return nullptr;
    }

    // descriptor set
    std::vector<VkDescriptorSetLayout> setLayouts(m_frameInFlightCount,
        m_product->m_pipeline->getDescriptorSetLayout());
    VkDescriptorSetAllocateInfo descriptorSetAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = m_product->m_descriptorPool,
        .descriptorSetCount = m_frameInFlightCount,
        .pSetLayouts = setLayouts.data(),
    };
    m_product->m_descriptorSets.resize(m_frameInFlightCount);
    res = vkAllocateDescriptorSets(deviceHandle, &descriptorSetAllocInfo, m_product->m_descriptorSets.data());
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to allocate descriptor sets : " << res << std::endl;
        return nullptr;
    }

    // uniform buffers

    m_product->m_mvpUniformBuffers.resize(m_frameInFlightCount);
    m_product->m_mvpUniformBuffersMapped.resize(m_frameInFlightCount);
    for (int i = 0; i < m_product->m_mvpUniformBuffers.size(); ++i)
    {
        BufferBuilder bb;
        BufferDirector bd;
        bd.createUniformBufferBuilder(bb);
        bb.setSize(sizeof(RenderStateABC::MVP));
        bb.setDevice(m_device);
        m_product->m_mvpUniformBuffers[i] = bb.build();

        vkMapMemory(deviceHandle, m_product->m_mvpUniformBuffers[i]->getMemory(), 0, sizeof(RenderStateABC::MVP), 0,
            &m_product->m_mvpUniformBuffersMapped[i]);
    }

    for (int i = 0; i < m_product->m_descriptorSets.size(); ++i)
    {
        VkDescriptorBufferInfo mvpBufferInfo = {
            .buffer = m_product->m_mvpUniformBuffers[i]->getHandle(),
            .offset = 0,
            .range = sizeof(RenderStateABC::MVP),
        };

        UniformDescriptorBuilder udb;
        udb.addSetWrites(VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_product->m_descriptorSets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &mvpBufferInfo,
            });

        if (m_textureDescriptorEnable)
        {
            VkDescriptorImageInfo imageInfo = {
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            };
            if (m_texture.lock())
            {
                auto texPtr = m_texture.lock();
                imageInfo.sampler = *texPtr->getSampler();
                imageInfo.imageView = texPtr->getImageView();
            }
            udb.addSetWrites(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_product->m_descriptorSets[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &imageInfo,
                });
        }

        std::vector<VkWriteDescriptorSet> writes = udb.buildAndRestart()->getSetWrites();
        vkUpdateDescriptorSets(deviceHandle, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    return std::move(m_product);
}

void EnvironmentCaptureRenderState::updateUniformBuffers(uint32_t imageIndex, uint32_t singleFrameRenderIndex, const CameraABC& camera,
    const std::vector<std::shared_ptr<Light>>& lights)
{
    glm::mat4 captureViews[] =
    {
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
       glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
    };

    MVP* mvpData = static_cast<MVP*>(m_mvpUniformBuffersMapped[imageIndex]);
    mvpData->proj = camera.getProjectionMatrix();
    mvpData->model = glm::identity<glm::mat4>();
    mvpData->view = captureViews[singleFrameRenderIndex];
}

void EnvironmentCaptureRenderState::recordBackBufferDrawObjectCommands(const VkCommandBuffer& commandBuffer)
{
    auto skyboxPtr = m_skybox.lock();

    VkBuffer vbos[] = { skyboxPtr->getVertexBufferHandle() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vbos, offsets);
    vkCmdDraw(commandBuffer, skyboxPtr->getVertexCount(), 1, 0, 0);
}
