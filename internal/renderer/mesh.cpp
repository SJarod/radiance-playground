#include <iostream>

#include <assimp/Importer.hpp>
#include <assimp/mesh.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "graphics/buffer.hpp"
#include "graphics/device.hpp"

#include "mesh.hpp"

Mesh::~Mesh()
{
    m_indexBuffer.reset();
    m_vertexBuffer.reset();
}

void MeshBuilder::createVertexBuffer()
{
    assert(!m_product->m_vertices.empty());

    // vertex buffer

    size_t vertexBufferSize = sizeof(Vertex) * m_product->m_vertices.size();

    BufferBuilder bb;
    BufferDirector bd;
    bd.configureStagingBufferBuilder(bb);
    bb.setDevice(m_product->m_device);
    bb.setSize(vertexBufferSize);
    std::unique_ptr<Buffer> stagingBuffer = bb.build();

    stagingBuffer->copyDataToMemory(m_product->m_vertices.data());

    bb.restart();
    bd.createVertexBufferBuilder(bb);
    bb.setDevice(m_product->m_device);
    bb.setSize(vertexBufferSize);
    m_product->m_vertexBuffer = bb.build();

    // transfer from staging buffer to vertex buffer

    m_product->m_vertexBuffer->transferBufferToBuffer(stagingBuffer->getHandle());
    stagingBuffer.reset();
}

void MeshBuilder::createIndexBuffer()
{
    assert(!m_product->m_indices.empty());

    // index buffer

    size_t indexBufferSize = sizeof(uint16_t) * m_product->m_indices.size();

    BufferBuilder bb;
    BufferDirector bd;
    bd.configureStagingBufferBuilder(bb);
    bb.setDevice(m_product->m_device);
    bb.setSize(indexBufferSize);

    std::unique_ptr<Buffer> stagingBuffer = bb.build();

    stagingBuffer->copyDataToMemory(m_product->m_indices.data());

    bb.restart();
    bd.createIndexBufferBuilder(bb);
    bb.setDevice(m_product->m_device);
    bb.setSize(indexBufferSize);
    m_product->m_indexBuffer = bb.build();

    m_product->m_indexBuffer->transferBufferToBuffer(stagingBuffer->getHandle());
    stagingBuffer.reset();
}

void MeshBuilder::setVerticesFromAiMesh(const aiMesh *pMesh)
{
    for (unsigned int i = 0; i < pMesh->mNumVertices; ++i)
    {
        const aiVector3D pPos = pMesh->mVertices[i];
        const aiVector3D pNormal = pMesh->mNormals[i];
        aiVector3D pUV = aiVector3D(0.f, 0.f, 0.f);
        if (pMesh->HasTextureCoords(0))
            pUV = pMesh->mTextureCoords[0][i];

        m_product->m_vertices.emplace_back(
            Vertex({pPos.x, pPos.y, pPos.z}, {pNormal.x, pNormal.y, pNormal.z}, {0.f, 0.f, 0.f, 1.f}, {pUV.x, pUV.y}));
    }
}
void MeshBuilder::setIndicesFromAiMesh(const aiMesh *pMesh)
{
    for (unsigned int i = 0; i < pMesh->mNumFaces; ++i)
    {
        const aiFace &Face = pMesh->mFaces[i];
        assert(Face.mNumIndices == 3);
        m_product->m_indices.push_back(Face.mIndices[0]);
        m_product->m_indices.push_back(Face.mIndices[1]);
        m_product->m_indices.push_back(Face.mIndices[2]);
    }
}

std::unique_ptr<Mesh> MeshBuilder::buildAndRestart()
{
    if (m_bLoadFromFile)
    {
        Assimp::Importer importer;
        const aiScene *pScene = importer.ReadFile(m_modelFilename, m_importerFlags);
        if (!pScene)
        {
            std::cerr << "Failed to load model : " << m_modelFilename << std::endl;
            return nullptr;
        }

        setVerticesFromAiMesh(pScene->mMeshes[0]);
        setIndicesFromAiMesh(pScene->mMeshes[0]);
    }

    createVertexBuffer();
    createIndexBuffer();

    auto result = std::move(m_product);
    restart();
    return result;
}

void MeshDirector::createAssimpMeshBuilder(MeshBuilder &builder)
{
    builder.setModelImporterFlags(aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs |
                                  aiProcess_JoinIdenticalVertices | aiProcess_ForceGenNormals);
}

void createSphereMesh(std::vector<Vertex> &vertices, std::vector<uint16_t> &indices, float radius, float latitude, float longitude)
{
    unsigned int uint_lon = static_cast<unsigned int>(longitude);
    unsigned int uint_lat = static_cast<unsigned int>(latitude);
    vertices.reserve((size_t)uint_lat * (size_t)uint_lon);

    const float R = 1.f / (latitude - 1.f);
    const float S = 1.f / (longitude - 1.f);

    for (unsigned int r = 0; r < uint_lat; r++)
    {
        for (unsigned int s = 0; s < uint_lon; s++)
        {
            Vertex vertex;

            const float y = glm::sin(-glm::half_pi<float>() + glm::pi<float>() * r * R);
            const float x = glm::cos(glm::two_pi<float>() * s * S) * glm::sin(glm::pi<float>() * r * R);
            const float z = glm::sin(glm::two_pi<float>() * s * S) * glm::sin(glm::pi<float>() * r * R);

            vertex.uv.x = s * S;
            vertex.uv.y = r * R;

            vertex.position = { x, y, z };
            vertex.position = glm::normalize(vertex.position) * radius;
            vertex.normal = glm::vec3(x, y, z);

            vertices.push_back(vertex);
        }
    }

    indices.reserve((size_t)uint_lat * (size_t)uint_lon * 6);

    for (unsigned int r = 0; r < uint_lat; r++)
    {
        for (unsigned int s = 0; s < uint_lon; s++)
        {
            indices.push_back(r * uint_lon + s);
            indices.push_back((r + 1) * uint_lon + s);
            indices.push_back(r * uint_lon + (s + 1));

            indices.push_back((r + 1) * uint_lon + (s + 1));
            indices.push_back(r * uint_lon + (s + 1));
            indices.push_back((r + 1) * uint_lon + s);
        }
    }
}

void MeshDirector::createSphereMeshBuilder(MeshBuilder &builder, float radius, float latitude, float longitude)
{
    std::vector<Vertex> vertices;
    std::vector<uint16_t> indices;
        
    createSphereMesh(vertices, indices, radius, latitude, longitude);

    builder.setVertices(vertices);
    builder.setIndices(indices);
}