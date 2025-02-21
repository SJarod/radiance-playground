#include <glm/glm.hpp>
#include <iostream>

#include "engine/camera.hpp"
#include "engine/uniform.hpp"
#include "graphics/buffer.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/render_pass.hpp"
#include "mesh.hpp"
#include "texture.hpp"

#include "render_state.hpp"

RenderStateABC::~RenderStateABC()
{
    if (!m_device.lock())
        return;

    vkDestroyDescriptorPool(m_device.lock()->getHandle(), m_descriptorPool, nullptr);

    m_pipeline.reset();
}

void RenderStateABC::updateUniformBuffers(uint32_t imageIndex, const Camera &camera)
{
    MVP ubo = {
        .model = glm::identity<glm::mat4>(),
        .view = camera.getViewMatrix(),
        .proj = camera.getProjectionMatrix(),
    };
    memcpy(m_uniformBuffersMapped[imageIndex], &ubo, sizeof(ubo));
}

void RenderStateABC::recordBackBufferDescriptorSetsCommands(VkCommandBuffer &commandBuffer, uint32_t imageIndex)
{
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getPipelineLayout(), 0, 1,
                            &m_descriptorSets[imageIndex], 0, nullptr);
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

    m_product->m_uniformBuffers.resize(m_frameInFlightCount);
    m_product->m_uniformBuffersMapped.resize(m_frameInFlightCount);
    for (int i = 0; i < m_product->m_uniformBuffers.size(); ++i)
    {
        BufferBuilder bb;
        BufferDirector bd;
        bd.createUniformBufferBuilder(bb);
        bb.setSize(sizeof(RenderStateABC::MVP));
        bb.setDevice(m_device);
        m_product->m_uniformBuffers[i] = bb.build();

        vkMapMemory(deviceHandle, m_product->m_uniformBuffers[i]->getMemory(), 0, sizeof(RenderStateABC::MVP), 0,
                    &m_product->m_uniformBuffersMapped[i]);
    }

    for (int i = 0; i < m_product->m_descriptorSets.size(); ++i)
    {
        VkDescriptorBufferInfo bufferInfo = {
            .buffer = m_product->m_uniformBuffers[i]->getHandle(),
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
            .pBufferInfo = &bufferInfo,
        });

        VkDescriptorImageInfo imageInfo = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        if (m_texture.lock())
        {
            auto texPtr = m_texture.lock();
            imageInfo.sampler = texPtr->getSampler();
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

        std::vector<VkWriteDescriptorSet> writes = udb.build()->getSetWrites();
        vkUpdateDescriptorSets(deviceHandle, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    auto result = std::move(m_product);
    return result;
}

void MeshRenderState::recordBackBufferDrawObjectCommands(VkCommandBuffer &commandBuffer)
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
        .maxSets = 1000,
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

    m_product->m_uniformBuffers.resize(m_frameInFlightCount);
    m_product->m_uniformBuffersMapped.resize(m_frameInFlightCount);
    for (int i = 0; i < m_product->m_uniformBuffers.size(); ++i)
    {
        BufferBuilder bb;
        BufferDirector bd;
        bd.createUniformBufferBuilder(bb);
        bb.setSize(sizeof(RenderStateABC::MVP));
        bb.setDevice(m_device);
        m_product->m_uniformBuffers[i] = bb.build();

        vkMapMemory(deviceHandle, m_product->m_uniformBuffers[i]->getMemory(), 0, sizeof(RenderStateABC::MVP), 0,
            &m_product->m_uniformBuffersMapped[i]);
    }

    for (int i = 0; i < m_product->m_descriptorSets.size(); ++i)
    {
        VkDescriptorBufferInfo bufferInfo = {
            .buffer = m_product->m_uniformBuffers[i]->getHandle(),
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
            .pBufferInfo = &bufferInfo,
            });

        VkDescriptorImageInfo imageInfo = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };
        if (m_texture.lock())
        {
            auto texPtr = m_texture.lock();
            imageInfo.sampler = texPtr->getSampler();
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

        std::vector<VkWriteDescriptorSet> writes = udb.build()->getSetWrites();
        vkUpdateDescriptorSets(deviceHandle, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
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
        .descriptorCount = 1000,
        });
}