#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "engine/vertex.hpp"
#include "graphics/buffer.hpp"

class Device;
class Buffer;
class Texture;
class SkyboxBuilder;

class Skybox
{
    friend SkyboxBuilder;

  private:
    std::weak_ptr<Device> m_device;

    std::unique_ptr<Buffer> m_vertexBuffer;
    std::unique_ptr<Buffer> m_indexBuffer;

    std::vector<Vertex> m_vertices;
    std::vector<uint16_t> m_indices;

    std::shared_ptr<Texture> m_texture;

    Skybox() = default;

  public:
    ~Skybox();

    Skybox(const Skybox &) = delete;
    Skybox &operator=(const Skybox &) = delete;
    Skybox(Skybox &&) = delete;
    Skybox &operator=(Skybox &&) = delete;

  public:
    [[nodiscard]] inline const VkBuffer getVertexBufferHandle() const
    {
        return m_vertexBuffer->getHandle();
    }
    [[nodiscard]] inline const VkBuffer getIndexBufferHandle() const
    {
        return m_indexBuffer->getHandle();
    }
    [[nodiscard]] inline const uint32_t getVertexCount() const
    {
        return m_vertices.size();
    }
    [[nodiscard]] inline const uint32_t getIndexCount() const
    {
        return m_indices.size();
    }
    [[nodiscard]] inline std::weak_ptr<Texture> getTexture() const
    {
        return m_texture;
    }

  public:
    void setTexture(const std::shared_ptr<Texture> &texture)
    {
        m_texture = texture;
    }
};

class SkyboxBuilder
{
 private:
    std::unique_ptr<Skybox> m_product;

    std::weak_ptr<Device> m_device;

    unsigned int m_importerFlags;

    void restart()
    {
        m_product = std::unique_ptr<Skybox>(new Skybox);
    }

    void createVertexBuffer();
    void createIndexBuffer();

 public:
    SkyboxBuilder()
    {
        restart();
    }

    void setDevice(std::weak_ptr<Device> device)
    {
        m_device = device;
        m_product->m_device = device;
    }

    void setCubemap(const std::shared_ptr<Texture>& cubemap)
    {
        m_product->m_texture = cubemap;
    }

    std::unique_ptr<Skybox> buildAndRestart();
};
