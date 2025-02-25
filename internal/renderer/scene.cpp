#include "light.hpp"
#include "mesh.hpp"
#include "texture.hpp"

#include "scene.hpp"

Scene::Scene(const std::weak_ptr<Device> device)
{
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

    std::shared_ptr<PointLight> light = std::make_shared<PointLight>();
    light->position = glm::vec3(-1.0, 0.0, 0.0);
	light->diffuseColor = glm::vec3(0.4, 1.0, 0.2);
	light->diffusePower =  1.0;
	light->specularColor = glm::vec3(1.0);
	light->specularPower = 1.0;
    m_lights.push_back(light);

    std::shared_ptr<PointLight> light1 = std::make_shared<PointLight>();
    light1->position = glm::vec3(0.0, 1.0, 1.0);
    light1->diffuseColor = glm::vec3(0.0, 0.0, 1.0);
    light1->diffusePower = 1.0;
    light1->specularColor = glm::vec3(1.0);
    light1->specularPower = 1.0;
    m_lights.push_back(light1);

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
    const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};
    mb.setDevice(device);
    mb.setVertices(vertices);
    mb.setIndices(indices);
    std::shared_ptr<Mesh> mesh2 = mb.build();

    const std::vector<unsigned char> imagePixels = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 0, 255, 255};
    td.createSRGBTextureBuilder(tb);
    tb.setDevice(device);
    tb.setImageData(imagePixels);
    tb.setWidth(2);
    tb.setHeight(2);
    mesh2->setTexture(tb.build());

    m_objects.push_back(mesh2);
}