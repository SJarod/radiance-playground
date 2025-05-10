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
#include "engine/uniform.hpp"

#include "render_graphs/compute_pp_graph.hpp"

#include "wsi/window.hpp"

#include "scripts/radiance_cascades.hpp"

#include "sample_scene_2d.hpp"

void SampleScene2D::load(std::weak_ptr<Context> cx, std::weak_ptr<Device> device, WindowGLFW *window,
                         RenderGraph *renderGraph, uint32_t frameInFlightCount, uint32_t maxProbeCount)
{
    auto devicePtr = device.lock();
    VkDevice deviceHandle = devicePtr->getHandle();

    // load objects
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
            .frameInFlightCount = frameInFlightCount,
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
        mb.setName("Square mesh");
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
        modelBuilder.setName("Square");
        modelBuilder.setMesh(mesh);

        m_objects.push_back(modelBuilder.build());
    }

    ComputeGraph *rg = dynamic_cast<ComputeGraph *>(renderGraph);
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

        PipelineBuilder<PipelineType::GRAPHICS> phongPb;
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

        PipelineDirector<PipelineType::GRAPHICS> phongPd;
        phongPd.configureColorDepthRasterizerBuilder(phongPb);
        phongPb.addUniformDescriptorPack(phongInstanceUdb.buildAndRestart());
        phongPb.addUniformDescriptorPack(phongMaterialUdb.buildAndRestart());

        std::shared_ptr<Pipeline> phongPipeline = phongPb.build();

        const std::vector<unsigned char> defaultDiffusePixels = {
            178, 0, 255, 255, 0, 0, 0, 255, 0, 0, 0, 0, 178, 0, 255, 255,
        };

        for (int i = 0; i < m_objects.size(); ++i)
        {
            ModelRenderStateBuilder mrsb;
            mrsb.setFrameInFlightCount(frameInFlightCount);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
            mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            mrsb.setProbeDescriptorEnable(false);
            mrsb.setDevice(device);

            mrsb.setModel(m_objects[i]);

            mrsb.setPipeline(phongPipeline);

            rg->m_opaquePhase->registerRenderStateToAllPool(RENDER_STATE_PTR(mrsb.build()));
        }

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

        PipelineBuilder<PipelineType::GRAPHICS> skyboxPb;
        PipelineDirector<PipelineType::GRAPHICS> skyboxPd;
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
            rsb.setInstanceDescriptorSetUpdatePredPerFrame(
                [this, window, rg, deviceHandle](const RenderPhase *parentPhase, const VkDescriptorSet set, uint32_t backBufferIndex) {
                    const auto &sampler = window->getSwapChain()->getSampler();
                    if (!sampler.has_value())
                        return;

                    VkDescriptorImageInfo imageInfo = {
                        .sampler = *sampler.value(),
                        .imageView = rg->m_skyboxPhase->getMostRecentRenderedImage(),
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
            PipelineBuilder<PipelineType::GRAPHICS> pb;
            PipelineDirector<PipelineType::GRAPHICS> pd;
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

        {
            PipelineBuilder<PipelineType::COMPUTE> pb;
            PipelineDirector<PipelineType::COMPUTE> pd;
            pd.configureComputeBuilder(pb);
            pb.setDevice(device);
            pb.addComputeShaderStage("radiance_gather");
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
            auto s = getReadOnlyInstancedComponents<RadianceCascades>();
            if (!s.empty())
            {
                auto rc = s[0];
                csb.setWorkGroup(glm::ivec3(rc->getCascadeCount(), 1, 1));
            }
            csb.setDescriptorSetUpdatePredPerFrame(
                [=](const RenderPhase *parentPhase, const VkDescriptorSet set, uint32_t backBufferIndex) {
                    const auto &sampler = window->getSwapChain()->getSampler();
                    if (!sampler.has_value())
                        return;

                    VkDescriptorImageInfo imageInfo = {
                        .sampler = *sampler.value(),
                        .imageView = rg->m_finalImageDirect->getMostRecentRenderedImage(),
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
                    auto s = getReadOnlyInstancedComponents<RadianceCascades>();
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
            rsb.setInstanceDescriptorSetUpdatePredPerFrame(
                [=](const RenderPhase *parentPhase, const VkDescriptorSet set, uint32_t backBufferIndex) {
                    const auto &sampler = window->getSwapChain()->getSampler();
                    if (!sampler.has_value())
                        return;

                    VkDescriptorImageInfo imageInfo = {
                        .sampler = *sampler.value(),
                        .imageView = rg->m_finalImageDirect->getMostRecentRenderedImage(),
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
            rsb.setInstanceDescriptorSetUpdatePred(
                [&](const RenderPhase *parentPhase, const VkDescriptorSet set, uint32_t backBufferIndex) {
                    std::vector<VkWriteDescriptorSet> writes;
                    auto s = getReadOnlyInstancedComponents<RadianceCascades>();
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
            PipelineBuilder<PipelineType::GRAPHICS> pb;
            PipelineDirector<PipelineType::GRAPHICS> pd;
            pd.configureColorDepthRasterizerBuilder(pb);
            pb.setDevice(device);
            pb.setRenderPass(rg->m_finalImageDirectIndirect->getRenderPass());
            pb.addVertexShaderStage("screen");
            pb.addFragmentShaderStage("final_image_direct_indirect");
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

    {
        ImGuiRenderStateBuilder imguirsb;

        imguirsb.setDevice(device);
        imguirsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        ImGui::CreateContext();
        if (!ImGui_ImplGlfw_InitForVulkan(window->getHandle(), true))
        {
            std::cerr << "Failed to initialize ImGui GLFW Implemenation For Vulkan" << std::endl;
            throw;
        }

        std::shared_ptr<RenderStateABC> render_state = RENDER_STATE_PTR(imguirsb.build());
        rg->m_imguiPhase->registerRenderStateToAllPool(render_state);

        // this initializes imgui for Vulkan
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = cx.lock()->getInstanceHandle();
        init_info.PhysicalDevice = devicePtr->getPhysicalHandle();
        init_info.Device = deviceHandle;
        init_info.Queue = devicePtr->getGraphicsQueue();
        init_info.DescriptorPool = render_state->getDescriptorPool();
        init_info.MinImageCount = 2;
        init_info.ImageCount = 2;
        init_info.RenderPass = rg->m_imguiPhase->getRenderPass()->getHandle();

        if (!ImGui_ImplVulkan_Init(&init_info))
        {
            std::cerr << "Failed to initialize ImGui Implementation for Vulkan" << std::endl;
            throw;
        }
    }
}