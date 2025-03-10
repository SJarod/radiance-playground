#include <iostream>

#include "graphics/buffer.hpp"
#include "graphics/device.hpp"

#include "skybox.hpp"

Skybox::~Skybox()
{
    m_indexBuffer.reset();
    m_vertexBuffer.reset();
}

void SkyboxBuilder::createVertexBuffer()
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

void SkyboxBuilder::createIndexBuffer()
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

std::unique_ptr<Skybox> SkyboxBuilder::buildAndRestart()
{
    m_product->m_vertices = {
        // positions          // normals
        {{-1.0f, 1.0f, -1.0f}}, {{-1.0f, -1.0f, -1.0f}}, {{1.0f, -1.0f, -1.0f}},  {{1.0f, -1.0f, -1.0f}},
        {{1.0f, 1.0f, -1.0f}},  {{-1.0f, 1.0f, -1.0f}},  {{-1.0f, -1.0f, 1.0f}},  {{-1.0f, -1.0f, -1.0f}},
        {{-1.0f, 1.0f, -1.0f}}, {{-1.0f, 1.0f, -1.0f}},  {{-1.0f, 1.0f, 1.0f}},   {{-1.0f, -1.0f, 1.0f}},
        {{1.0f, -1.0f, -1.0f}}, {{1.0f, -1.0f, 1.0f}},   {{1.0f, 1.0f, 1.0f}},    {{1.0f, 1.0f, 1.0f}},
        {{1.0f, 1.0f, -1.0f}},  {{1.0f, -1.0f, -1.0f}},  {{-1.0f, -1.0f, 1.0f}},  {{-1.0f, 1.0f, 1.0f}},
        {{1.0f, 1.0f, 1.0f}},   {{1.0f, 1.0f, 1.0f}},    {{1.0f, -1.0f, 1.0f}},   {{-1.0f, -1.0f, 1.0f}},
        {{-1.0f, 1.0f, -1.0f}}, {{1.0f, 1.0f, -1.0f}},   {{1.0f, 1.0f, 1.0f}},    {{1.0f, 1.0f, 1.0f}},
        {{-1.0f, 1.0f, 1.0f}},  {{-1.0f, 1.0f, -1.0f}},  {{-1.0f, -1.0f, -1.0f}}, {{-1.0f, -1.0f, 1.0f}},
        {{1.0f, -1.0f, -1.0f}}, {{1.0f, -1.0f, -1.0f}},  {{-1.0f, -1.0f, 1.0f}},  {{1.0f, -1.0f, 1.0f}},
    };

    createVertexBuffer();

    auto result = std::move(m_product);
    restart();
    return result;
}
