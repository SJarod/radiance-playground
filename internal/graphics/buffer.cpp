#include <cassert>
#include <iostream>

#include "device.hpp"

#include "engine/uniform.hpp"

#include "buffer.hpp"

void Buffer::mapMemory(void **ppData)
{
    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    vmaMapMemory(devicePtr->getAllocator(), m_allocation, ppData);
}

void Buffer::copyDataToMemory(const void *srcData)
{
    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    // filling the VBO (bind and unbind CPU accessible memory)
    void *data;

    vmaMapMemory(devicePtr->getAllocator(), m_allocation, &data);

    memcpy(data, srcData, m_size);

    vmaUnmapMemory(devicePtr->getAllocator(), m_allocation);
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

    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    VmaAllocationInfo info;
    vmaGetAllocationInfo(devicePtr->getAllocator(), m_allocation, &info);
    if (info.pMappedData)
        vmaUnmapMemory(devicePtr->getAllocator(), m_allocation);

    vmaDestroyBuffer(devicePtr->getAllocator(), m_handle, m_allocation);
    devicePtr->addBufferCount(-1);
    devicePtr->untrackBufferName(m_name);
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

    VmaAllocationCreateInfo allocInfo = {
        .requiredFlags = m_properties,
    };

    VkResult res = vmaCreateBuffer(devicePtr->getAllocator(), &createInfo, &allocInfo, &m_product->m_handle,
                                   &m_product->m_allocation, nullptr);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create buffer and allocate memory: " << res << std::endl;
        return nullptr;
    }

    m_product->m_name += std::string(" Buffer " + std::to_string(devicePtr->getBufferCount()));
    devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_BUFFER,
        .objectHandle = (uint64_t)m_product->m_handle,
        .pObjectName = m_product->m_name.c_str(),
    });
    devicePtr->addBufferCount(1);
    devicePtr->trackBufferName(m_product->m_name);

    VmaAllocationInfo info;
    vmaGetAllocationInfo(devicePtr->getAllocator(), m_product->m_allocation, &info);

    static int deviceMemoryCount = 0;
    devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_DEVICE_MEMORY,
        .objectHandle = (uint64_t)info.deviceMemory,
        .pObjectName = std::string("Buffer Device Memory " + std::to_string(deviceMemoryCount++)).c_str(),
    });

    return std::move(m_product);
}

void BufferDirector::configureStagingBufferBuilder(BufferBuilder &builder)
{
    builder.setUsage(VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}
void BufferDirector::configureVertexBufferBuilder(BufferBuilder &builder)
{
    builder.setUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}
void BufferDirector::configureIndexBufferBuilder(BufferBuilder &builder)
{
    builder.setUsage(VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
}
void BufferDirector::configureUniformBufferBuilder(BufferBuilder &builder)
{
    builder.setUsage(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

void BufferDirector::configureStorageBufferBuilder(BufferBuilder &builder)
{
    builder.setUsage(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}