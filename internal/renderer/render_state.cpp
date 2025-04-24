#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"

#include "engine/camera.hpp"
#include "engine/uniform.hpp"
#include "engine/probe_grid.hpp"
#include "graphics/buffer.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/render_pass.hpp"
#include "light.hpp"
#include "mesh.hpp"
#include "render_phase.hpp"
#include "model.hpp"
#include "skybox.hpp"
#include "texture.hpp"

#include "render_state.hpp"

std::shared_ptr<Texture> ModelRenderState::s_defaultDiffuseTexture;

const glm::vec3 captureViewCenter[] =
{
   glm::vec3(-1.f, 0.f, 0.f),
   glm::vec3( 1.f, 0.f, 0.f),
   glm::vec3( 0.f, 1.f, 0.f),
   glm::vec3( 0.f,-1.f, 0.f),
   glm::vec3( 0.f, 0.f, 1.f),
   glm::vec3( 0.f, 0.f,-1.f),
};

const glm::vec3 captureViewUp[] =
{
   glm::vec3(0.f, 1.f, 0.f),
   glm::vec3(0.f, 1.f, 0.f),
   glm::vec3(0.f, 0.f,-1.f),
   glm::vec3(0.f, 0.f, 1.f),
   glm::vec3(0.f, 1.f, 0.f),
   glm::vec3(0.f, 1.f, 0.f)
};

const glm::mat4 captureViews[] =
{
   glm::lookAt(glm::vec3(0.f, 0.f, 0.f), captureViewCenter[0], captureViewUp[0]),
   glm::lookAt(glm::vec3(0.f, 0.f, 0.f), captureViewCenter[1], captureViewUp[1]),
   glm::lookAt(glm::vec3(0.f, 0.f, 0.f), captureViewCenter[2], captureViewUp[2]),
   glm::lookAt(glm::vec3(0.f, 0.f, 0.f), captureViewCenter[3], captureViewUp[3]),
   glm::lookAt(glm::vec3(0.f, 0.f, 0.f), captureViewCenter[4], captureViewUp[4]),
   glm::lookAt(glm::vec3(0.f, 0.f, 0.f), captureViewCenter[5], captureViewUp[5]),
};


const glm::mat4 capturePartialProj = glm::perspective(glm::half_pi<float>(), 1.0f, 0.1f, 1000.f);

RenderStateABC::~RenderStateABC()
{
    if (!m_device.lock())
        return;

    vkDestroyDescriptorPool(m_device.lock()->getHandle(), m_descriptorPool, nullptr);

    m_pipeline.reset();
}

void RenderStateABC::updateUniformBuffers(uint32_t backBufferIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex, const CameraABC &camera,
                                          const std::vector<std::shared_ptr<Light>> &lights, const std::unique_ptr<ProbeGrid> &probeGrid, bool captureModeEnabled)
{
    if (m_mvpUniformBuffersMapped.size() > 0)
    {
        MVP* mvpData = static_cast<MVP*>(m_mvpUniformBuffersMapped[backBufferIndex]);
        mvpData->model = glm::identity<glm::mat4>();

        if (!captureModeEnabled)
        {
            mvpData->proj = camera.getProjectionMatrix();
            mvpData->views[0] = camera.getViewMatrix();
        }
        else 
        {
            const glm::vec3& probePosition = probeGrid->getProbeAtIndex(pooledFramebufferIndex)->position;

            mvpData->proj = capturePartialProj;
            mvpData->proj[1][1] *= -1;

            for (int i = 0; i < 6; i++)
                mvpData->views[i] = glm::lookAt(probePosition, probePosition + captureViewCenter[i], captureViewUp[i]);
        }
    }

    if (m_probeStorageBuffersMapped.size() > 0)
    {
        const std::vector<std::unique_ptr<Probe>> & probes = probeGrid->getProbes();
        ProbeContainer* probeContainer = static_cast<ProbeContainer*>(m_probeStorageBuffersMapped[backBufferIndex]);

        for (int i = 0; i < probes.size(); i++)
        {
            const Probe* probe = probes[i].get();

            ProbeContainer::Probe probeData{
                .position = probe->position,
            };

            probeContainer->probes[i] = probeData;
        }

        probeContainer->dimensions = probeGrid->getDimensions();
        probeContainer->extent = probeGrid->getExtent();
        probeContainer->cornerPosition = probeGrid->getCornerPosition();
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
                    .attenuation = pointLight->attenuation,
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
    if (m_instanceDescriptorSetUpdatePredPerFrame)
    {
        for (const auto &instanceSet : m_instanceDescriptorSets)
        {
            m_instanceDescriptorSetUpdatePredPerFrame(parentPhase, imageIndex, instanceSet);
        }
    }

    if (m_materialDescriptorSetUpdatePredPerFrame)
    {
        for (const auto &materialSetsPerMesh : m_materialDescriptorSetsPerSubObject)
        {
            for (const auto& materialSet : materialSetsPerMesh)
            {

                m_materialDescriptorSetUpdatePredPerFrame(parentPhase, imageIndex, materialSet);
            }
        }
    }
}

void RenderStateABC::updateDescriptorSets(const RenderPhase *parentPhase, uint32_t imageIndex)
{
    if (m_instanceDescriptorSetUpdatePred)
    {
        for (const auto& set : m_instanceDescriptorSets)
        {
            m_instanceDescriptorSetUpdatePred(parentPhase, imageIndex, set);
        }
    }

    if (m_materialDescriptorSetUpdatePred)
    {
        for (const auto& set : m_instanceDescriptorSets)
        {
            m_materialDescriptorSetUpdatePred(parentPhase, imageIndex, set);
        }
    }
}

void RenderStateABC::recordBackBufferDescriptorSetsCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex, uint32_t backBufferIndex)
{
    std::vector<VkDescriptorSet> descriptorSets;

    if (m_instanceDescriptorSetEnable && backBufferIndex < m_instanceDescriptorSets.size())
    {
        descriptorSets.push_back(m_instanceDescriptorSets[backBufferIndex]);
    }

    if (subObjectIndex < m_materialDescriptorSetsPerSubObject.size())
    {
        if (m_materialDescriptorSetEnable && backBufferIndex < m_materialDescriptorSetsPerSubObject[subObjectIndex].size())
        {
            descriptorSets.push_back(m_materialDescriptorSetsPerSubObject[subObjectIndex][backBufferIndex]);
        }
    }

    if (descriptorSets.size() == 0u)
        return;

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getPipelineLayout(), 0,
                            descriptorSets.size(), descriptorSets.data(), 0, nullptr);
}

void ModelRenderStateBuilder::setPipeline(std::shared_ptr<Pipeline> pipeline)
{
    m_product->m_pipeline = pipeline;
}
void ModelRenderStateBuilder::addPoolSize(VkDescriptorType poolSizeType)
{
    m_poolSizes.push_back(VkDescriptorPoolSize{
        .type = poolSizeType,
        .descriptorCount = m_frameInFlightCount,
    });
}

std::unique_ptr<RenderStateABC> ModelRenderStateBuilder::build()
{
    assert(m_device.lock());

    auto deviceHandle = m_device.lock()->getHandle();

    // descriptor pool
    VkDescriptorPoolCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = m_frameInFlightCount * (1u + 1u * m_product->getSubObjectCount()),
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
    std::optional<VkDescriptorSetLayout> instanceDescriptorSetLayout = m_product->m_pipeline->getDescriptorSetLayoutAtIndex(0u);

    if (instanceDescriptorSetLayout.has_value())
    {
        std::vector<VkDescriptorSetLayout> instanceSetLayouts(m_frameInFlightCount, instanceDescriptorSetLayout.value());
        VkDescriptorSetAllocateInfo instanceDescriptorSetAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = m_product->m_descriptorPool,
            .descriptorSetCount = m_frameInFlightCount,
            .pSetLayouts = instanceSetLayouts.data(),
        };
        m_product->m_instanceDescriptorSets.resize(m_frameInFlightCount);
        res = vkAllocateDescriptorSets(deviceHandle, &instanceDescriptorSetAllocInfo, m_product->m_instanceDescriptorSets.data());
        if (res != VK_SUCCESS)
        {
            std::cerr << "Failed to allocate instance descriptor sets : " << res << std::endl;
            return nullptr;
        }
    }

    std::optional<VkDescriptorSetLayout> materialDescriptorSetLayout = m_product->m_pipeline->getDescriptorSetLayoutAtIndex(1u);

    if (materialDescriptorSetLayout.has_value())
    {
        std::vector<VkDescriptorSetLayout> materialSetLayouts(m_frameInFlightCount, materialDescriptorSetLayout.value());
        VkDescriptorSetAllocateInfo materialDescriptorSetAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = m_product->m_descriptorPool,
            .descriptorSetCount = m_frameInFlightCount,
            .pSetLayouts = materialSetLayouts.data(),
        };

        const uint32_t materialInstanceCount = m_product->getSubObjectCount();
        m_product->m_materialDescriptorSetsPerSubObject.resize(materialInstanceCount);
        for (uint32_t i = 0u; i < materialInstanceCount; i++)
        {
            auto &materialDescriptorSets = m_product->m_materialDescriptorSetsPerSubObject[i];
            materialDescriptorSets.resize(m_frameInFlightCount);

            res = vkAllocateDescriptorSets(deviceHandle, &materialDescriptorSetAllocInfo, materialDescriptorSets.data());
            if (res != VK_SUCCESS)
            {
                std::cerr << "Failed to allocate material descriptor sets : " << res << std::endl;
                return nullptr;
            }
        }
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

    if (m_probeDescriptorEnable)
    {
        m_product->m_probeStorageBuffers.resize(m_frameInFlightCount);
        m_product->m_probeStorageBuffersMapped.resize(m_frameInFlightCount);

        for (int i = 0; i < m_product->m_probeStorageBuffers.size(); ++i)
        {
            BufferBuilder bb;
            BufferDirector bd;
            bd.createStorageBufferBuilder(bb);
            bb.setSize(sizeof(RenderStateABC::ProbeContainer));
            bb.setDevice(m_device);
            m_product->m_probeStorageBuffers[i] = bb.build();

            vkMapMemory(deviceHandle, m_product->m_probeStorageBuffers[i]->getMemory(), 0,
                sizeof(RenderStateABC::ProbeContainer), 0,
                &m_product->m_probeStorageBuffersMapped[i]);
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

    std::vector<VkDescriptorBufferInfo> mvpBufferInfos;
    mvpBufferInfos.reserve(m_frameInFlightCount);

    std::vector<VkDescriptorBufferInfo> probeBufferInfos;
    probeBufferInfos.reserve(m_frameInFlightCount);

    std::vector<VkDescriptorBufferInfo> pointLightBufferInfos;
    std::vector<VkDescriptorBufferInfo> directionalLightBufferInfos;
    pointLightBufferInfos.reserve(m_frameInFlightCount);
    directionalLightBufferInfos.reserve(m_frameInFlightCount);

    std::vector<std::vector<VkDescriptorImageInfo>> envMapImageInfos;
    envMapImageInfos.reserve(m_frameInFlightCount);

    std::vector<VkDescriptorImageInfo> diffuseImageInfos;
    diffuseImageInfos.reserve(m_product->getSubObjectCount() * m_frameInFlightCount);

    UniformDescriptorBuilder udb;
    for (uint32_t i = 0u; i < m_product->m_instanceDescriptorSets.size(); ++i)
    {
        if (m_mvpDescriptorEnable)
        {
            VkDescriptorBufferInfo& mvpBufferInfo = mvpBufferInfos.emplace_back();
            mvpBufferInfo.buffer = m_product->m_mvpUniformBuffers[i]->getHandle();
            mvpBufferInfo.offset = 0;
            mvpBufferInfo.range = sizeof(RenderStateABC::MVP);
            udb.addSetWrites(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_product->m_instanceDescriptorSets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &mvpBufferInfo,
            });
        }

        if (m_probeDescriptorEnable)
        {
            VkDescriptorBufferInfo& probeBufferInfo = probeBufferInfos.emplace_back();
            probeBufferInfo.buffer = m_product->m_probeStorageBuffers[i]->getHandle();
            probeBufferInfo.offset = 0;
            probeBufferInfo.range = sizeof(RenderStateABC::ProbeContainer);
            udb.addSetWrites(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_product->m_instanceDescriptorSets[i],
                .dstBinding = 5,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &probeBufferInfo,
                });
        }

        if (m_lightDescriptorEnable)
        {
            VkDescriptorBufferInfo& pointLightBufferInfo = pointLightBufferInfos.emplace_back();
            pointLightBufferInfo.buffer = m_product->m_pointLightStorageBuffers[i]->getHandle();
            pointLightBufferInfo.offset = 0;
            pointLightBufferInfo.range = sizeof(RenderStateABC::PointLightContainer);
            udb.addSetWrites(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_product->m_instanceDescriptorSets[i],
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &pointLightBufferInfo,
            });
            
            VkDescriptorBufferInfo& directionalLightBufferInfo = directionalLightBufferInfos.emplace_back();
			directionalLightBufferInfo.buffer = m_product->m_directionalLightStorageBuffers[i]->getHandle();
            directionalLightBufferInfo.offset = 0;
            directionalLightBufferInfo.range = sizeof(RenderStateABC::DirectionalLightContainer);
            udb.addSetWrites(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_product->m_instanceDescriptorSets[i],
                .dstBinding = 3,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo = &directionalLightBufferInfo,
            });
        }

        if (m_environmentMaps.size() > 0)
        {
            auto &envMapImageArrayInfos = envMapImageInfos.emplace_back();
            envMapImageArrayInfos.reserve(m_environmentMaps.size());
            // Max probe count per draw (may be higher)
            for (uint32_t i = 0u; i < m_environmentMaps.size(); i++)
            {
                std::shared_ptr<Texture> texPtr = m_environmentMaps[i].lock();

                VkDescriptorImageInfo& envMapImageInfo = envMapImageArrayInfos.emplace_back();
                envMapImageInfo.sampler = *texPtr->getSampler();
                envMapImageInfo.imageView = texPtr->getImageView();
                envMapImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }

            udb.addSetWrites(VkWriteDescriptorSet{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_product->m_instanceDescriptorSets[i],
                .dstBinding = 4,
                .dstArrayElement = 0,
                .descriptorCount = static_cast<uint32_t>(envMapImageArrayInfos.size()),
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = envMapImageArrayInfos.data(),
            });
        }
    }

    for (uint32_t i = 0u; i < m_product->m_materialDescriptorSetsPerSubObject.size(); ++i)
    {
        auto &materialDescriptorSets = m_product->m_materialDescriptorSetsPerSubObject[i];
        for (uint32_t j = 0u; j < materialDescriptorSets.size(); j++)
        {
            if (m_textureDescriptorEnable)
            {
                std::weak_ptr<Texture> currentTexture = m_texture;

                if (currentTexture.expired())
                    currentTexture = m_product->m_model.lock()->getMesh(i)->getTexture();

                if (currentTexture.expired())
                    currentTexture = ModelRenderState::s_defaultDiffuseTexture;

                if (!currentTexture.expired())
                {
                    std::shared_ptr<Texture> texPtr = currentTexture.lock();

                    VkDescriptorImageInfo& diffuseImageInfo = diffuseImageInfos.emplace_back();
                    diffuseImageInfo.sampler = *texPtr->getSampler();
                    diffuseImageInfo.imageView = texPtr->getImageView();
                    diffuseImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

                    udb.addSetWrites(VkWriteDescriptorSet{
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = materialDescriptorSets[j],
                        .dstBinding = 1,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        .pImageInfo = &diffuseImageInfo,
                    });
                }
            }
        }
    }

    std::vector<VkWriteDescriptorSet> writes = udb.buildAndRestart()->getSetWrites();
    if (!writes.empty())
        vkUpdateDescriptorSets(deviceHandle, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    return std::move(m_product);
}

void ModelRenderState::updatePushConstants(const VkCommandBuffer& commandBuffer, uint32_t imageIndex, uint32_t singleFrameRenderCount, const CameraABC& camera, const std::vector<std::shared_ptr<Light>>& lights)
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

void ModelRenderState::recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex)
{
    auto modelPtr = m_model.lock();
    auto meshPtr = modelPtr->getMesh(subObjectIndex);
  
    VkBuffer vbos[] = {meshPtr->getVertexBufferHandle()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vbos, offsets);
    vkCmdBindIndexBuffer(commandBuffer, meshPtr->getIndexBufferHandle(), 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed(commandBuffer, meshPtr->getIndexCount(), 1, 0, 0, 0);
}

void ModelRenderState::updateUniformBuffers(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex, const CameraABC& camera, const std::vector<std::shared_ptr<Light>>& lights, const std::unique_ptr<ProbeGrid> &probeGrid, bool captureModeEnabled)
{
    RenderStateABC::updateUniformBuffers(imageIndex, singleFrameRenderIndex, pooledFramebufferIndex, camera, lights, probeGrid, captureModeEnabled);

    if (m_mvpUniformBuffersMapped.size() > 0)
    {
        MVP* mvpData = static_cast<MVP*>(m_mvpUniformBuffersMapped[imageIndex]);
        auto modelPtr = m_model.lock();

        mvpData->model = modelPtr->getTransform().getTransformMatrix();
    }
}

uint32_t ModelRenderState::getSubObjectCount() const
{
    return m_model.lock()->getMeshes().size();
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

void ImGuiRenderState::recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex)
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
    std::optional<VkDescriptorSetLayout> instanceDescriptorSetLayout = m_product->m_pipeline->getDescriptorSetLayoutAtIndex(0u);

    if (instanceDescriptorSetLayout.has_value())
    {
        std::vector<VkDescriptorSetLayout> instanceSetLayouts(m_frameInFlightCount, instanceDescriptorSetLayout.value());
        VkDescriptorSetAllocateInfo instanceDescriptorSetAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = m_product->m_descriptorPool,
            .descriptorSetCount = m_frameInFlightCount,
            .pSetLayouts = instanceSetLayouts.data(),
        };
        m_product->m_instanceDescriptorSets.resize(m_frameInFlightCount);
        res = vkAllocateDescriptorSets(deviceHandle, &instanceDescriptorSetAllocInfo, m_product->m_instanceDescriptorSets.data());
        if (res != VK_SUCCESS)
        {
            std::cerr << "Failed to allocate instance descriptor sets : " << res << std::endl;
            return nullptr;
        }
    }

    std::optional<VkDescriptorSetLayout> materialDescriptorSetLayout = m_product->m_pipeline->getDescriptorSetLayoutAtIndex(1u);

    if (materialDescriptorSetLayout.has_value())
    {
        std::vector<VkDescriptorSetLayout> materialSetLayouts(m_frameInFlightCount, materialDescriptorSetLayout.value());
        VkDescriptorSetAllocateInfo materialDescriptorSetAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = m_product->m_descriptorPool,
            .descriptorSetCount = m_frameInFlightCount,
            .pSetLayouts = materialSetLayouts.data(),
        };

        const uint32_t materialInstanceCount = m_product->getSubObjectCount();
        m_product->m_materialDescriptorSetsPerSubObject.resize(materialInstanceCount);
        for (uint32_t i = 0u; i < materialInstanceCount; i++)
        {
            auto& materialDescriptorSets = m_product->m_materialDescriptorSetsPerSubObject[i];
            materialDescriptorSets.resize(m_frameInFlightCount);

            res = vkAllocateDescriptorSets(deviceHandle, &materialDescriptorSetAllocInfo, materialDescriptorSets.data());
            if (res != VK_SUCCESS)
            {
                std::cerr << "Failed to allocate material descriptor sets : " << res << std::endl;
                return nullptr;
            }
        }
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

    for (int i = 0; i < m_product->m_instanceDescriptorSets.size(); ++i)
    {
        VkDescriptorBufferInfo mvpBufferInfo = {
            .buffer = m_product->m_mvpUniformBuffers[i]->getHandle(),
            .offset = 0,
            .range = sizeof(RenderStateABC::MVP),
        };

        UniformDescriptorBuilder udb;
        udb.addSetWrites(VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_product->m_instanceDescriptorSets[i],
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
                .dstSet = m_product->m_instanceDescriptorSets[i],
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

void SkyboxRenderState::updateUniformBuffers(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex, const CameraABC &camera,
                                             const std::vector<std::shared_ptr<Light>> &lights, const std::unique_ptr<ProbeGrid> &probeGrid, bool captureModeEnabled)
{
    MVP *mvpData = static_cast<MVP *>(m_mvpUniformBuffersMapped[imageIndex]);
    mvpData->model = glm::identity<glm::mat4>();

    if (!captureModeEnabled)
    {
        mvpData->proj = camera.getProjectionMatrix();
        mvpData->views[0] = camera.getViewMatrix();
    }
    else
    {
        const glm::vec3& probePosition = probeGrid->getProbeAtIndex(pooledFramebufferIndex)->position;

        mvpData->proj = capturePartialProj;
        mvpData->proj[1][1] *= -1;

        for (int i = 0; i < 6; i++)
            mvpData->views[i] = glm::lookAt(probePosition, probePosition + captureViewCenter[i], captureViewUp[i]);
    }
}

void SkyboxRenderState::recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex)
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
    std::optional<VkDescriptorSetLayout> instanceDescriptorSetLayout = m_product->m_pipeline->getDescriptorSetLayoutAtIndex(0u);

    if (instanceDescriptorSetLayout.has_value())
    {
        std::vector<VkDescriptorSetLayout> instanceSetLayouts(m_frameInFlightCount, instanceDescriptorSetLayout.value());
        VkDescriptorSetAllocateInfo instanceDescriptorSetAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = m_product->m_descriptorPool,
            .descriptorSetCount = m_frameInFlightCount,
            .pSetLayouts = instanceSetLayouts.data(),
        };
        m_product->m_instanceDescriptorSets.resize(m_frameInFlightCount);
        res = vkAllocateDescriptorSets(deviceHandle, &instanceDescriptorSetAllocInfo, m_product->m_instanceDescriptorSets.data());
        if (res != VK_SUCCESS)
        {
            std::cerr << "Failed to allocate instance descriptor sets : " << res << std::endl;
            return nullptr;
        }
    }

    std::optional<VkDescriptorSetLayout> materialDescriptorSetLayout = m_product->m_pipeline->getDescriptorSetLayoutAtIndex(1u);

    if (materialDescriptorSetLayout.has_value())
    {
        std::vector<VkDescriptorSetLayout> materialSetLayouts(m_frameInFlightCount, materialDescriptorSetLayout.value());
        VkDescriptorSetAllocateInfo materialDescriptorSetAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = m_product->m_descriptorPool,
            .descriptorSetCount = m_frameInFlightCount,
            .pSetLayouts = materialSetLayouts.data(),
        };

        const uint32_t materialInstanceCount = m_product->getSubObjectCount();
        m_product->m_materialDescriptorSetsPerSubObject.resize(materialInstanceCount);
        for (uint32_t i = 0u; i < materialInstanceCount; i++)
        {
            auto& materialDescriptorSets = m_product->m_materialDescriptorSetsPerSubObject[i];
            materialDescriptorSets.resize(m_frameInFlightCount);

            res = vkAllocateDescriptorSets(deviceHandle, &materialDescriptorSetAllocInfo, materialDescriptorSets.data());
            if (res != VK_SUCCESS)
            {
                std::cerr << "Failed to allocate material descriptor sets : " << res << std::endl;
                return nullptr;
            }
        }
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

    for (int i = 0; i < m_product->m_instanceDescriptorSets.size(); ++i)
    {
        VkDescriptorBufferInfo mvpBufferInfo = {
            .buffer = m_product->m_mvpUniformBuffers[i]->getHandle(),
            .offset = 0,
            .range = sizeof(RenderStateABC::MVP),
        };

        UniformDescriptorBuilder udb;
        udb.addSetWrites(VkWriteDescriptorSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_product->m_instanceDescriptorSets[i],
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
                .dstSet = m_product->m_instanceDescriptorSets[i],
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

void EnvironmentCaptureRenderState::updateUniformBuffers(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex, const CameraABC& camera,
    const std::vector<std::shared_ptr<Light>>& lights, const std::unique_ptr<ProbeGrid> &probeGrid, bool captureModeEnabled)
{
    uint32_t bufferIndex = std::min(m_mvpUniformBuffersMapped.size() - 1, (size_t)imageIndex);

    MVP* mvpData = static_cast<MVP*>(m_mvpUniformBuffersMapped[bufferIndex]);
    mvpData->model = glm::identity<glm::mat4>();

    if (!captureModeEnabled)
    {
        mvpData->proj = camera.getProjectionMatrix();
        mvpData->views[0] = camera.getViewMatrix();
    }
    else
    {
        const glm::vec3& probePosition = probeGrid->getProbeAtIndex(pooledFramebufferIndex)->position;

        mvpData->proj = capturePartialProj;
        mvpData->proj[1][1] *= -1;

        for (int i = 0; i < 6; i++)
            mvpData->views[i] = glm::lookAt(probePosition, probePosition + captureViewCenter[i], captureViewUp[i]);
    }
}

void EnvironmentCaptureRenderState::recordBackBufferDrawObjectCommands(const VkCommandBuffer& commandBuffer, uint32_t subObjectIndex)
{
    auto skyboxPtr = m_skybox.lock();

    VkBuffer vbos[] = { skyboxPtr->getVertexBufferHandle() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vbos, offsets);
    vkCmdDraw(commandBuffer, skyboxPtr->getVertexCount(), 1, 0, 0);
}
