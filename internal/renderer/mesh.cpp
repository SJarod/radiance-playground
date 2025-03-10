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

void MeshBuilder::setVerticesFromAiScene(const aiScene *pScene)
{
    aiMesh *mesh = pScene->mMeshes[0];
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
    {
        const aiVector3D pPos = mesh->mVertices[i];
        const aiVector3D pNormal = mesh->mNormals[i];
        aiVector3D pUV = aiVector3D(0.f, 0.f, 0.f);
        if (mesh->HasTextureCoords(0))
            pUV = mesh->mTextureCoords[0][i];

        m_product->m_vertices.emplace_back(
            Vertex({pPos.x, pPos.y, pPos.z}, {pNormal.x, pNormal.y, pNormal.z}, {0.f, 0.f, 0.f, 1.f}, {pUV.x, pUV.y}));
    }
}
void MeshBuilder::setIndicesFromAiScene(const aiScene *pScene)
{
    aiMesh *mesh = pScene->mMeshes[0];
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
    {
        const aiFace &Face = mesh->mFaces[i];
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

        setVerticesFromAiScene(pScene);
        setIndicesFromAiScene(pScene);
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