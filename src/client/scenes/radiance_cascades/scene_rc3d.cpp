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

#include "render_graphs/radiance_cascades/graph_rc3d.hpp"

#include "scripts/move_camera.hpp"
#include "scripts/radiance_cascades3d.hpp"

#include "wsi/window.hpp"

#include "scene_rc3d.hpp"

void SceneRC3D::load(std::weak_ptr<Context> cx, std::weak_ptr<Device> device, WindowGLFW *window,
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

        auto radianceCascadesScript = std::make_unique<RadianceCascades3D>();
        RadianceCascades3D::init_data init{
            .device = device,
            .frameInFlightCount = frameInFlightCount,
        };
        radianceCascadesScript->init(&init);
        m_scripts.push_back(std::move(radianceCascadesScript));

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

        std::shared_ptr<DirectionalLight> light2 = std::make_shared<DirectionalLight>();
        light2->direction = glm::vec3(1.0, 1.0, 1.0);
        light2->diffuseColor = glm::vec3(1.0, 1.0, 1.0);
        light2->diffusePower = 1.0;
        light2->specularColor = glm::vec3(1.0);
        light2->specularPower = 1.0;
        m_lights.push_back(light2);
    }

    GraphRC3D *rg = dynamic_cast<GraphRC3D *>(renderGraph);
    // load objects into render graph
    {

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

        UniformDescriptorBuilder phongMaterialUdb;
        phongMaterialUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });

        PipelineBuilder<PipelineTypeE::GRAPHICS> phongPb;
        phongPb.setDevice(device);
        phongPb.addVertexShaderStage("simple");
        phongPb.addFragmentShaderStage("forward/phong");
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

        for (int i = 0; i < m_objects.size(); ++i)
        {
            ModelRenderStateBuilder mrsb;
            mrsb.setFrameInFlightCount(frameInFlightCount);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxProbeCount);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            mrsb.setDevice(device);
            mrsb.setModel(m_objects[i]);

            mrsb.setPipeline(phongPipeline);

            rg->m_opaquePhase->registerRenderStateToAllPool(RENDER_STATE_PTR(mrsb.build()));
        }

        std::shared_ptr<Pipeline> probeGridDebugPipeline0 = nullptr;
        std::shared_ptr<Pipeline> probeGridDebugPipeline1 = nullptr;
        std::shared_ptr<Pipeline> probeGridDebugPipeline2 = nullptr;

        {
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
            probeGridDebugPb.addFragmentShaderStage("forward/red");
            probeGridDebugPb.setRenderPass(rg->m_probesDebugPhase->getRenderPass());
            probeGridDebugPb.setExtent(window->getSwapChain()->getExtent());
            probeGridDebugPb.addPushConstantRange(VkPushConstantRange{
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .offset = 0,
                .size = 16,
            });

            PipelineDirector<PipelineTypeE::GRAPHICS> probeGridDebugPd;
            probeGridDebugPd.configureColorDepthRasterizerBuilder(probeGridDebugPb);
            // probeGridDebugPb.setPolygonMode(VK_POLYGON_MODE_LINE);
            probeGridDebugPb.addUniformDescriptorPack(probeGridDebugUdb.buildAndRestart());

            probeGridDebugPipeline0 = probeGridDebugPb.build();
        }
        {
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
            probeGridDebugPb.addFragmentShaderStage("forward/green");
            probeGridDebugPb.setRenderPass(rg->m_probesDebugPhase->getRenderPass());
            probeGridDebugPb.setExtent(window->getSwapChain()->getExtent());
            probeGridDebugPb.addPushConstantRange(VkPushConstantRange{
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .offset = 0,
                .size = 16,
            });

            PipelineDirector<PipelineTypeE::GRAPHICS> probeGridDebugPd;
            probeGridDebugPd.configureColorDepthRasterizerBuilder(probeGridDebugPb);
            // probeGridDebugPb.setPolygonMode(VK_POLYGON_MODE_LINE);
            probeGridDebugPb.addUniformDescriptorPack(probeGridDebugUdb.buildAndRestart());

            probeGridDebugPipeline1 = probeGridDebugPb.build();
        }
        {
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
            probeGridDebugPb.addFragmentShaderStage("forward/blue");
            probeGridDebugPb.setRenderPass(rg->m_probesDebugPhase->getRenderPass());
            probeGridDebugPb.setExtent(window->getSwapChain()->getExtent());
            probeGridDebugPb.addPushConstantRange(VkPushConstantRange{
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                .offset = 0,
                .size = 16,
            });

            PipelineDirector<PipelineTypeE::GRAPHICS> probeGridDebugPd;
            probeGridDebugPd.configureColorDepthRasterizerBuilder(probeGridDebugPb);
            // probeGridDebugPb.setPolygonMode(VK_POLYGON_MODE_LINE);
            probeGridDebugPb.addUniformDescriptorPack(probeGridDebugUdb.buildAndRestart());

            probeGridDebugPipeline2 = probeGridDebugPb.build();
        }

        // probes
        auto s = getReadOnlyInstancedComponents<RadianceCascades3D>();
        m_grid0 = std::make_unique<ProbeGrid>();
        m_grid0->setProbesForce(s[0]->probePositions[0]);
        m_grid0->instanceCountOverride = s[0]->probePositions[0].size();
        m_grid1 = std::make_unique<ProbeGrid>();
        m_grid1->setProbesForce(s[0]->probePositions[1]);
        m_grid1->instanceCountOverride = s[0]->probePositions[1].size();
        m_grid2 = std::make_unique<ProbeGrid>();
        m_grid2->setProbesForce(s[0]->probePositions[2]);
        m_grid2->instanceCountOverride = s[0]->probePositions[2].size();

        MeshDirector md;
        MeshBuilder sphereMb;
        md.createSphereMeshBuilder(sphereMb, 0.5f, 50, 50);
        sphereMb.setDevice(device);
        std::shared_ptr<Mesh> sphereMesh = sphereMb.buildAndRestart();

        MeshBuilder cubeMb;
        md.createCubeMeshBuilder(cubeMb, glm::vec3(0.5));
        cubeMb.setDevice(device);
        std::shared_ptr<Mesh> cubeMesh = cubeMb.buildAndRestart();

        ProbeGridRenderStateBuilder prsb0;
        prsb0.setFrameInFlightCount(frameInFlightCount);
        prsb0.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        prsb0.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxProbeCount);
        prsb0.setDevice(device);
        prsb0.setPipeline(probeGridDebugPipeline0);
        prsb0.setProbeGrid(m_grid0);
        prsb0.setMesh(cubeMesh);
        rg->m_probesDebugPhase->registerRenderStateToAllPool(RENDER_STATE_PTR(prsb0.build()));
        ProbeGridRenderStateBuilder prsb1;
        prsb1.setFrameInFlightCount(frameInFlightCount);
        prsb1.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        prsb1.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxProbeCount);
        prsb1.setDevice(device);
        prsb1.setPipeline(probeGridDebugPipeline1);
        prsb1.setProbeGrid(m_grid1);
        prsb1.setMesh(sphereMesh);
        rg->m_probesDebugPhase->registerRenderStateToAllPool(RENDER_STATE_PTR(prsb1.build()));
        ProbeGridRenderStateBuilder prsb2;
        prsb2.setFrameInFlightCount(frameInFlightCount);
        prsb2.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        prsb2.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxProbeCount);
        prsb2.setDevice(device);
        prsb2.setPipeline(probeGridDebugPipeline2);
        prsb2.setProbeGrid(m_grid2);
        prsb2.setMesh(cubeMesh);
        rg->m_probesDebugPhase->registerRenderStateToAllPool(RENDER_STATE_PTR(prsb2.build()));

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

        if (m_skybox)
        {
            SkyboxRenderStateBuilder srsb;
            srsb.setFrameInFlightCount(frameInFlightCount);
            srsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            srsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            srsb.setDevice(device);
            srsb.setSkybox(m_skybox);
            srsb.setTexture(m_skybox->getTexture());
            srsb.setPipeline(skyboxPipeline);
            rg->m_skyboxPhase->registerRenderStateToAllPool(RENDER_STATE_PTR(srsb.build()));
        }

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
        modelBuilder.setName("screen quad");
        m_screen = modelBuilder.build();
        ModelRenderStateBuilder quadRsb;
        quadRsb.setDevice(device);
        quadRsb.setProbeDescriptorEnable(false);
        quadRsb.setLightDescriptorEnable(false);
        quadRsb.setTextureDescriptorEnable(false);
        quadRsb.setMVPDescriptorEnable(false);
        quadRsb.setPushViewPositionEnable(false);
        quadRsb.setFrameInFlightCount(frameInFlightCount);
        quadRsb.setModel(m_screen);
        quadRsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        quadRsb.setInstanceDescriptorSetUpdatePredPerFrame([=](const RenderPhase *parentPhase, VkCommandBuffer cmd,
                                                               const GPUStateI *self, const VkDescriptorSet &set,
                                                               uint32_t backBufferIndex) {
            const auto &sampler = window->getSwapChain()->getSampler();
            if (!sampler.has_value())
                return;

            VkDescriptorImageInfo imageInfo = {
                .sampler = *sampler.value(),
                .imageView = rg->m_skyboxPhase->getMostRecentRenderedImage().second,
                .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
            };
            VkWriteDescriptorSet write = {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = set,
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .pImageInfo = &imageInfo,
            };
            vkUpdateDescriptorSets(deviceHandle, 1, &write, 0, nullptr);
        });
        PipelineBuilder<PipelineTypeE::GRAPHICS> postProcessPb;
        PipelineDirector<PipelineTypeE::GRAPHICS> postProcessPd;
        postProcessPd.configureColorDepthRasterizerBuilder(postProcessPb);
        postProcessPb.setDevice(device);
        postProcessPb.setRenderPass(rg->m_finalImageDirect->getRenderPass());
        postProcessPb.addVertexShaderStage("pp/screen");
        postProcessPb.addFragmentShaderStage("pp/final_image");
        postProcessPb.setExtent(window->getSwapChain()->getExtent());
        postProcessPb.setDepthTestEnable(VK_FALSE);
        postProcessPb.setDepthWriteEnable(VK_FALSE);
        postProcessPb.setBlendEnable(VK_FALSE);
        postProcessPb.setFrontFace(VK_FRONT_FACE_CLOCKWISE);
        UniformDescriptorBuilder postProcessUdb;
        postProcessUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
        postProcessPb.addUniformDescriptorPack(postProcessUdb.buildAndRestart());
        quadRsb.setPipeline(postProcessPb.build());
        rg->m_finalImageDirect->registerRenderStateToAllPool(RENDER_STATE_PTR(quadRsb.build()));

        {
            PipelineBuilder<PipelineTypeE::COMPUTE> pb;
            PipelineDirector<PipelineTypeE::COMPUTE> pd;
            pd.configureComputeBuilder(pb);
            pb.setDevice(device);
            pb.addComputeShaderStage("rc/radiance_gather_3drt");
            UniformDescriptorBuilder udb;
            // rendered image
            udb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            });
            udb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            });
            // cascade desc buffer
            udb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            });
            // cascade probes position buffer
            udb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
                .binding = 3,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            });
            // radiance interval storage buffer
            udb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
                .binding = 4,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            });
            pb.addUniformDescriptorPack(udb.buildAndRestart());
            ComputeStateBuilder csb;
            csb.setDevice(device);
            csb.setFrameInFlightCount(frameInFlightCount);
            csb.setPipeline(pb.build());
            csb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            csb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            csb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            csb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            csb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            auto s = getReadOnlyInstancedComponents<RadianceCascades3D>();
            if (!s.empty())
            {
                auto rc = s[0];
                csb.setWorkGroup(glm::ivec3(rc->getCascadeCount(), 1, 1));
            }
            csb.setDescriptorSetUpdatePredPerFrame([=](const RenderPhase *parentPhase, VkCommandBuffer cmd,
                                                       const GPUStateI *self, const VkDescriptorSet set,
                                                       uint32_t backBufferIndex) {
                const auto &sampler = window->getSwapChain()->getSampler();
                if (!sampler.has_value())
                    return;

                VkDescriptorImageInfo imageInfo = {
                    .sampler = *sampler.value(),
                    .imageView = rg->m_finalImageDirect->getMostRecentRenderedImage().second,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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
            csb.setDescriptorSetUpdatePred(
                [&](const RenderPhase *parentPhase, const VkDescriptorSet set, uint32_t backBufferIndex) {
                    auto s = getReadOnlyInstancedComponents<RadianceCascades3D>();
                    std::vector<VkWriteDescriptorSet> writes;
                    if (!s.empty())
                    {
                        auto rc = s[0];

                        {
                            VkDescriptorBufferInfo bufferInfo = {
                                .buffer = rc->getParametersBufferHandle()->getHandle(),
                                .offset = 0,
                                .range = rc->getParametersBufferHandle()->getSize(),
                            };
                            writes.push_back(VkWriteDescriptorSet{
                                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = set,
                                .dstBinding = 1,
                                .dstArrayElement = 0,
                                .descriptorCount = 1,
                                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                .pBufferInfo = &bufferInfo,
                            });
                        }
                        {
                            VkDescriptorBufferInfo bufferInfo = {
                                .buffer = rc->getCascadesDescBufferHandle()->getHandle(),
                                .offset = 0,
                                .range = rc->getCascadesDescBufferHandle()->getSize(),
                            };
                            writes.push_back(VkWriteDescriptorSet{
                                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = set,
                                .dstBinding = 2,
                                .dstArrayElement = 0,
                                .descriptorCount = 1,
                                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                .pBufferInfo = &bufferInfo,
                            });
                        }
                        {
                            VkDescriptorBufferInfo bufferInfo = {
                                .buffer = rc->getProbePositionsBufferHandle()->getHandle(),
                                .offset = 0,
                                .range = rc->getProbePositionsBufferHandle()->getSize(),
                            };
                            writes.push_back(VkWriteDescriptorSet{
                                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = set,
                                .dstBinding = 3,
                                .dstArrayElement = 0,
                                .descriptorCount = 1,
                                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                .pBufferInfo = &bufferInfo,
                            });
                        }
                        {
                            VkDescriptorBufferInfo bufferInfo = {
                                .buffer = rc->getRadianceIntervalsStorageBufferHandle(backBufferIndex)->getHandle(),
                                .offset = 0,
                                .range = rc->getRadianceIntervalsStorageBufferHandle(backBufferIndex)->getSize(),
                            };
                            writes.push_back(VkWriteDescriptorSet{
                                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = set,
                                .dstBinding = 4,
                                .dstArrayElement = 0,
                                .descriptorCount = 1,
                                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                .pBufferInfo = &bufferInfo,
                            });
                        }
                    }
                    vkUpdateDescriptorSets(deviceHandle, writes.size(), writes.data(), 0, nullptr);
                });
            rg->m_computePhase->registerComputeState(COMPUTE_STATE_PTR(csb.build()));
        }

        {
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
            rsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            rsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            rsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            rsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            rsb.setInstanceDescriptorSetUpdatePredPerFrame([=](const RenderPhase *parentPhase, VkCommandBuffer cmd,
                                                               const GPUStateI *self, const VkDescriptorSet set,
                                                               uint32_t backBufferIndex) {
                const auto &sampler = window->getSwapChain()->getSampler();
                if (!sampler.has_value())
                    return;

                auto mostRecentImage = rg->m_finalImageDirect->getMostRecentRenderedImage();
                assert(mostRecentImage.first.has_value());

                ImageLayoutTransitionBuilder iltb;
                ImageLayoutTransitionDirector iltd;
                iltd.configureBuilder<VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR>(
                    iltb);
                VkImage im = mostRecentImage.first.value();
                iltb.setImageHandle(im, VK_IMAGE_ASPECT_COLOR_BIT);
                auto transitionPtr = iltb.buildAndRestart();
                ImageLayoutTransition transition = *transitionPtr;
                vkCmdPipelineBarrier(cmd, transition.srcStageMask, transition.dstStageMask, 0, 0, nullptr, 0, nullptr,
                                     1, &transition.barrier);

                VkDescriptorImageInfo imageInfo = {
                    .sampler = *sampler.value(),
                    .imageView = mostRecentImage.second,
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
            rsb.setInstanceDescriptorSetUpdatePred(
                [&](const RenderPhase *parentPhase, const VkDescriptorSet set, uint32_t backBufferIndex) {
                    std::vector<VkWriteDescriptorSet> writes;
                    auto s = getReadOnlyInstancedComponents<RadianceCascades3D>();
                    if (!s.empty())
                    {
                        auto rc = s[0];

                        {
                            VkDescriptorBufferInfo bufferInfo = {
                                .buffer = rc->getParametersBufferHandle()->getHandle(),
                                .offset = 0,
                                .range = rc->getParametersBufferHandle()->getSize(),
                            };
                            writes.push_back(VkWriteDescriptorSet{
                                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = set,
                                .dstBinding = 1,
                                .dstArrayElement = 0,
                                .descriptorCount = 1,
                                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                .pBufferInfo = &bufferInfo,
                            });
                        }
                        {
                            VkDescriptorBufferInfo bufferInfo = {
                                .buffer = rc->getCascadesDescBufferHandle()->getHandle(),
                                .offset = 0,
                                .range = rc->getCascadesDescBufferHandle()->getSize(),
                            };
                            writes.push_back(VkWriteDescriptorSet{
                                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = set,
                                .dstBinding = 2,
                                .dstArrayElement = 0,
                                .descriptorCount = 1,
                                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                .pBufferInfo = &bufferInfo,
                            });
                        }
                        {
                            VkDescriptorBufferInfo bufferInfo = {
                                .buffer = rc->getProbePositionsBufferHandle()->getHandle(),
                                .offset = 0,
                                .range = rc->getProbePositionsBufferHandle()->getSize(),
                            };
                            writes.push_back(VkWriteDescriptorSet{
                                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = set,
                                .dstBinding = 3,
                                .dstArrayElement = 0,
                                .descriptorCount = 1,
                                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                .pBufferInfo = &bufferInfo,
                            });
                        }
                        {
                            VkDescriptorBufferInfo bufferInfo = {
                                .buffer = rc->getRadianceIntervalsStorageBufferHandle(backBufferIndex)->getHandle(),
                                .offset = 0,
                                .range = rc->getRadianceIntervalsStorageBufferHandle(backBufferIndex)->getSize(),
                            };
                            writes.push_back(VkWriteDescriptorSet{
                                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                .dstSet = set,
                                .dstBinding = 4,
                                .dstArrayElement = 0,
                                .descriptorCount = 1,
                                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                .pBufferInfo = &bufferInfo,
                            });
                        }
                    }
                    vkUpdateDescriptorSets(deviceHandle, writes.size(), writes.data(), 0, nullptr);
                });
            PipelineBuilder<PipelineTypeE::GRAPHICS> pb;
            PipelineDirector<PipelineTypeE::GRAPHICS> pd;
            pd.configureColorDepthRasterizerBuilder(pb);
            pb.setDevice(device);
            pb.setRenderPass(rg->m_finalImageDirectIndirect->getRenderPass());
            pb.addVertexShaderStage("pp/screen");
            pb.addFragmentShaderStage("deferred/radiance_apply");
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
            udb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            });
            // cascade desc buffer
            udb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            });
            // cascade probes position buffer
            udb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
                .binding = 3,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            });
            // radiance interval storage buffer
            udb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
                .binding = 4,
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            });
            pb.addUniformDescriptorPack(udb.buildAndRestart());
            rsb.setPipeline(pb.build());
            rg->m_finalImageDirectIndirect->registerRenderStateToAllPool(RENDER_STATE_PTR(rsb.build()));
        }
    }
}