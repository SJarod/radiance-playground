#pragma once

#include <memory>

#include <vulkan/vulkan.hpp>

class Device;
class BufferBuilder;

class Buffer
{
    friend BufferBuilder;

  private:
    std::weak_ptr<Device> m_device;

    VkBuffer m_handle;
    VkDeviceMemory m_memory;
    size_t m_size;

    Buffer() = default;

  public:
    ~Buffer();

    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;
    Buffer(Buffer &&) = delete;
    Buffer &operator=(Buffer &&) = delete;

    void copyDataToMemory(const void *srcData);

    void transferBufferToBuffer(VkBuffer src);

  public:
    [[nodiscard]] inline const VkBuffer &getHandle() const
    {
        return m_handle;
    }

    [[nodiscard]] inline const VkDeviceMemory &getMemory() const
    {
        return m_memory;
    }

    [[nodiscrad]] inline const size_t getSize() const
    {
        return m_size;
    }
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