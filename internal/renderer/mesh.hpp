#pragma once

#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

#include "engine/vertex.hpp"
#include "engine/transform.hpp"
#include "graphics/buffer.hpp"

class Device;
class Buffer;
class Texture;
class aiMesh;
class MeshBuilder;

class Mesh
{
    friend MeshBuilder;

  private:
    std::weak_ptr<Device> m_device;

    std::unique_ptr<Buffer> m_vertexBuffer;
    std::unique_ptr<Buffer> m_indexBuffer;

    std::vector<Vertex> m_vertices;
    std::vector<uint16_t> m_indices;

    std::shared_ptr<Texture> m_texture;

    Mesh() = default;
  public:
    ~Mesh();

    Mesh(const Mesh &) = delete;
    Mesh &operator=(const Mesh &) = delete;
    Mesh(Mesh &&) = delete;
    Mesh &operator=(Mesh &&) = delete;

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

class MeshBuilder
{
  private:
    std::unique_ptr<Mesh> m_product;

    std::weak_ptr<Device> m_device;

    std::string m_modelFilename;
    bool m_bLoadFromFile = false;

    unsigned int m_importerFlags = 0x00000000;

    void restart()
    {
        m_product = std::unique_ptr<Mesh>(new Mesh);
    }

    void createVertexBuffer();
    void createIndexBuffer();

  public:
    MeshBuilder()
    {
        restart();
    }

    void setDevice(std::weak_ptr<Device> device)
    {
        m_device = device;
        m_product->m_device = device;
    }
    void setVertices(const std::vector<Vertex> &vertices)
    {
        m_product->m_vertices = vertices;
        m_bLoadFromFile = false;
    }
    void setIndices(const std::vector<uint16_t> &indices)
    {
        m_product->m_indices = indices;
        m_bLoadFromFile = false;
    }
    void setModelFilename(const std::string &filename)
    {
        m_modelFilename = filename;
        m_bLoadFromFile = true;
    }
    void setModelImporterFlags(unsigned int flags)
    {
        m_importerFlags = flags;
    }

    void setVerticesFromAiMesh(const aiMesh* pMesh);
    void setIndicesFromAiMesh(const aiMesh* pMesh);

    std::unique_ptr<Mesh> buildAndRestart();
};

class MeshDirector
{
  public:
    void createAssimpMeshBuilder(MeshBuilder &builder);
    void createSphereMeshBuilder(MeshBuilder& builder, float radius, float latitude, float longitude);
    void createCubeMeshBuilder(MeshBuilder &builder, const glm::vec3& halfExtent);
};