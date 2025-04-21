#include "graphics/device.hpp"

#include "renderer/light.hpp"
#include "renderer/mesh.hpp"
#include "renderer/skybox.hpp"
#include "renderer/model.hpp"
#include "renderer/texture.hpp"

#include "engine/camera.hpp"

#include "scripts/move_camera.hpp"

#include "wsi/window.hpp"

#include "sample_scene.hpp"

SampleScene::SampleScene(std::weak_ptr<Device> device, WindowGLFW *window)
{
    m_cameras.emplace_back(std::make_unique<PerspectiveCamera>());
    m_mainCamera = m_cameras[m_cameras.size() - 1].get();

    auto moveCameraScript = std::make_unique<MoveCamera>();
    auto userData = MoveCamera::UserDataT{
        .window = *window,
        .camera = *m_mainCamera,
    };
    moveCameraScript->init(&userData);
    m_scripts.emplace_back(std::move(moveCameraScript));

    TextureDirector td;

    CubemapBuilder ctb;
    td.configureSRGBTextureBuilder(ctb);
    ctb.setDevice(device);
    ctb.setRightTextureFilename("assets/skybox/right.jpg");
    ctb.setLeftTextureFilename("assets/skybox/left.jpg");
    ctb.setTopTextureFilename("assets/skybox/top.jpg");
    ctb.setBottomTextureFilename("assets/skybox/bottom.jpg");
    ctb.setFrontTextureFilename("assets/skybox/front.jpg");
    ctb.setBackTextureFilename("assets/skybox/back.jpg");

    SkyboxBuilder mainSb;
    mainSb.setDevice(device);
    mainSb.setCubemap(ctb.buildAndRestart());
    m_skybox = mainSb.buildAndRestart();

    ModelBuilder modelBuilder;
    modelBuilder.setDevice(device);
    modelBuilder.setModelFilename("assets/Sponza-master/sponza.glb");
    modelBuilder.setName("Sponza");
    std::shared_ptr<Model> loadedModel = modelBuilder.build();
    Transform loadedModelTransform;
    loadedModelTransform.scale = glm::vec3(0.025f);
    loadedModel->setTransform(loadedModelTransform);

    m_objects.push_back(loadedModel);
    std::shared_ptr<PointLight> light = std::make_shared<PointLight>();
    light->position = glm::vec3(-4.0, 1.0, 0.0);
    light->attenuation = glm::vec3(0.0, 0.0, 1.0);
    light->diffuseColor = glm::vec3(1.0, 0.0, 0.0);
    light->diffusePower = 5.0;
    light->specularColor = glm::vec3(1.0);
    light->specularPower = 1.0;
    m_lights.push_back(light);

    std::shared_ptr<PointLight> light1 = std::make_shared<PointLight>();
    light1->position = glm::vec3(1.0, 1.0, 3.0);
    light->attenuation = glm::vec3(0.0, 0.0, 1.0);
    light1->diffuseColor = glm::vec3(0.0, 0.0, 1.0);
    light1->diffusePower = 5.0;
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
    const std::vector<uint16_t> indices = {
        0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4,
    };

    MeshBuilder mb;
    MeshDirector md;
    mb.setDevice(device);
    mb.setVertices(vertices);
    mb.setIndices(indices);
    std::shared_ptr<Mesh> mesh2 = mb.buildAndRestart();

    const std::vector<unsigned char> imagePixels = {
        255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 0, 255, 255,
    };

    TextureBuilder tb;
    td.configureSRGBTextureBuilder(tb);
    tb.setDevice(device);
    tb.setImageData(imagePixels);
    tb.setWidth(2);
    tb.setHeight(2);
    mesh2->setTexture(tb.buildAndRestart());

    ModelBuilder modelBuilder2;
    modelBuilder2.setMesh(mesh2);
    modelBuilder2.setName("Planes");

    m_objects.push_back(modelBuilder2.build());

    MeshBuilder sphereMb;
    md.createAssimpMeshBuilder(sphereMb);
    sphereMb.setDevice(device);
    sphereMb.setModelFilename("assets/sphere.obj");
    std::shared_ptr<Mesh> sphereMesh = sphereMb.buildAndRestart();

    ModelBuilder sphereModelBuilder;
    sphereModelBuilder.setMesh(sphereMesh);
    sphereModelBuilder.setName("Sphere");

    m_objects.push_back(sphereModelBuilder.build());


    MeshBuilder cubeMb;
    md.createAssimpMeshBuilder(cubeMb);
    cubeMb.setDevice(device);
    cubeMb.setModelFilename("assets/cube.obj");
    std::shared_ptr<Mesh> cubeMesh = cubeMb.buildAndRestart();

    ModelBuilder cubeModelBuilder;
    cubeModelBuilder.setMesh(cubeMesh);
    cubeModelBuilder.setName("Cube");

    m_objects.push_back(cubeModelBuilder.build());
}