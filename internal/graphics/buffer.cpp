#include <cassert>
#include <iostream>

#include "device.hpp"

#include "engine/uniform.hpp"

#include "buffer.hpp"

void Buffer::copyDataToMemory(const void *srcData)
{
    auto deviceHandle = m_device.lock()->getHandle();
    // filling the VBO (bind and unbind CPU accessible memory)
    void *data;
    vkMapMemory(deviceHandle, m_memory, 0, m_size, 0, &data);
    // TODO : flush memory
    memcpy(data, srcData, m_size);
    // TODO : invalidate memory before reading in the pipeline
    vkUnmapMemory(deviceHandle, m_memory);
}

void Buffer::transferBufferToBuffer(VkBuffer src)
{
    auto devicePtr = m_device.lock();

    VkCommandBuffer commandBuffer = devicePtr->cmdBeginOneTimeSubmit();

    VkBufferCopy copyRegion{
        .size = m_size,
    };
    vkCmdCopyBuffer(commandBuffer, src, m_handle, 1, &copyRegion);

    devicePtr->cmdEndOneTimeSubmit(commandBuffer);
}

Buffer::~Buffer()
{
    if (m_device.expired())
        return;

    auto deviceHandle = m_device.lock()->getHandle();
    vkFreeMemory(deviceHandle, m_memory, nullptr);
    vkDestroyBuffer(deviceHandle, m_handle, nullptr);
}

std::unique_ptr<Buffer> BufferBuilder::build()
{
    assert(!m_device.expired());

    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    VkBufferCreateInfo createInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                     .flags = 0,
                                     .size = m_size,
                                     .usage = m_usage,
                                     .sharingMode = VK_SHARING_MODE_EXCLUSIVE};
    VkResult res = vkCreateBuffer(deviceHandle, &createInfo, nullptr, &m_product->m_handle);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create buffer : " << res << std::endl;
        return nullptr;
    }
    static int bufferCount = 0;
    devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)m_product->m_handle,
        .pObjectName = std::string("Buffer " + std::to_string(bufferCount++)).c_str(),
    });

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(deviceHandle, m_product->m_handle, &memReq);
    std::optional<uint32_t> memoryTypeIndex = devicePtr->findMemoryTypeIndex(memReq, m_properties);
    VkMemoryAllocateInfo allocInfo = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                      .allocationSize = memReq.size,
                                      .memoryTypeIndex = memoryTypeIndex.value()};
    res = vkAllocateMemory(deviceHandle, &allocInfo, nullptr, &m_product->m_memory);
    static int deviceMemoryCount = 0;
    devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_DEVICE_MEMORY,
        .objectHandle = (uint64_t)m_product->m_memory,
        .pObjectName = std::string("Buffer Device Memory " + std::to_string(deviceMemoryCount++)).c_str(),
    });

    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to allocate buffer memory : " << res << std::endl;
        return nullptr;
    }

    vkBindBufferMemory(deviceHandle, m_product->m_handle, m_product->m_memory, 0);

    return std::move(m_product);
}

void BufferDirector::configureStagingBufferBuilder(BufferBuilder &builder)
{
    builder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}
void BufferDirector::createVertexBufferBuilder(BufferBuilder &builder)
{
    builder.setUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}
void BufferDirector::createIndexBufferBuilder(BufferBuilder &builder)
{
    builder.setUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}
void BufferDirector::createUniformBufferBuilder(BufferBuilder &builder)
{
    builder.setUsage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void BufferDirector::createStorageBufferBuilder(BufferBuilder &builder)
{
    builder.setUsage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}