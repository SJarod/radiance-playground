#include <assimp/Importer.hpp>
#include <memory>
#include <string>
#include <format>

#include "graphics/context.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/render_pass.hpp"

#include "wsi/window.hpp"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include "renderer/light.hpp"
#include "renderer/mesh.hpp"
#include "renderer/model.hpp"
#include "renderer/render_graph.hpp"
#include "renderer/render_phase.hpp"
#include "renderer/render_state.hpp"
#include "renderer/renderer.hpp"
#include "renderer/skybox.hpp"
#include "renderer/texture.hpp"

#include "engine/camera.hpp"
#include "engine/probe_grid.hpp"
#include "engine/uniform.hpp"
#include "engine/vertex.hpp"

#include "sample_scene.hpp"
#include "sample_scene_2d.hpp"

#include "application.hpp"

constexpr uint32_t maxProbeCount = 64u;

Application::Application()
{
    WindowGLFW::init();

    m_window = std::make_unique<WindowGLFW>();

    glfwSetKeyCallback(m_window->getHandle(), InputManager::KeyCallback);
    ContextBuilder cb;
    cb.addLayer("VK_LAYER_KHRONOS_validation");
    cb.addLayer("VK_LAYER_LUNARG_monitor");
    cb.addInstanceExtension(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    cb.addInstanceExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    auto requireExtensions = m_window->getRequiredExtensions();
    for (const auto &extension : requireExtensions)
    {
        cb.addInstanceExtension(extension);
    }
    m_context = cb.build();

    m_window->setSurface(
        std::move(std::make_unique<Surface>(m_context, &WindowGLFW::createSurfacePredicate, m_window->getHandle())));

    auto physicalDevices = m_context->getAvailablePhysicalDevices();
    for (auto physicalDevice : physicalDevices)
    {
        DeviceBuilder db;
        db.setContext(m_context);
        db.setPhysicalDevice(physicalDevice);
        db.setSurface(m_window->getSurface());
        db.addDeviceExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        db.addDeviceExtension(VK_KHR_MULTIVIEW_EXTENSION_NAME);
        m_devices.emplace_back(db.build());
    }

    std::weak_ptr<Device> mainDevice = m_devices[0];

    SwapChainBuilder scb;
    scb.setDevice(mainDevice);
    scb.setWidth(1366);
    scb.setHeight(768);
    scb.setSwapchainImageFormat(VkSurfaceFormatKHR{
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    });
    scb.setSwapchainPresentMode(VK_PRESENT_MODE_IMMEDIATE_KHR);
    scb.setUseImagesAsSamplers(true);
    m_window->setSwapChain(scb.build());

    RenderPassAttachmentBuilder rpab;
    RenderPassAttachmentDirector rpad;
    RenderPassDirector rpd;

    TextureDirector td;

    // Capture environment map
    for (int i = 0; i < maxProbeCount; i++)
    {
        CubemapBuilder captureEnvMapBuilder;
        captureEnvMapBuilder.setDevice(mainDevice);
        captureEnvMapBuilder.setWidth(256);
        captureEnvMapBuilder.setHeight(256);
        captureEnvMapBuilder.setCreateFromUserData(false);
        captureEnvMapBuilder.setDepthImageEnable(true);
        td.configureUNORMTextureBuilder(captureEnvMapBuilder);
        capturedEnvMaps.push_back(captureEnvMapBuilder.buildAndRestart());
    }

    // Opaque capture
    RenderPassBuilder opaqueCaptureRpb;
    opaqueCaptureRpb.setDevice(mainDevice);
    rpd.configurePooledCubemapsRenderPassBuilder(opaqueCaptureRpb, capturedEnvMaps, true);

    rpad.configureAttachmentClearBuilder(rpab);
    rpab.setFormat(capturedEnvMaps[0]->getImageFormat());
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    auto opaqueCaptureColorAttachment = rpab.buildAndRestart();
    opaqueCaptureRpb.addColorAttachment(*opaqueCaptureColorAttachment);

    rpad.configureAttachmentClearBuilder(rpab);
    rpab.setFormat(capturedEnvMaps[0]->getDepthImageFormat().value());
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    auto opaqueCaptureDepthAttachment = rpab.buildAndRestart();
    opaqueCaptureRpb.addDepthAttachment(*opaqueCaptureDepthAttachment);

    RenderPhaseBuilder opaqueCaptureRb;
    opaqueCaptureRb.setDevice(mainDevice);
    opaqueCaptureRb.setRenderPass(opaqueCaptureRpb.build());
    opaqueCaptureRb.setCaptureEnable(true);
    auto opaqueCapturePhase = opaqueCaptureRb.build();
    m_opaqueCapturePhase = opaqueCapturePhase.get();

    // Skybox capture
    RenderPassBuilder skyboxCaptureRpb;
    skyboxCaptureRpb.setDevice(mainDevice);
    rpd.configurePooledCubemapsRenderPassBuilder(skyboxCaptureRpb, capturedEnvMaps, true);

    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(capturedEnvMaps[0]->getImageFormat());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto skyboxCaptureColorAttachment = rpab.buildAndRestart();
    skyboxCaptureRpb.addColorAttachment(*skyboxCaptureColorAttachment);

    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(capturedEnvMaps[0]->getDepthImageFormat().value());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    auto skyboxCaptureDepthAttachment = rpab.buildAndRestart();
    skyboxCaptureRpb.addDepthAttachment(*skyboxCaptureDepthAttachment);

    RenderPhaseBuilder skyboxCaptureRb;
    skyboxCaptureRb.setDevice(mainDevice);
    skyboxCaptureRb.setRenderPass(skyboxCaptureRpb.build());
    skyboxCaptureRb.setCaptureEnable(true);
    auto skyboxCapturePhase = skyboxCaptureRb.build();
    m_skyboxCapturePhase = skyboxCapturePhase.get();

    // Irradiance cubemap
    for (int i = 0; i < maxProbeCount; i++)
    {
        CubemapBuilder irradianceMapBuilder;
        irradianceMapBuilder.setDevice(mainDevice);
        irradianceMapBuilder.setWidth(128);
        irradianceMapBuilder.setHeight(128);
        irradianceMapBuilder.setCreateFromUserData(false);
        irradianceMapBuilder.setResolveEnable(true);
        td.configureUNORMTextureBuilder(irradianceMapBuilder);
        irradianceMaps.push_back(irradianceMapBuilder.buildAndRestart());
    }

    // Irradiance convolution
    RenderPassBuilder irradianceConvolutionRpb;
    irradianceConvolutionRpb.setDevice(mainDevice);
    rpd.configurePooledCubemapsRenderPassBuilder(irradianceConvolutionRpb, irradianceMaps, true, false);
    rpad.configureAttachmentDontCareBuilder(rpab);
    rpab.setFormat(irradianceMaps[0]->getImageFormat());
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto irradianceColorAttachment = rpab.buildAndRestart();
    irradianceConvolutionRpb.addColorAttachment(*irradianceColorAttachment);

    RenderPhaseBuilder irradianceConvolutionRb;
    irradianceConvolutionRb.setDevice(mainDevice);
    irradianceConvolutionRb.setRenderPass(irradianceConvolutionRpb.build());
    irradianceConvolutionRb.setCaptureEnable(true);
    auto irradianceConvolutionPhase = irradianceConvolutionRb.build();
    m_irradianceConvolutionPhase = irradianceConvolutionPhase.get();

    // Opaque
    RenderPassBuilder opaqueRpb;
    opaqueRpb.setDevice(mainDevice);
    rpd.configureSwapChainRenderPassBuilder(opaqueRpb, *m_window->getSwapChain());

    rpad.configureAttachmentClearBuilder(rpab);
    rpab.setFormat(m_window->getSwapChain()->getImageFormat());
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    auto clearColorAttachment = rpab.buildAndRestart();
    opaqueRpb.addColorAttachment(*clearColorAttachment);

    rpad.configureAttachmentClearBuilder(rpab);
    rpab.setFormat(m_window->getSwapChain()->getDepthImageFormat());
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    auto clearDepthAttachment = rpab.buildAndRestart();
    opaqueRpb.addDepthAttachment(*clearDepthAttachment);

    RenderPhaseBuilder opaqueRb;
    opaqueRb.setDevice(mainDevice);
    opaqueRb.setRenderPass(opaqueRpb.build());
    auto opaquePhase = opaqueRb.build();
    m_opaquePhase = opaquePhase.get();

    RenderPassBuilder probesDebugRpb;
    probesDebugRpb.setDevice(mainDevice);
    rpd.configureSwapChainRenderPassBuilder(probesDebugRpb, *m_window->getSwapChain());

    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(m_window->getSwapChain()->getImageFormat());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    auto probesDebugColorAttachment = rpab.buildAndRestart();
    probesDebugRpb.addColorAttachment(*probesDebugColorAttachment);

    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(m_window->getSwapChain()->getDepthImageFormat());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    auto probesDebugDepthAttachment = rpab.buildAndRestart();
    probesDebugRpb.addDepthAttachment(*probesDebugDepthAttachment);

    RenderPhaseBuilder probesDebugRb;
    probesDebugRb.setDevice(mainDevice);
    probesDebugRb.setRenderPass(probesDebugRpb.build());
    auto probesDebugPhase = probesDebugRb.build();
    m_probesDebugPhase = probesDebugPhase.get();

    // Skybox
    RenderPassBuilder skyboxRpb;
    skyboxRpb.setDevice(mainDevice);
    rpd.configureSwapChainRenderPassBuilder(skyboxRpb, *m_window->getSwapChain());

    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(m_window->getSwapChain()->getImageFormat());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto loadColorAttachment = rpab.buildAndRestart();
    skyboxRpb.addColorAttachment(*loadColorAttachment);

    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(m_window->getSwapChain()->getDepthImageFormat());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    auto loadDepthAttachment = rpab.buildAndRestart();
    skyboxRpb.addDepthAttachment(*loadDepthAttachment);

    RenderPhaseBuilder skyboxRb;
    skyboxRb.setDevice(mainDevice);
    skyboxRb.setRenderPass(skyboxRpb.build());
    auto skyboxPhase = skyboxRb.build();
    m_skyboxPhase = skyboxPhase.get();

    RenderPassBuilder postProcessRpb;
    postProcessRpb.setDevice(mainDevice);
    rpd.configureSwapChainRenderPassBuilder(postProcessRpb, *m_window->getSwapChain(), false);
    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(m_window->getSwapChain()->getImageFormat());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    auto finalLoadColorAttachment = rpab.buildAndRestart();
    postProcessRpb.addColorAttachment(*finalLoadColorAttachment);

    RenderPhaseBuilder postProcessRb;
    postProcessRb.setDevice(mainDevice);
    postProcessRb.setRenderPass(postProcessRpb.build());
    postProcessRb.setParentPhase(m_skyboxPhase);
    auto postProcessPhase = postProcessRb.build();
    m_postProcessPhase = postProcessPhase.get();

    RenderPassBuilder imguiRpb;
    imguiRpb.setDevice(mainDevice);
    rpd.configureSwapChainRenderPassBuilder(imguiRpb, *m_window->getSwapChain(), false);

    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(m_window->getSwapChain()->getImageFormat());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    auto imguiLoadColorAttachment = rpab.buildAndRestart();
    imguiRpb.addColorAttachment(*imguiLoadColorAttachment);

    RenderPhaseBuilder imguiRb;
    imguiRb.setDevice(mainDevice);
    imguiRb.setRenderPass(imguiRpb.build());
    auto imguiPhase = imguiRb.build();
    m_imguiPhase = imguiPhase.get();

    RendererBuilder rb;
    rb.setDevice(mainDevice);
    rb.setSwapChain(m_window->getSwapChain());
    std::unique_ptr<RenderGraph> renderGraph = std::make_unique<RenderGraph>();
    renderGraph->addOneTimeRenderPhase(std::move(opaqueCapturePhase));
    renderGraph->addOneTimeRenderPhase(std::move(skyboxCapturePhase));
    renderGraph->addOneTimeRenderPhase(std::move(irradianceConvolutionPhase));
    renderGraph->addRenderPhase(std::move(opaquePhase));
    renderGraph->addRenderPhase(std::move(probesDebugPhase));
    renderGraph->addRenderPhase(std::move(skyboxPhase));
    renderGraph->addRenderPhase(std::move(postProcessPhase));
    renderGraph->addRenderPhase(std::move(imguiPhase));
    rb.setRenderGraph(std::move(renderGraph));
    m_renderer = rb.build();
}

Application::~Application()
{
    // ensure all commands have finished
    vkDeviceWaitIdle(m_devices[0]->getHandle());

    capturedEnvMaps.clear();
    irradianceMaps.clear();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_renderer.reset();
    m_scene.reset();

    m_window.reset();

    m_devices.clear();
    m_context.reset();

    WindowGLFW::terminate();
}

void Application::initImgui()
{
    VkDevice deviceHandle = m_devices[0]->getHandle();

    ImGuiRenderStateBuilder imguirsb;

    imguirsb.setDevice(m_devices[0]);
    imguirsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    ImGui::CreateContext();
    if (!ImGui_ImplGlfw_InitForVulkan(m_window->getHandle(), true))
    {
        std::cerr << "Failed to initialize ImGui GLFW Implemenation For Vulkan" << std::endl;
        throw;
    }

    std::shared_ptr<RenderStateABC> render_state = imguirsb.build();
    m_imguiPhase->registerRenderStateToAllPool(render_state);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_context->getInstanceHandle();
    init_info.PhysicalDevice = m_devices[0]->getPhysicalHandle();
    init_info.Device = deviceHandle;
    init_info.Queue = m_devices[0]->getGraphicsQueue();
    init_info.DescriptorPool = render_state->getDescriptorPool();
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.RenderPass = m_imguiPhase->getRenderPass()->getHandle();

    if (!ImGui_ImplVulkan_Init(&init_info))
    {
        std::cerr << "Failed to initialize ImGui Implementation for Vulkan" << std::endl;
        throw;
    }
}

void Application::displayImgui()
{
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Radiance playground");

    ImGui::Text(std::format("Average FPS: {0}", ImGui::GetIO().Framerate).c_str());

    if (ImGui::CollapsingHeader("Scene Objects", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
    {
        const auto &objects = m_scene->getObjects();
        for (auto &object : objects) 
        {
            if (ImGui::TreeNode(object->getName().c_str()))
            {
                Transform t = object->getTransform();
                glm::vec3 euler = glm::degrees(glm::eulerAngles(t.rotation));

                bool isTransformEdited = false;
                ImGui::PushID(object->getName().c_str());
                isTransformEdited |= ImGui::DragFloat3("Position", &t.position[0]);
                if (ImGui::DragFloat3("Rotation", &euler[0]))
                {
                    isTransformEdited = true;
                    t.rotation = glm::quat(glm::radians(euler));
                }
                isTransformEdited |= ImGui::DragFloat3("Scale", &t.scale[0]);
                ImGui::PopID();

                if (isTransformEdited)
                {
                    object->setTransform(t);
                }

                ImGui::TreePop();
            }
        }
    }

    if (ImGui::CollapsingHeader("Scene Lights", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
    {
        const auto &lights = m_scene->getLights();

        int lightIndex = 0u;
        for (auto& light : lights)
        {
            if (ImGui::TreeNode(std::format("Light {0}", lightIndex).c_str()))
            {
                ImGui::PushID(lightIndex);
                if (PointLight* pointLight = dynamic_cast<PointLight*>(light.get()))
                {
                    ImGui::DragFloat3("Position", &pointLight->position.x);
                    ImGui::DragFloat3("Attenuation", &pointLight->attenuation[0]);
                }
                else if (DirectionalLight* directionalLight = dynamic_cast<DirectionalLight*>(light.get()))
                {
                    ImGui::DragFloat3("Direction", &directionalLight->direction.x);
                }

                ImGui::ColorEdit3("Diffuse Color", &light->diffuseColor.r);
                ImGui::DragFloat("Diffuse Power", &light->diffusePower);

                ImGui::ColorEdit3("Specular Color", &light->specularColor.r);
                ImGui::DragFloat("Specular Power", &light->specularPower);

                ImGui::PopID();

                ImGui::TreePop();
            }

            lightIndex++;
        }
    }


    ImGui::End();
}

void Application::runLoop()
{
    std::shared_ptr<Device> mainDevice = m_devices[0];

    m_window->makeContextCurrent();

    bool show_demo_window = true;

    m_scene = std::make_unique<SampleScene2D>(mainDevice);

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

    PipelineBuilder irradianceConvolutionPb;
    irradianceConvolutionPb.setDevice(mainDevice);
    irradianceConvolutionPb.addVertexShaderStage("skybox");
    irradianceConvolutionPb.addFragmentShaderStage("irradiance_convolution");
    irradianceConvolutionPb.setRenderPass(m_irradianceConvolutionPhase->getRenderPass());
    irradianceConvolutionPb.setExtent(m_window->getSwapChain()->getExtent());

    PipelineDirector irradianceConvolutionPd;
    irradianceConvolutionPd.createColorDepthRasterizerBuilder(irradianceConvolutionPb);
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

    UniformDescriptorBuilder phongMaterialUdb;
    phongMaterialUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    });

    PipelineBuilder phongPb;
    phongPb.setDevice(mainDevice);
    phongPb.addVertexShaderStage("phong");
    phongPb.addFragmentShaderStage("phong");
    phongPb.setRenderPass(m_opaquePhase->getRenderPass());
    phongPb.setExtent(m_window->getSwapChain()->getExtent());
    phongPb.addPushConstantRange(VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = 16,
    });

    PipelineDirector phongPd;
    phongPd.createColorDepthRasterizerBuilder(phongPb);
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

    UniformDescriptorBuilder phongCaptureMaterialUdb;
    phongCaptureMaterialUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    });

    PipelineBuilder phongCapturePb;
    phongCapturePb.setDevice(mainDevice);
    phongCapturePb.addVertexShaderStage("phong");
    phongCapturePb.addFragmentShaderStage("phong");
    phongCapturePb.setRenderPass(m_opaqueCapturePhase->getRenderPass());
    phongCapturePb.setExtent(m_window->getSwapChain()->getExtent());
    phongCapturePb.addPushConstantRange(VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = 16,
    });

    PipelineDirector phongCapturePd;
    phongCapturePd.createColorDepthRasterizerBuilder(phongCapturePb);
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

    PipelineBuilder environmentMapPb;
    environmentMapPb.setDevice(mainDevice);
    environmentMapPb.addVertexShaderStage("environment_map");
    environmentMapPb.addFragmentShaderStage("environment_map");
    environmentMapPb.setRenderPass(m_skyboxPhase->getRenderPass());
    environmentMapPb.setExtent(m_window->getSwapChain()->getExtent());
    environmentMapPb.addPushConstantRange(VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = 16,
    });

    PipelineDirector environmentMapPd;
    environmentMapPd.createColorDepthRasterizerBuilder(environmentMapPb);
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

    PipelineBuilder environmentMapCapturePb;
    environmentMapCapturePb.setDevice(mainDevice);
    environmentMapCapturePb.addVertexShaderStage("environment_map");
    environmentMapCapturePb.addFragmentShaderStage("environment_map");
    environmentMapCapturePb.setRenderPass(m_skyboxCapturePhase->getRenderPass());
    environmentMapCapturePb.setExtent(m_window->getSwapChain()->getExtent());
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
    tb.setDevice(mainDevice);
    tb.setImageData(defaultDiffusePixels);
    ModelRenderState::s_defaultDiffuseTexture = tb.buildAndRestart();

    PipelineDirector environmentMapCapturePd;
    environmentMapCapturePd.createColorDepthRasterizerBuilder(environmentMapCapturePb);
    environmentMapCapturePb.addUniformDescriptorPack(environmentMapCaptureUdb.buildAndRestart());

    std::shared_ptr<Pipeline> environmentMapCapturePipeline = environmentMapCapturePb.build();

    auto objects = m_scene->getObjects();

    std::shared_ptr<Skybox> skybox = m_scene->getSkybox();
    for (int i = 0; i < objects.size(); ++i)
    {
        ModelRenderStateBuilder mrsb;
        mrsb.setFrameInFlightCount(m_window->getSwapChain()->getSwapChainImageCount());
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        mrsb.setDevice(mainDevice);

        ModelRenderStateBuilder captureMrsb;
        captureMrsb.setFrameInFlightCount(m_window->getSwapChain()->getSwapChainImageCount());
        captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        captureMrsb.setDevice(mainDevice);

        mrsb.setModel(objects[i]);
        captureMrsb.setModel(objects[i]);

        // Check if the mesh is the quad, the sphere or the cube
        if (i != 1 && i != 2 && i != 3)
        {
            mrsb.setPipeline(phongPipeline);
            captureMrsb.setPipeline(phongCapturePipeline);

            mrsb.setEnvironmentMaps(irradianceMaps);
            captureMrsb.setEnvironmentMaps(irradianceMaps);
        }
        else
        {
            mrsb.setTextureDescriptorEnable(false);
            mrsb.setProbeDescriptorEnable(false);
            mrsb.setLightDescriptorEnable(false);
            mrsb.setPipeline(environmentMapPipeline);

            captureMrsb.setTextureDescriptorEnable(false);
            captureMrsb.setProbeDescriptorEnable(false);
            captureMrsb.setLightDescriptorEnable(false);
            captureMrsb.setPipeline(environmentMapCapturePipeline);

            mrsb.setEnvironmentMaps(irradianceMaps);
            captureMrsb.setEnvironmentMaps(irradianceMaps);
        }

        m_opaquePhase->registerRenderStateToAllPool(mrsb.build());
        m_opaqueCapturePhase->registerRenderStateToAllPool(captureMrsb.build());
    }

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

    PipelineBuilder probeGridDebugPb;
    probeGridDebugPb.setDevice(mainDevice);
    probeGridDebugPb.addVertexShaderStage("probe_grid_debug");
    probeGridDebugPb.addFragmentShaderStage("probe_grid_debug");
    probeGridDebugPb.setRenderPass(m_probesDebugPhase->getRenderPass());
    probeGridDebugPb.setExtent(m_window->getSwapChain()->getExtent());
    probeGridDebugPb.addPushConstantRange(VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = 16,
        });

    PipelineDirector probeGridDebugPd;
    probeGridDebugPd.createColorDepthRasterizerBuilder(probeGridDebugPb);
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
    std::shared_ptr<ProbeGrid> grid = gridBuilder.build();

    MeshDirector md;
    MeshBuilder sphereMb;
    md.createSphereMeshBuilder(sphereMb, 0.5f, 50, 50);
    sphereMb.setDevice(mainDevice);
    std::shared_ptr<Mesh> sphereMesh = sphereMb.buildAndRestart();

    ProbeGridRenderStateBuilder prsb;
    prsb.setFrameInFlightCount(3);
    prsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    prsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    prsb.setDevice(mainDevice);
    prsb.setPipeline(probeGridDebugPipeline);
    prsb.setProbeGrid(grid);
    prsb.setEnvironmentMaps(irradianceMaps);
    prsb.setMesh(sphereMesh);
    m_probesDebugPhase->registerRenderStateToAllPool(prsb.build());

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

    PipelineBuilder skyboxPb;
    PipelineDirector skyboxPd;
    skyboxPd.createColorDepthRasterizerBuilder(skyboxPb);
    skyboxPb.setDevice(mainDevice);
    skyboxPb.addVertexShaderStage("skybox");
    skyboxPb.addFragmentShaderStage("skybox");
    skyboxPb.setRenderPass(m_skyboxPhase->getRenderPass());
    skyboxPb.setExtent(m_window->getSwapChain()->getExtent());
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

    PipelineBuilder skyboxCapturePb;
    PipelineDirector skyboxCapturePd;
    skyboxCapturePd.createColorDepthRasterizerBuilder(skyboxCapturePb);
    skyboxCapturePb.setDevice(mainDevice);
    skyboxCapturePb.addVertexShaderStage("skybox");
    skyboxCapturePb.addFragmentShaderStage("skybox");
    skyboxCapturePb.setRenderPass(m_skyboxCapturePhase->getRenderPass());
    skyboxCapturePb.setExtent(m_window->getSwapChain()->getExtent());
    skyboxCapturePb.setDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL);

    skyboxCapturePb.addUniformDescriptorPack(skyboxOpaqueUdb.buildAndRestart());

    std::shared_ptr<Pipeline> skyboxCapturePipeline = skyboxCapturePb.build();

    if (skybox)
    {
        for (int i = 0; i < maxProbeCount; i++)
        {
            EnvironmentCaptureRenderStateBuilder irsb;
            irsb.setFrameInFlightCount(1);
            irsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
            irsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            irsb.setDevice(mainDevice);
            irsb.setSkybox(skybox);
            irsb.setTexture(capturedEnvMaps[i]);
            irsb.setPipeline(irradianceConvolutionPipeline);
            m_irradianceConvolutionPhase->registerRenderStateToSpecificPool(irsb.build(), i);
        }

        SkyboxRenderStateBuilder srsb;
        srsb.setFrameInFlightCount(m_renderer->getFrameInFlightCount());
        srsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        srsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        srsb.setDevice(mainDevice);
        srsb.setSkybox(skybox);
        srsb.setTexture(skybox->getTexture());
        srsb.setPipeline(skyboxPipeline);
        m_skyboxPhase->registerRenderStateToAllPool(srsb.build());

        SkyboxRenderStateBuilder captureSrsb;
        captureSrsb.setFrameInFlightCount(m_renderer->getFrameInFlightCount());
        captureSrsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        captureSrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        captureSrsb.setDevice(mainDevice);
        captureSrsb.setSkybox(skybox);
        captureSrsb.setTexture(skybox->getTexture());
        captureSrsb.setPipeline(skyboxCapturePipeline);
        m_skyboxCapturePhase->registerRenderStateToAllPool(captureSrsb.build());
    }

    MeshBuilder mb;
    mb.setDevice(mainDevice);
    mb.setVertices({{{-1.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 0.0f, 0.0f, 1.f}, {0.f, 0.f}},
                    {{1.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, {0.0f, 1.0f, 0.0f, 1.f}, {1.f, 0.f}},
                    {{1.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {0.0f, 0.0f, 1.0f, 1.f}, {1.f, 1.f}},
                    {{-1.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 1.0f, 1.0f, 1.f}, {0.f, 1.f}}});
    mb.setIndices({0, 1, 2, 2, 3, 0});
    std::shared_ptr<Mesh> postProcessQuadMesh = mb.buildAndRestart();
    ModelBuilder modelBuilder;
    modelBuilder.setMesh(postProcessQuadMesh);
    modelBuilder.setName("Viking Room");
    std::shared_ptr<Model> postProcessQuadModel = modelBuilder.build();
    ModelRenderStateBuilder quadRsb;
    quadRsb.setDevice(mainDevice);
    quadRsb.setProbeDescriptorEnable(false);
    quadRsb.setLightDescriptorEnable(false);
    quadRsb.setTextureDescriptorEnable(false);
    quadRsb.setMVPDescriptorEnable(false);
    quadRsb.setPushViewPositionEnable(false);
    quadRsb.setFrameInFlightCount(m_renderer->getFrameInFlightCount());
    quadRsb.setModel(postProcessQuadModel);
    quadRsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    quadRsb.setMaterialDescriptorSetUpdatePred(
        [&](const RenderPhase *parentPhase, uint32_t imageIndex, const VkDescriptorSet set) {
            auto deviceHandle = mainDevice->getHandle();
            const auto &sampler = m_window->getSwapChain()->getSampler();
            if (!sampler.has_value())
                return;

            VkDescriptorImageInfo imageInfo = {
                .sampler = *sampler.value(),
                .imageView = parentPhase->getRenderPass()->getImageView(0u, imageIndex),
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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
    PipelineBuilder postProcessPb;
    PipelineDirector postProcessPd;
    postProcessPd.createColorDepthRasterizerBuilder(postProcessPb);
    postProcessPb.setDevice(mainDevice);
    postProcessPb.setRenderPass(m_postProcessPhase->getRenderPass());
    postProcessPb.addVertexShaderStage("screen");
    postProcessPb.addFragmentShaderStage("postprocess");
    postProcessPb.setExtent(m_window->getSwapChain()->getExtent());
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
    m_postProcessPhase->registerRenderStateToAllPool(quadRsb.build());

    initImgui();

    auto &lights = m_scene->getLights();

    CameraABC *mainCamera = m_scene->getMainCamera();

    m_scene->beginSimulation();
    while (!m_window->shouldClose())
    {
        m_timeManager.markFrame();
        float deltaTime = m_timeManager.deltaTime();

        displayImgui();

        m_inputManager.UpdateInputStates();
        m_window->pollEvents();

        m_scene->updateSimulation(deltaTime);

        VkResult res = m_renderer->renderFrame(
            VkRect2D{
                .offset = {0, 0},
                .extent = m_window->getSwapChain()->getExtent(),
            },
            *mainCamera, lights, grid);
        if (res == VK_ERROR_OUT_OF_DATE_KHR)
        {
            m_window->recreateSwapChain();
            m_renderer->setSwapChain(m_window->getSwapChain());
        }

        m_window->swapBuffers();
    }

    // ensure all commands have finished before destroying objects in this scope
    vkDeviceWaitIdle(mainDevice->getHandle());
}