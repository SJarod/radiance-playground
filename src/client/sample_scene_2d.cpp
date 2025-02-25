#include "graphics/device.hpp"

#include "renderer/mesh.hpp"
#include "renderer/texture.hpp"

#include "engine/camera.hpp"

#include "sample_scene_2d.hpp"

SampleScene2D::SampleScene2D(std::weak_ptr<Device> device) : SceneABC()
{
    m_cameras.emplace_back(std::make_unique<OrthographicCamera>());
    m_mainCamera = m_cameras[m_cameras.size() - 1].get();

    MeshBuilder mb;
    MeshDirector md;
    md.createAssimpMeshBuilder(mb);
    mb.setDevice(device);
    mb.setModelFilename("assets/viking_room.obj");
    std::shared_ptr<Mesh> mesh = mb.build();

    TextureBuilder tb;
    TextureDirector td;
    td.createSRGBTextureBuilder(tb);
    tb.setDevice(device);
    tb.setTextureFilename("assets/viking_room.png");
    mesh->setTexture(tb.build());

    m_objects.push_back(mesh);

    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f, 1.f}, {1.f, 0.f}},
        {{0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 1.f, 0.f, 1.f}, {0.f, 0.f}},
        {{0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f, 1.f}, {0.f, 1.f}},
        {{-0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f}},
        {{-0.5f, -0.5f, -0.5f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f, 1.f}, {1.f, 0.f}},
        {{0.5f, -0.5f, -0.5f}, {0.f, 0.f, 1.f}, {0.f, 1.f, 0.f, 1.f}, {0.f, 0.f}},
        {{0.5f, 0.5f, -0.5f}, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f, 1.f}, {0.f, 1.f}},
        {{-0.5f, 0.5f, -0.5f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f}},
    };
    const std::vector<uint16_t> indices = {
        0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4,
    };
    mb.setDevice(device);
    mb.setVertices(vertices);
    mb.setIndices(indices);
    std::shared_ptr<Mesh> mesh2 = mb.build();

    const std::vector<unsigned char> imagePixels = {
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 0, 255, 255,
    };
    td.createSRGBTextureBuilder(tb);
    tb.setDevice(device);
    tb.setImageData(imagePixels);
    tb.setWidth(2);
    tb.setHeight(2);
    mesh2->setTexture(tb.build());

    m_objects.push_back(mesh2);
}