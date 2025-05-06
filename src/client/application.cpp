#include <assimp/Importer.hpp>
#include <format>
#include <memory>
#include <string>

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

#include "scripts/radiance_cascades.hpp"

#include "application.hpp"

constexpr uint32_t maxProbeCount = 64u;
constexpr uint32_t bufferingType = 3;

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
        if (m_devices.back()->isDiscrete())
        {
            m_discreteDevice = m_devices.back();
            std::cout << "Using device : " << m_discreteDevice->getDeviceName() << std::endl;
        }
    }

    SwapChainBuilder scb;
    scb.setDevice(m_discreteDevice);
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
        captureEnvMapBuilder.setDevice(m_discreteDevice);
        captureEnvMapBuilder.setWidth(256);
        captureEnvMapBuilder.setHeight(256);
        captureEnvMapBuilder.setCreateFromUserData(false);
        captureEnvMapBuilder.setDepthImageEnable(true);
        td.configureUNORMTextureBuilder(captureEnvMapBuilder);
        capturedEnvMaps.push_back(captureEnvMapBuilder.buildAndRestart());
    }

    // Opaque capture
    RenderPassBuilder opaqueCaptureRpb;
    opaqueCaptureRpb.setDevice(m_discreteDevice);
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
    opaqueCaptureRb.setDevice(m_discreteDevice);
    opaqueCaptureRb.setRenderPass(opaqueCaptureRpb.build());
    opaqueCaptureRb.setCaptureEnable(true);
    opaqueCaptureRb.setBufferingType(bufferingType);
    auto opaqueCapturePhase = opaqueCaptureRb.build();
    m_opaqueCapturePhase = opaqueCapturePhase.get();

    // Skybox capture
    RenderPassBuilder skyboxCaptureRpb;
    skyboxCaptureRpb.setDevice(m_discreteDevice);
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
    skyboxCaptureRb.setDevice(m_discreteDevice);
    skyboxCaptureRb.setRenderPass(skyboxCaptureRpb.build());
    skyboxCaptureRb.setCaptureEnable(true);
    skyboxCaptureRb.setBufferingType(bufferingType);
    auto skyboxCapturePhase = skyboxCaptureRb.build();
    m_skyboxCapturePhase = skyboxCapturePhase.get();

    // Irradiance cubemap
    for (int i = 0; i < maxProbeCount; i++)
    {
        CubemapBuilder irradianceMapBuilder;
        irradianceMapBuilder.setDevice(m_discreteDevice);
        irradianceMapBuilder.setWidth(128);
        irradianceMapBuilder.setHeight(128);
        irradianceMapBuilder.setCreateFromUserData(false);
        irradianceMapBuilder.setResolveEnable(true);
        td.configureUNORMTextureBuilder(irradianceMapBuilder);
        irradianceMaps.push_back(irradianceMapBuilder.buildAndRestart());
    }

    // Irradiance convolution
    RenderPassBuilder irradianceConvolutionRpb;
    irradianceConvolutionRpb.setDevice(m_discreteDevice);
    rpd.configurePooledCubemapsRenderPassBuilder(irradianceConvolutionRpb, irradianceMaps, true, false);
    rpad.configureAttachmentDontCareBuilder(rpab);
    rpab.setFormat(irradianceMaps[0]->getImageFormat());
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto irradianceColorAttachment = rpab.buildAndRestart();
    irradianceConvolutionRpb.addColorAttachment(*irradianceColorAttachment);

    RenderPhaseBuilder irradianceConvolutionRb;
    irradianceConvolutionRb.setDevice(m_discreteDevice);
    irradianceConvolutionRb.setRenderPass(irradianceConvolutionRpb.build());
    irradianceConvolutionRb.setCaptureEnable(true);
    irradianceConvolutionRb.setBufferingType(bufferingType);
    auto irradianceConvolutionPhase = irradianceConvolutionRb.build();
    m_irradianceConvolutionPhase = irradianceConvolutionPhase.get();

    // Opaque
    RenderPassBuilder opaqueRpb;
    opaqueRpb.setDevice(m_discreteDevice);
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
    opaqueRb.setDevice(m_discreteDevice);
    opaqueRb.setRenderPass(opaqueRpb.build());
    opaqueRb.setBufferingType(bufferingType);
    auto opaquePhase = opaqueRb.build();
    m_opaquePhase = opaquePhase.get();

    RenderPassBuilder probesDebugRpb;
    probesDebugRpb.setDevice(m_discreteDevice);
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
    probesDebugRb.setDevice(m_discreteDevice);
    probesDebugRb.setRenderPass(probesDebugRpb.build());
    auto probesDebugPhase = probesDebugRb.build();
    m_probesDebugPhase = probesDebugPhase.get();

    // Skybox
    RenderPassBuilder skyboxRpb;
    skyboxRpb.setDevice(m_discreteDevice);
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
    skyboxRb.setDevice(m_discreteDevice);
    skyboxRb.setRenderPass(skyboxRpb.build());
    skyboxRb.setBufferingType(bufferingType);
    auto skyboxPhase = skyboxRb.build();
    m_skyboxPhase = skyboxPhase.get();

    std::unique_ptr<RenderPhase> postProcessPhase;
    {
        RenderPassBuilder postProcessRpb;
        postProcessRpb.setDevice(m_discreteDevice);
        rpd.configureSwapChainRenderPassBuilder(postProcessRpb, *m_window->getSwapChain(), false);
        rpad.configureAttachmentLoadBuilder(rpab);
        rpab.setFormat(m_window->getSwapChain()->getImageFormat());
        rpab.setInitialLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        rpab.setFinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        auto finalLoadColorAttachment = rpab.buildAndRestart();
        postProcessRpb.addColorAttachment(*finalLoadColorAttachment);
        RenderPhaseBuilder postProcessRb;
        postProcessRb.setDevice(m_discreteDevice);
        postProcessRb.setRenderPass(postProcessRpb.build());
        postProcessRb.setParentPhase(m_skyboxPhase);
        postProcessRb.setBufferingType(bufferingType);
        postProcessPhase = postProcessRb.build();
        m_postProcessPhase = postProcessPhase.get();
    }

    std::unique_ptr<ComputePhase> computePhase;
    {
        ComputePhaseBuilder cpb;
        cpb.setDevice(m_discreteDevice);
        cpb.setBufferingType(bufferingType);
        computePhase = cpb.build();
        m_computePhase = computePhase.get();
    }

    std::unique_ptr<RenderPhase> postProcess2Phase;
    {
        RenderPassBuilder postProcessRpb;
        postProcessRpb.setDevice(m_discreteDevice);
        rpd.configureSwapChainRenderPassBuilder(postProcessRpb, *m_window->getSwapChain(), false);
        rpad.configureAttachmentLoadBuilder(rpab);
        rpab.setFormat(m_window->getSwapChain()->getImageFormat());
        rpab.setInitialLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        rpab.setFinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        auto finalLoadColorAttachment = rpab.buildAndRestart();
        postProcessRpb.addColorAttachment(*finalLoadColorAttachment);
        RenderPhaseBuilder postProcessRb;
        postProcessRb.setDevice(m_discreteDevice);
        postProcessRb.setRenderPass(postProcessRpb.build());
        postProcessRb.setBufferingType(bufferingType);
        postProcess2Phase = postProcessRb.build();
        m_postProcess2Phase = postProcess2Phase.get();
    }

    RenderPassBuilder imguiRpb;
    imguiRpb.setDevice(m_discreteDevice);
    rpd.configureSwapChainRenderPassBuilder(imguiRpb, *m_window->getSwapChain(), false);

    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(m_window->getSwapChain()->getImageFormat());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    auto imguiLoadColorAttachment = rpab.buildAndRestart();
    imguiRpb.addColorAttachment(*imguiLoadColorAttachment);

    RenderPhaseBuilder imguiRb;
    imguiRb.setDevice(m_discreteDevice);
    imguiRb.setRenderPass(imguiRpb.build());
    imguiRb.setBufferingType(bufferingType);
    auto imguiPhase = imguiRb.build();
    m_imguiPhase = imguiPhase.get();

    RendererBuilder rb;
    rb.setDevice(m_discreteDevice);
    rb.setSwapChain(m_window->getSwapChain());
    rb.setFrameInFlightCount(bufferingType);
    std::unique_ptr<RenderGraph> renderGraph = std::make_unique<RenderGraph>();
    renderGraph->addOneTimeRenderPhase(std::move(opaqueCapturePhase));
    renderGraph->addOneTimeRenderPhase(std::move(skyboxCapturePhase));
    renderGraph->addOneTimeRenderPhase(std::move(irradianceConvolutionPhase));
    renderGraph->addRenderPhase(std::move(opaquePhase));
    renderGraph->addRenderPhase(std::move(probesDebugPhase));
    renderGraph->addRenderPhase(std::move(skyboxPhase));
    renderGraph->addRenderPhase(std::move(postProcessPhase));
    renderGraph->addPhase(std::move(computePhase));
    renderGraph->addRenderPhase(std::move(postProcess2Phase));
    renderGraph->addRenderPhase(std::move(imguiPhase));
    rb.setRenderGraph(std::move(renderGraph));
    m_renderer = rb.build();
}

Application::~Application()
{
    // ensure all commands have finished
    vkDeviceWaitIdle(m_discreteDevice->getHandle());

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
    VkDevice deviceHandle = m_discreteDevice->getHandle();

    ImGuiRenderStateBuilder imguirsb;

    imguirsb.setDevice(m_discreteDevice);
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
    init_info.PhysicalDevice = m_discreteDevice->getPhysicalHandle();
    init_info.Device = deviceHandle;
    init_info.Queue = m_discreteDevice->getGraphicsQueue();
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
        for (auto &light : lights)
        {
            if (ImGui::TreeNode(std::format("Light {0}", lightIndex).c_str()))
            {
                ImGui::PushID(lightIndex);
                if (PointLight *pointLight = dynamic_cast<PointLight *>(light.get()))
                {
                    ImGui::DragFloat3("Position", &pointLight->position.x);
                    ImGui::DragFloat3("Attenuation", &pointLight->attenuation[0]);
                }
                else if (DirectionalLight *directionalLight = dynamic_cast<DirectionalLight *>(light.get()))
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
    m_window->makeContextCurrent();

    bool show_demo_window = true;

    m_scene = std::make_unique<SampleScene2D>(m_discreteDevice, bufferingType);

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

    PipelineBuilder<PipelineType::GRAPHICS> irradianceConvolutionPb;
    irradianceConvolutionPb.setDevice(m_discreteDevice);
    irradianceConvolutionPb.addVertexShaderStage("skybox");
    irradianceConvolutionPb.addFragmentShaderStage("irradiance_convolution");
    irradianceConvolutionPb.setRenderPass(m_irradianceConvolutionPhase->getRenderPass());
    irradianceConvolutionPb.setExtent(m_window->getSwapChain()->getExtent());

    PipelineDirector<PipelineType::GRAPHICS> irradianceConvolutionPd;
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

    UniformDescriptorBuilder phongMaterialUdb;
    phongMaterialUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    });

    PipelineBuilder<PipelineType::GRAPHICS> phongPb;
    phongPb.setDevice(m_discreteDevice);
    phongPb.addVertexShaderStage("phong");
    phongPb.addFragmentShaderStage("phong");
    phongPb.setRenderPass(m_opaquePhase->getRenderPass());
    phongPb.setExtent(m_window->getSwapChain()->getExtent());
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

    PipelineBuilder<PipelineType::GRAPHICS> phongCapturePb;
    phongCapturePb.setDevice(m_discreteDevice);
    phongCapturePb.addVertexShaderStage("phong");
    phongCapturePb.addFragmentShaderStage("phong");
    phongCapturePb.setRenderPass(m_opaqueCapturePhase->getRenderPass());
    phongCapturePb.setExtent(m_window->getSwapChain()->getExtent());
    phongCapturePb.addPushConstantRange(VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = 16,
    });

    PipelineDirector<PipelineType::GRAPHICS> phongCapturePd;
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

    PipelineBuilder<PipelineType::GRAPHICS> environmentMapPb;
    environmentMapPb.setDevice(m_discreteDevice);
    environmentMapPb.addVertexShaderStage("environment_map");
    environmentMapPb.addFragmentShaderStage("environment_map");
    environmentMapPb.setRenderPass(m_skyboxPhase->getRenderPass());
    environmentMapPb.setExtent(m_window->getSwapChain()->getExtent());
    environmentMapPb.addPushConstantRange(VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = 16,
    });

    PipelineDirector<PipelineType::GRAPHICS> environmentMapPd;
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

    PipelineBuilder<PipelineType::GRAPHICS> environmentMapCapturePb;
    environmentMapCapturePb.setDevice(m_discreteDevice);
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
    tb.setDevice(m_discreteDevice);
    tb.setImageData(defaultDiffusePixels);
    ModelRenderState::s_defaultDiffuseTexture = tb.buildAndRestart();

    PipelineDirector<PipelineType::GRAPHICS> environmentMapCapturePd;
    environmentMapCapturePd.configureColorDepthRasterizerBuilder(environmentMapCapturePb);
    environmentMapCapturePb.addUniformDescriptorPack(environmentMapCaptureUdb.buildAndRestart());

    std::shared_ptr<Pipeline> environmentMapCapturePipeline = environmentMapCapturePb.build();

    auto objects = m_scene->getObjects();

    std::shared_ptr<Skybox> skybox = m_scene->getSkybox();
    for (int i = 0; i < objects.size(); ++i)
    {
        ModelRenderStateBuilder mrsb;
        mrsb.setFrameInFlightCount(m_renderer->getFrameInFlightCount());
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        mrsb.setDevice(m_discreteDevice);

        ModelRenderStateBuilder captureMrsb;
        captureMrsb.setFrameInFlightCount(m_renderer->getFrameInFlightCount());
        captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        captureMrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        captureMrsb.setDevice(m_discreteDevice);

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

    PipelineBuilder<PipelineType::GRAPHICS> probeGridDebugPb;
    probeGridDebugPb.setDevice(m_discreteDevice);
    probeGridDebugPb.addVertexShaderStage("probe_grid_debug");
    probeGridDebugPb.addFragmentShaderStage("probe_grid_debug");
    probeGridDebugPb.setRenderPass(m_probesDebugPhase->getRenderPass());
    probeGridDebugPb.setExtent(m_window->getSwapChain()->getExtent());
    probeGridDebugPb.addPushConstantRange(VkPushConstantRange{
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = 16,
    });

    PipelineDirector<PipelineType::GRAPHICS> probeGridDebugPd;
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
    std::shared_ptr<ProbeGrid> grid = gridBuilder.build();

    MeshDirector md;
    MeshBuilder sphereMb;
    md.createSphereMeshBuilder(sphereMb, 0.5f, 50, 50);
    sphereMb.setDevice(m_discreteDevice);
    std::shared_ptr<Mesh> sphereMesh = sphereMb.buildAndRestart();

    ProbeGridRenderStateBuilder prsb;
    prsb.setFrameInFlightCount(3);
    prsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    prsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    prsb.setDevice(m_discreteDevice);
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

    PipelineBuilder<PipelineType::GRAPHICS> skyboxPb;
    PipelineDirector<PipelineType::GRAPHICS> skyboxPd;
    skyboxPd.configureColorDepthRasterizerBuilder(skyboxPb);
    skyboxPb.setDevice(m_discreteDevice);
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

    PipelineBuilder<PipelineType::GRAPHICS> skyboxCapturePb;
    PipelineDirector<PipelineType::GRAPHICS> skyboxCapturePd;
    skyboxCapturePd.configureColorDepthRasterizerBuilder(skyboxCapturePb);
    skyboxCapturePb.setDevice(m_discreteDevice);
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
            irsb.setDevice(m_discreteDevice);
            irsb.setSkybox(skybox);
            irsb.setTexture(capturedEnvMaps[i]);
            irsb.setPipeline(irradianceConvolutionPipeline);
            m_irradianceConvolutionPhase->registerRenderStateToSpecificPool(irsb.build(), i);
        }

        SkyboxRenderStateBuilder srsb;
        srsb.setFrameInFlightCount(m_renderer->getFrameInFlightCount());
        srsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        srsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        srsb.setDevice(m_discreteDevice);
        srsb.setSkybox(skybox);
        srsb.setTexture(skybox->getTexture());
        srsb.setPipeline(skyboxPipeline);
        m_skyboxPhase->registerRenderStateToAllPool(srsb.build());

        SkyboxRenderStateBuilder captureSrsb;
        captureSrsb.setFrameInFlightCount(m_renderer->getFrameInFlightCount());
        captureSrsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        captureSrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        captureSrsb.setDevice(m_discreteDevice);
        captureSrsb.setSkybox(skybox);
        captureSrsb.setTexture(skybox->getTexture());
        captureSrsb.setPipeline(skyboxCapturePipeline);
        m_skyboxCapturePhase->registerRenderStateToAllPool(captureSrsb.build());
    }

    MeshBuilder mb;
    mb.setDevice(m_discreteDevice);
    mb.setVertices({{{-1.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 0.0f, 0.0f, 1.f}, {0.f, 0.f}},
                    {{1.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, {0.0f, 1.0f, 0.0f, 1.f}, {1.f, 0.f}},
                    {{1.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {0.0f, 0.0f, 1.0f, 1.f}, {1.f, 1.f}},
                    {{-1.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 1.0f, 1.0f, 1.f}, {0.f, 1.f}}});
    mb.setIndices({0, 1, 2, 2, 3, 0});
    std::shared_ptr<Mesh> postProcessQuadMesh = mb.buildAndRestart();
    ModelBuilder modelBuilder;
    modelBuilder.setMesh(postProcessQuadMesh);
    modelBuilder.setName("post process quad");
    std::shared_ptr<Model> postProcessQuadModel = modelBuilder.build();
    ModelRenderStateBuilder quadRsb;
    quadRsb.setDevice(m_discreteDevice);
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
            auto deviceHandle = m_discreteDevice->getHandle();
            const auto &sampler = m_window->getSwapChain()->getSampler();
            if (!sampler.has_value())
                return;

            VkDescriptorImageInfo imageInfo = {
                .sampler = *sampler.value(),
                .imageView = parentPhase->getRenderPass()->getImageView(0u, imageIndex),
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

            auto s = m_scene->getReadOnlyInstancedComponents<RadianceCascades>();
            if (!s.empty())
            {
                auto rc = s[0];

                {
                    VkDescriptorBufferInfo bufferInfo = {
                        .buffer = rc->getCascadesDescBufferHandle()->getHandle(),
                        .offset = 0,
                        .range = rc->getCascadesDescBufferHandle()->getSize(),
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
                        .buffer = rc->getProbePositionsBufferHandle()->getHandle(),
                        .offset = 0,
                        .range = rc->getProbePositionsBufferHandle()->getSize(),
                    };
                    writes.push_back(VkWriteDescriptorSet{
                        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                        .dstSet = set,
                        .dstBinding = 2,
                        .dstArrayElement = 0,
                        .descriptorCount = 1,
                        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                        .pBufferInfo = &bufferInfo,
                    });
                }
            }
            vkUpdateDescriptorSets(deviceHandle, writes.size(), writes.data(), 0, nullptr);
        });
    // quadRsb.setMaterialDescriptorSetUpdatePredPerFrame(
    //     [&](const RenderPhase *parentPhase, uint32_t imageIndex, const VkDescriptorSet set, uint32_t backBufferIndex)
    //     {
    //         auto deviceHandle = m_discreteDevice->getHandle();
    //         std::vector<VkWriteDescriptorSet> writes;

    //         auto s = m_scene->getReadOnlyInstancedComponents<RadianceCascades>();
    //         if (!s.empty())
    //         {
    //             auto rc = s[0];

    //             {
    //                 VkDescriptorBufferInfo bufferInfo = {
    //                     .buffer = rc->getRadianceIntervalsStorageBufferHandle(backBufferIndex)->getHandle(),
    //                     .offset = 0,
    //                     .range = rc->getRadianceIntervalsStorageBufferHandle(backBufferIndex)->getSize(),
    //                 };
    //                 writes.push_back(VkWriteDescriptorSet{
    //                     .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
    //                     .dstSet = set,
    //                     .dstBinding = 3,
    //                     .dstArrayElement = 0,
    //                     .descriptorCount = 1,
    //                     .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    //                     .pBufferInfo = &bufferInfo,
    //                 });
    //             }
    //         }
    //         vkUpdateDescriptorSets(deviceHandle, writes.size(), writes.data(), 0, nullptr);
    //     });
    PipelineBuilder<PipelineType::GRAPHICS> postProcessPb;
    PipelineDirector<PipelineType::GRAPHICS> postProcessPd;
    postProcessPd.configureColorDepthRasterizerBuilder(postProcessPb);
    postProcessPb.setDevice(m_discreteDevice);
    postProcessPb.setRenderPass(m_postProcessPhase->getRenderPass());
    postProcessPb.addVertexShaderStage("screen");
    postProcessPb.addFragmentShaderStage("postprocess");
    postProcessPb.setExtent(m_window->getSwapChain()->getExtent());
    postProcessPb.setDepthTestEnable(VK_FALSE);
    postProcessPb.setDepthWriteEnable(VK_FALSE);
    postProcessPb.setBlendEnable(VK_FALSE);
    postProcessPb.setFrontFace(VK_FRONT_FACE_CLOCKWISE);
    UniformDescriptorBuilder postProcessUdb;
    // rendered image
    postProcessUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    });
    // cascade desc buffer
    postProcessUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    });
    // cascade probes position buffer
    postProcessUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    });
    // radiance interval storage buffer
    postProcessUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
        .binding = 3,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
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
    vkDeviceWaitIdle(m_discreteDevice->getHandle());
}