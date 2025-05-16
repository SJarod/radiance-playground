#pragma once

#include <memory>
#include <string>

#include <vulkan/vulkan.hpp>

#include <vk_mem_alloc.h>

class Device;
class BufferBuilder;

class Buffer
{
    friend BufferBuilder;

  private:
    std::weak_ptr<Device> m_device;

    std::string m_name = "Unnamed";

    VkBuffer m_handle;
    VmaAllocation m_allocation;
    size_t m_size;

    Buffer() = default;

  public:
    ~Buffer();

    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;
    Buffer(Buffer &&) = delete;
    Buffer &operator=(Buffer &&) = delete;

    void mapMemory(void **ppData);

    void copyDataToMemory(const void *srcData);

    void transferBufferToBuffer(Buffer &src);

  public:
    [[nodiscard]] inline const VkBuffer &getHandle() const
    {
        return m_handle;
    }

    [[nodiscrad]] inline const size_t getSize() const
    {
        return m_size;
    }

    [[nodiscard]] inline std::string getName() const
    {
        return m_name;
    }

    [[nodiscard]] VkDeviceAddress getDeviceAddress() const;
};

class BufferBuilder
{
  private:
    std::unique_ptr<Buffer> m_product;

    std::weak_ptr<Device> m_device;

    size_t m_size;

    VkBufferUsageFlags m_usage;
    VkMemoryPropertyFlags m_properties;

  public:
    BufferBuilder()
    {
        restart();
    }

    void restart()
    {
        m_product = std::unique_ptr<Buffer>(new Buffer);
    }

    void setDevice(std::weak_ptr<Device> a)
    {
        m_device = a;
        m_product->m_device = a;
    }
    void setSize(size_t a)
    {
        m_size = a;
        m_product->m_size = a;
    }
    void setUsage(VkBufferUsageFlags a)
    {
        m_usage = a;
    }
    void setProperties(VkMemoryPropertyFlags a)
    {
        m_properties = a;
    }
    void setName(std::string name)
    {
        m_product->m_name = name;
    }

    std::unique_ptr<Buffer> build();
};

class BufferDirector
{
  public:
    void configureStagingBufferBuilder(BufferBuilder &builder);
    void configureVertexBufferBuilder(BufferBuilder &builder);
    void configureIndexBufferBuilder(BufferBuilder &builder);
    void configureUniformBufferBuilder(BufferBuilder &builder);
    void configureStorageBufferBuilder(BufferBuilder &builder);
};