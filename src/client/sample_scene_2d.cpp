#include "graphics/device.hpp"

#include "renderer/light.hpp"
#include "renderer/mesh.hpp"
#include "renderer/model.hpp"
#include "renderer/texture.hpp"

#include "engine/camera.hpp"

#include "scripts/radiance_cascades.hpp"

#include "sample_scene_2d.hpp"

SampleScene2D::SampleScene2D(std::weak_ptr<Device> device, uint32_t framesInFlightCount)
{
    m_cameras.emplace_back(std::make_unique<OrthographicCamera>());
    m_mainCamera = m_cameras[m_cameras.size() - 1].get();
    m_mainCamera->setTransform(Transform{
        .position = {0.f, 0.f, 200.f},
        .rotation = glm::identity<glm::quat>(),
        .scale = {1.f, 1.f, 1.f},
    });
    m_mainCamera->setNear(-1000.f);

    auto radianceCascadesScript = std::make_unique<RadianceCascades>();
    RadianceCascades::init_data init{
        .device = device,
        .frameInFlightCount = framesInFlightCount,
    };
    radianceCascadesScript->init(&init);
    m_scripts.push_back(std::move(radianceCascadesScript));

    std::shared_ptr<PointLight> light = std::make_shared<PointLight>();
    light->position = glm::vec3(0.0, 0.25, 0.1);
    light->diffuseColor = glm::vec3(1.0);
    light->diffusePower = 1.0;
    light->specularColor = glm::vec3(1.0);
    light->specularPower = 1.0;
    m_lights.push_back(light);

    const std::vector<Vertex> vertices = {
        {{-0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 0.f, 0.f, 1.f}, {1.f, 0.f}},
        {{0.5f, -0.5f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 1.f, 0.f, 1.f}, {0.f, 0.f}},
        {{0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {0.f, 0.f, 1.f, 1.f}, {0.f, 1.f}},
        {{-0.5f, 0.5f, 0.f}, {0.f, 0.f, 1.f}, {1.f, 1.f, 1.f, 1.f}, {1.f, 1.f}},
    };
    const std::vector<uint16_t> indices = {
        0, 1, 2, 2, 3, 0,
    };
    MeshBuilder mb;
    mb.setDevice(device);
    mb.setVertices(vertices);
    mb.setIndices(indices);
    std::shared_ptr<Mesh> mesh = mb.buildAndRestart();

    const std::vector<unsigned char> imagePixels = {
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 0, 255, 255,
    };
    TextureBuilder tb;
    TextureDirector td;
    td.configureSRGBTextureBuilder(tb);
    tb.setDevice(device);
    tb.setImageData(imagePixels);
    tb.setWidth(2);
    tb.setHeight(2);
    mesh->setTexture(tb.buildAndRestart());

    ModelBuilder modelBuilder;
    modelBuilder.setMesh(mesh);

    m_objects.push_back(modelBuilder.build());
}