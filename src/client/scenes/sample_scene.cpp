#include <iostream>

#include <vulkan/vulkan.hpp>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include "graphics/context.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"

#include "renderer/light.hpp"
#include "renderer/mesh.hpp"
#include "renderer/model.hpp"
#include "renderer/render_graph.hpp"
#include "renderer/render_phase.hpp"
#include "renderer/render_state.hpp"
#include "renderer/skybox.hpp"
#include "renderer/texture.hpp"

#include "engine/camera.hpp"
#include "engine/probe_grid.hpp"
#include "engine/uniform.hpp"

#include "render_graphs/irradiance_baked_graph.hpp"

#include "scripts/move_camera.hpp"

#include "wsi/window.hpp"

#include "sample_scene.hpp"

void SampleScene::load(std::weak_ptr<Context> cx, std::weak_ptr<Device> device, WindowGLFW *window,
                       RenderGraph *renderGraph, uint32_t frameInFlightCount, uint32_t maxProbeCount)
{
    auto devicePtr = device.lock();
    VkDevice deviceHandle = devicePtr->getHandle();

    // load objects
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
        light->attenuation = glm::vec3(0.0, 0.2, 0.0);
        light->diffuseColor = glm::vec3(1.0, 0.0, 0.0);
        light->diffusePower = 5.0;
        light->specularColor = glm::vec3(1.0);
        light->specularPower = 1.0;
        m_lights.push_back(light);

        std::shared_ptr<PointLight> light1 = std::make_shared<PointLight>();
        light1->position = glm::vec3(25.0, 1.0, 3.0);
        light1->attenuation = glm::vec3(0.0, 0.2, 0.0);
        light1->diffuseColor = glm::vec3(0.0, 0.0, 1.0);
        light1->diffusePower = 5.0;
        light1->specularColor = glm::vec3(1.0);
        light1->specularPower = 1.0;
        m_lights.push_back(light1);

        std::shared_ptr<DirectionalLight> light2 = std::make_shared<DirectionalLight>();
        light2->direction = glm::vec3(1.0, 1.0, 1.0);
        light2->diffuseColor = glm::vec3(1.0, 1.0, 1.0);
        light2->diffusePower = 1.0;
        light2->specularColor = glm::vec3(1.0);
        light2->specularPower = 1.0;
        m_lights.push_back(light2);

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
        tb.setName("Square texture 4 color");
        mesh2->setTexture(tb.buildAndRestart());

        ModelBuilder modelBuilder2;
        modelBuilder2.setMesh(mesh2);
        modelBuilder2.setName("Planes");

        m_objects.push_back(modelBuilder2.build());

        MeshBuilder sphereMb;
        md.createSphereMeshBuilder(sphereMb, 1.f, 50, 50);
        sphereMb.setDevice(device);
        std::shared_ptr<Mesh> sphereMesh = sphereMb.buildAndRestart();

        ModelBuilder sphereModelBuilder;
        sphereModelBuilder.setMesh(sphereMesh);
        sphereModelBuilder.setName("Sphere");
        std::shared_ptr<Model> sphereModel = sphereModelBuilder.build();
        Transform sphereTransform = sphereModel->getTransform();
        sphereTransform.position = glm::vec3(1.f, 1.f, 0.f);
        sphereModel->setTransform(sphereTransform);
        m_objects.push_back(sphereModel);

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

    BakedGraph *rg = dynamic_cast<BakedGraph *>(renderGraph);
    // load objects into render graph
    {
        UniformDescriptorBuilder irradianceConvolutionUdb;

        irradianceConvolutionUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        });

        irradianceConvolutionUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });

        PipelineBuilder<PipelineTypeE::GRAPHICS> irradianceConvolutionPb;
        irradianceConvolutionPb.setDevice(device);
        irradianceConvolutionPb.addVertexShaderStage("skybox");
        irradianceConvolutionPb.addFragmentShaderStage("irradiance_convolution");
        irradianceConvolutionPb.setRenderPass(rg->m_irradianceConvolutionPhase->getRenderPass());
        irradianceConvolutionPb.setExtent(window->getSwapChain()->getExtent());

        PipelineDirector<PipelineTypeE::GRAPHICS> irradianceConvolutionPd;
        irradianceConvolutionPd.configureColorDepthRasterizerBuilder(irradianceConvolutionPb);
        irradianceConvolutionPb.addUniformDescriptorPack(irradianceConvolutionUdb.buildAndRestart());

        std::shared_ptr<Pipeline> irradianceConvolutionPipeline = irradianceConvolutionPb.build();

        // material
        UniformDescriptorBuilder phongInstanceUdb;
        phongInstanceUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        });
        phongInstanceUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
        phongInstanceUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
        phongInstanceUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = maxProbeCount,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
        phongInstanceUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 5,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
        phongInstanceUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 6,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
#ifdef USE_NV_PRO_CORE
            .descriptorCount = 1, // number of tlas
#else
            .descriptorCount = static_cast<uint32_t>(m_objects.size()), // number of tlas
#endif
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });

        UniformDescriptorBuilder phongMaterialUdb;
        phongMaterialUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });

        PipelineBuilder<PipelineTypeE::GRAPHICS> phongPb;
        phongPb.setDevice(device);
        phongPb.addVertexShaderStage("phong");
        phongPb.addFragmentShaderStage("phong");
        phongPb.setRenderPass(rg->m_opaquePhase->getRenderPass());
        phongPb.setExtent(window->getSwapChain()->getExtent());
        phongPb.addPushConstantRange(VkPushConstantRange{
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = 16,
        });

        PipelineDirector<PipelineTypeE::GRAPHICS> phongPd;
        phongPd.configureColorDepthRasterizerBuilder(phongPb);
        phongPb.addUniformDescriptorPack(phongInstanceUdb.buildAndRestart());
        phongPb.addUniformDescriptorPack(phongMaterialUdb.buildAndRestart());

        std::shared_ptr<Pipeline> phongPipeline = phongPb.build();

        UniformDescriptorBuilder phongCaptureInstanceUdb;
        phongCaptureInstanceUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        });
        phongCaptureInstanceUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
        phongCaptureInstanceUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 3,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
        phongCaptureInstanceUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = maxProbeCount,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
        phongCaptureInstanceUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 5,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
        phongCaptureInstanceUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 6,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = 1, // number of tlas in this phase (1)
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });

        UniformDescriptorBuilder phongCaptureMaterialUdb;
        phongCaptureMaterialUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });

        PipelineBuilder<PipelineTypeE::GRAPHICS> phongCapturePb;
        phongCapturePb.setDevice(device);
        phongCapturePb.addVertexShaderStage("phong");
        phongCapturePb.addFragmentShaderStage("phong");
        phongCapturePb.setRenderPass(rg->m_opaqueCapturePhase->getRenderPass());
        phongCapturePb.setExtent(window->getSwapChain()->getExtent());
        phongCapturePb.addPushConstantRange(VkPushConstantRange{
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = 16,
        });

        PipelineDirector<PipelineTypeE::GRAPHICS> phongCapturePd;
        phongCapturePd.configureColorDepthRasterizerBuilder(phongCapturePb);
        phongCapturePb.addUniformDescriptorPack(phongCaptureInstanceUdb.buildAndRestart());
        phongCapturePb.addUniformDescriptorPack(phongCaptureMaterialUdb.buildAndRestart());

        std::shared_ptr<Pipeline> phongCapturePipeline = phongCapturePb.build();

        UniformDescriptorBuilder environmentMapUdb;
        environmentMapUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        });
        environmentMapUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = maxProbeCount,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });

        PipelineBuilder<PipelineTypeE::GRAPHICS> environmentMapPb;
        environmentMapPb.setDevice(device);
        environmentMapPb.addVertexShaderStage("environment_map");
        environmentMapPb.addFragmentShaderStage("environment_map");
        environmentMapPb.setRenderPass(rg->m_skyboxPhase->getRenderPass());
        environmentMapPb.setExtent(window->getSwapChain()->getExtent());
        environmentMapPb.addPushConstantRange(VkPushConstantRange{
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = 16,
        });

        PipelineDirector<PipelineTypeE::GRAPHICS> environmentMapPd;
        environmentMapPd.configureColorDepthRasterizerBuilder(environmentMapPb);
        environmentMapPb.addUniformDescriptorPack(environmentMapUdb.buildAndRestart());

        std::shared_ptr<Pipeline> environmentMapPipeline = environmentMapPb.build();

        UniformDescriptorBuilder environmentMapCaptureUdb;
        environmentMapCaptureUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        });
        environmentMapCaptureUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = maxProbeCount,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });

        PipelineBuilder<PipelineTypeE::GRAPHICS> environmentMapCapturePb;
        environmentMapCapturePb.setDevice(device);
        environmentMapCapturePb.addVertexShaderStage("environment_map");
        environmentMapCapturePb.addFragmentShaderStage("environment_map");
        environmentMapCapturePb.setRenderPass(rg->m_skyboxCapturePhase->getRenderPass());
        environmentMapCapturePb.setExtent(window->getSwapChain()->getExtent());
        environmentMapCapturePb.addPushConstantRange(VkPushConstantRange{
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = 16,
        });

        const std::vector<unsigned char> defaultDiffusePixels = {
            178, 0, 255, 255, 0, 0, 0, 255, 0, 0, 0, 0, 178, 0, 255, 255,
        };

        TextureDirector td;
        TextureBuilder tb;
        td.configureSRGBTextureBuilder(tb);
        tb.setWidth(2);
        tb.setHeight(2);
        tb.setImageData(defaultDiffusePixels);
        tb.setDevice(device);
        tb.setImageData(defaultDiffusePixels);
        tb.setName("defaultDiffusePixels");
        ModelRenderState::s_defaultDiffuseTexture = tb.buildAndRestart();

        PipelineDirector<PipelineTypeE::GRAPHICS> environmentMapCapturePd;
        environmentMapCapturePd.configureColorDepthRasterizerBuilder(environmentMapCapturePb);
        environmentMapCapturePb.addUniformDescriptorPack(environmentMapCaptureUdb.buildAndRestart());

        std::shared_ptr<Pipeline> environmentMapCapturePipeline = environmentMapCapturePb.build();

        for (int i = 0; i < m_objects.size(); ++i)
        {
            ModelRenderStateBuilder mrsb;
            mrsb.setFrameInFlightCount(frameInFlightCount);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxProbeCount);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
#ifdef USE_NV_PRO_CORE
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1);
#else
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, m_objects.size());
#endif

            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

            mrsb.setDevice(device);
            mrsb.setModel(m_objects[i]);
            mrsb.setInstanceDescriptorSetUpdatePredPerFrame([=](const RenderPhase *parentPhase, VkCommandBuffer cmd,
                                                                const VkDescriptorSet &set, uint32_t backBufferIndex) {
                auto tlas = rg->m_opaquePhase->getTLAS();
                VkWriteDescriptorSetAccelerationStructureKHR descASInfo = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                    .accelerationStructureCount = static_cast<uint32_t>(tlas.size()),
                    .pAccelerationStructures = tlas.data(),
                };
                std::vector<VkWriteDescriptorSet> writes;
                writes.push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .pNext = &descASInfo,
                    .dstSet = set,
                    .dstBinding = 6,
                    .dstArrayElement = 0,
                    .descriptorCount = descASInfo.accelerationStructureCount,
                    .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                });

                vkUpdateDescriptorSets(deviceHandle, writes.size(), writes.data(), 0, nullptr);
            });

            // Check if the mesh is the quad, the sphere or the cube
            if (i != 1 && i != 2 && i != 3)
            {
                mrsb.setEnvironmentMaps(rg->m_irradianceMaps);
                mrsb.setPipeline(phongPipeline);

                ModelRenderStateBuilder captureMrsb;
                captureMrsb.setFrameInFlightCount(window->getSwapChain()->getSwapChainImageCount());
                captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
                captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
                captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
                captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxProbeCount);
                captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
                captureMrsb.addPoolSize(
                    VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                    1); // number of tlas in the ray tracing phase (there are as many objects as render states
                        // registered in the phase (one in the occurence of this particular phase))

                captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

                captureMrsb.setDevice(device);
                captureMrsb.setModel(m_objects[i]);
                captureMrsb.setPipeline(phongCapturePipeline);
                captureMrsb.setEnvironmentMaps(rg->m_irradianceMaps);
                captureMrsb.setCaptureCount(maxProbeCount);
                captureMrsb.setCaptureCount(maxProbeCount);
                captureMrsb.setInstanceDescriptorSetUpdatePredPerFrame(
                    [=](const RenderPhase *parentPhase, VkCommandBuffer cmd, const VkDescriptorSet &set,
                        uint32_t backBufferIndex) {
                        auto tlas = rg->m_opaqueCapturePhase->getTLAS();
                        VkWriteDescriptorSetAccelerationStructureKHR descASInfo = {
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                            .accelerationStructureCount = static_cast<uint32_t>(tlas.size()),
                            .pAccelerationStructures = tlas.data(),
                        };
                        std::vector<VkWriteDescriptorSet> writes;
                        writes.push_back(VkWriteDescriptorSet{
                            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                            .pNext = &descASInfo,
                            .dstSet = set,
                            .dstBinding = 6,
                            .dstArrayElement = 0,
                            .descriptorCount = descASInfo.accelerationStructureCount,
                            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                        });

                        vkUpdateDescriptorSets(deviceHandle, writes.size(), writes.data(), 0, nullptr);
                    });

                rg->m_opaqueCapturePhase->registerRenderStateToAllPool(RENDER_STATE_PTR(captureMrsb.build()));
            }
            else
            {
                mrsb.setTextureDescriptorEnable(false);
                mrsb.setProbeDescriptorEnable(false);
                mrsb.setLightDescriptorEnable(false);
                mrsb.setPipeline(environmentMapPipeline);

                mrsb.setEnvironmentMaps(rg->m_irradianceMaps);
            }

            rg->m_opaquePhase->registerRenderStateToAllPool(RENDER_STATE_PTR(mrsb.build()));
        }

        rg->m_opaqueCapturePhase->generateBottomLevelAS();
        rg->m_opaqueCapturePhase->generateTopLevelAS();

        rg->m_opaquePhase->generateBottomLevelAS();
        rg->m_opaquePhase->generateTopLevelAS();

        UniformDescriptorBuilder probeGridDebugUdb;
        probeGridDebugUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        });
        probeGridDebugUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 4,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = maxProbeCount,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
        probeGridDebugUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 5,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        });

        PipelineBuilder<PipelineTypeE::GRAPHICS> probeGridDebugPb;
        probeGridDebugPb.setDevice(device);
        probeGridDebugPb.addVertexShaderStage("probe_grid_debug");
        probeGridDebugPb.addFragmentShaderStage("probe_grid_debug");
        probeGridDebugPb.setRenderPass(rg->m_probesDebugPhase->getRenderPass());
        probeGridDebugPb.setExtent(window->getSwapChain()->getExtent());
        probeGridDebugPb.addPushConstantRange(VkPushConstantRange{
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .offset = 0,
            .size = 16,
        });

        PipelineDirector<PipelineTypeE::GRAPHICS> probeGridDebugPd;
        probeGridDebugPd.configureColorDepthRasterizerBuilder(probeGridDebugPb);
        probeGridDebugPb.addUniformDescriptorPack(probeGridDebugUdb.buildAndRestart());

        std::shared_ptr<Pipeline> probeGridDebugPipeline = probeGridDebugPb.build();

        // probes
        ProbeGridBuilder gridBuilder;
        const glm::vec3 extent = glm::vec3(60.f, 10.f, 20.f);
        const glm::vec3 cornerPosition = glm::vec3(extent.x * -0.5f, 0.f, extent.z * -0.5f);
        gridBuilder.setXAxisProbeCount(4u);
        gridBuilder.setYAxisProbeCount(4u);
        gridBuilder.setZAxisProbeCount(4u);
        gridBuilder.setExtent(extent);
        gridBuilder.setCornerPosition(cornerPosition);
        m_grid = gridBuilder.build();

        MeshDirector md;
        MeshBuilder sphereMb;
        md.createSphereMeshBuilder(sphereMb, 0.5f, 50, 50);
        sphereMb.setDevice(device);
        std::shared_ptr<Mesh> sphereMesh = sphereMb.buildAndRestart();

        MeshBuilder cubeMb;
        md.createCubeMeshBuilder(cubeMb, glm::vec3(0.5));
        cubeMb.setDevice(device);
        std::shared_ptr<Mesh> cubeMesh = cubeMb.buildAndRestart();

        ProbeGridRenderStateBuilder prsb;
        prsb.setFrameInFlightCount(frameInFlightCount);
        prsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        prsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxProbeCount);
        prsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        prsb.setDevice(device);
        prsb.setPipeline(probeGridDebugPipeline);
        prsb.setProbeGrid(m_grid);
        prsb.setEnvironmentMaps(rg->m_irradianceMaps);
        prsb.setMesh(cubeMesh);
        rg->m_probesDebugPhase->registerRenderStateToAllPool(RENDER_STATE_PTR(prsb.build()));

        // skybox
        UniformDescriptorBuilder skyboxUdb;
        skyboxUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        });
        skyboxUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });

        PipelineBuilder<PipelineTypeE::GRAPHICS> skyboxPb;
        PipelineDirector<PipelineTypeE::GRAPHICS> skyboxPd;
        skyboxPd.configureColorDepthRasterizerBuilder(skyboxPb);
        skyboxPb.setDevice(device);
        skyboxPb.addVertexShaderStage("skybox");
        skyboxPb.addFragmentShaderStage("skybox");
        skyboxPb.setRenderPass(rg->m_skyboxPhase->getRenderPass());
        skyboxPb.setExtent(window->getSwapChain()->getExtent());
        skyboxPb.setDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL);

        skyboxPb.addUniformDescriptorPack(skyboxUdb.buildAndRestart());

        std::shared_ptr<Pipeline> skyboxPipeline = skyboxPb.build();

        // skybox
        UniformDescriptorBuilder skyboxOpaqueUdb;
        skyboxOpaqueUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        });
        skyboxOpaqueUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });

        PipelineBuilder<PipelineTypeE::GRAPHICS> skyboxCapturePb;
        PipelineDirector<PipelineTypeE::GRAPHICS> skyboxCapturePd;
        skyboxCapturePd.configureColorDepthRasterizerBuilder(skyboxCapturePb);
        skyboxCapturePb.setDevice(device);
        skyboxCapturePb.addVertexShaderStage("skybox");
        skyboxCapturePb.addFragmentShaderStage("skybox");
        skyboxCapturePb.setRenderPass(rg->m_skyboxCapturePhase->getRenderPass());
        skyboxCapturePb.setExtent(window->getSwapChain()->getExtent());
        skyboxCapturePb.setDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL);

        skyboxCapturePb.addUniformDescriptorPack(skyboxOpaqueUdb.buildAndRestart());

        std::shared_ptr<Pipeline> skyboxCapturePipeline = skyboxCapturePb.build();

        if (m_skybox)
        {
            for (int i = 0; i < maxProbeCount; i++)
            {
                EnvironmentCaptureRenderStateBuilder irsb;
                irsb.setFrameInFlightCount(1);
                irsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
                irsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
                irsb.setDevice(device);
                irsb.setSkybox(m_skybox);
                irsb.setTexture(rg->m_capturedEnvMaps[i]);
                irsb.setPipeline(irradianceConvolutionPipeline);
                rg->m_irradianceConvolutionPhase->registerRenderStateToSpecificPool(RENDER_STATE_PTR(irsb.build()), i);
            }

            SkyboxRenderStateBuilder srsb;
            srsb.setFrameInFlightCount(frameInFlightCount);
            srsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            srsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            srsb.setDevice(device);
            srsb.setSkybox(m_skybox);
            srsb.setTexture(m_skybox->getTexture());
            srsb.setPipeline(skyboxPipeline);
            rg->m_skyboxPhase->registerRenderStateToAllPool(RENDER_STATE_PTR(srsb.build()));

            SkyboxRenderStateBuilder captureSrsb;
            captureSrsb.setFrameInFlightCount(frameInFlightCount);
            captureSrsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            captureSrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            captureSrsb.setDevice(device);
            captureSrsb.setSkybox(m_skybox);
            captureSrsb.setTexture(m_skybox->getTexture());
            captureSrsb.setPipeline(skyboxCapturePipeline);
            captureSrsb.setCaptureCount(maxProbeCount);

            rg->m_skyboxCapturePhase->registerRenderStateToAllPool(RENDER_STATE_PTR(captureSrsb.build()));
        }

        {
            MeshBuilder mb;
            mb.setDevice(device);
            mb.setVertices({{{-1.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 0.0f, 0.0f, 1.f}, {0.f, 0.f}},
                            {{1.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, {0.0f, 1.0f, 0.0f, 1.f}, {1.f, 0.f}},
                            {{1.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {0.0f, 0.0f, 1.0f, 1.f}, {1.f, 1.f}},
                            {{-1.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 1.0f, 1.0f, 1.f}, {0.f, 1.f}}});
            mb.setIndices({0, 1, 2, 2, 3, 0});
            std::shared_ptr<Mesh> postProcessQuadMesh = mb.buildAndRestart();
            ModelBuilder modelBuilder;
            modelBuilder.setMesh(postProcessQuadMesh);
            modelBuilder.setName("post process quad");
            m_screen = modelBuilder.build();
            ModelRenderStateBuilder rsb;
            rsb.setDevice(device);
            rsb.setProbeDescriptorEnable(false);
            rsb.setLightDescriptorEnable(false);
            rsb.setTextureDescriptorEnable(false);
            rsb.setMVPDescriptorEnable(false);
            rsb.setPushViewPositionEnable(false);
            rsb.setFrameInFlightCount(frameInFlightCount);
            rsb.setModel(m_screen);
            rsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            rsb.setInstanceDescriptorSetUpdatePredPerFrame([=](const RenderPhase *parentPhase, VkCommandBuffer cmd,
                                                               const VkDescriptorSet set, uint32_t backBufferIndex) {
                const auto &sampler = window->getSwapChain()->getSampler();
                if (!sampler.has_value())
                    return;

                VkDescriptorImageInfo imageInfo = {
                    .sampler = *sampler.value(),
                    .imageView = rg->m_skyboxPhase->getMostRecentRenderedImage().second,
                    .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
                };
                std::vector<VkWriteDescriptorSet> writes;
                writes.push_back(VkWriteDescriptorSet{
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = set,
                    .dstBinding = 0,
                    .dstArrayElement = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &imageInfo,
                });

                vkUpdateDescriptorSets(deviceHandle, writes.size(), writes.data(), 0, nullptr);
            });
            PipelineBuilder<PipelineTypeE::GRAPHICS> pb;
            PipelineDirector<PipelineTypeE::GRAPHICS> pd;
            pd.configureColorDepthRasterizerBuilder(pb);
            pb.setDevice(device);
            pb.setRenderPass(rg->m_finalImageDirect->getRenderPass());
            pb.addVertexShaderStage("screen");
            pb.addFragmentShaderStage("final_image_direct");
            pb.setExtent(window->getSwapChain()->getExtent());
            pb.setDepthTestEnable(VK_FALSE);
            pb.setDepthWriteEnable(VK_FALSE);
            pb.setBlendEnable(VK_FALSE);
            pb.setFrontFace(VK_FRONT_FACE_CLOCKWISE);
            UniformDescriptorBuilder udb;
            // rendered image
            udb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            });
            pb.addUniformDescriptorPack(udb.buildAndRestart());
            rsb.setPipeline(pb.build());
            rg->m_finalImageDirect->registerRenderStateToAllPool(RENDER_STATE_PTR(rsb.build()));
        }
    }
}