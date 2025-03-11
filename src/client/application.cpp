#include <assimp/Importer.hpp>
#include <memory>

#include "graphics/context.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/render_pass.hpp"

#include "wsi/window.hpp"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include "renderer/light.hpp"
#include "renderer/mesh.hpp"
#include "renderer/render_graph.hpp"
#include "renderer/render_phase.hpp"
#include "renderer/render_state.hpp"
#include "renderer/renderer.hpp"
#include "renderer/skybox.hpp"
#include "renderer/texture.hpp"

#include "engine/camera.hpp"
#include "engine/uniform.hpp"
#include "engine/vertex.hpp"

#include "sample_scene.hpp"
#include "sample_scene_2d.hpp"

#include "application.hpp"

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
        m_devices.emplace_back(db.build());
    }

    std::weak_ptr<Device> mainDevice = m_devices[1];

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

    RenderPassBuilder phongRpb;
    phongRpb.setDevice(mainDevice);
    phongRpb.setSwapChain(m_window->getSwapChain());

    RenderPassAttachmentBuilder rpab;
    RenderPassAttachmentDirector rpad;

    rpad.configureAttachmentClearBuilder(rpab);
    rpab.setFormat(m_window->getSwapChain()->getImageFormat());
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    auto clearColorAttachment = rpab.buildAndRestart();
    phongRpb.addColorAttachment(*clearColorAttachment);

    rpad.configureAttachmentClearBuilder(rpab);
    rpab.setFormat(m_window->getSwapChain()->getDepthImageFormat());
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    auto clearDepthAttachment = rpab.buildAndRestart();
    phongRpb.addDepthAttachment(*clearDepthAttachment);

    RenderPhaseBuilder phongRb;
    phongRb.setDevice(mainDevice);
    phongRb.setRenderPass(phongRpb.build());
    auto phongPhase = phongRb.build();
    m_phongPhase = phongPhase.get();

    RenderPassBuilder skyboxRpb;
    skyboxRpb.setDevice(mainDevice);
    skyboxRpb.setSwapChain(m_window->getSwapChain());

    rpad.configureAttachmentDontCareBuilder(rpab);
    rpab.setFormat(m_window->getSwapChain()->getImageFormat());
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
    postProcessRpb.setSwapChain(m_window->getSwapChain());
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
    imguiRpb.setSwapChain(m_window->getSwapChain());

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
    renderGraph->addRenderPhase(std::move(phongPhase));
    renderGraph->addRenderPhase(std::move(skyboxPhase));
    renderGraph->addRenderPhase(std::move(postProcessPhase));
    renderGraph->addRenderPhase(std::move(imguiPhase));
    rb.setRenderGraph(std::move(renderGraph));
    m_renderer = rb.build();
}

Application::~Application()
{
    vkDeviceWaitIdle(m_devices[1]->getHandle());

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
    VkDevice deviceHandle = m_devices[1]->getHandle();

    ImGuiRenderStateBuilder imguirsb;

    imguirsb.setDevice(m_devices[1]);
    imguirsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    ImGui::CreateContext();
    if (!ImGui_ImplGlfw_InitForVulkan(m_window->getHandle(), true))
    {
        std::cerr << "Failed to initialize ImGui GLFW Implemenation For Vulkan" << std::endl;
        throw;
    }

    std::shared_ptr<RenderStateABC> render_state = imguirsb.build();
    m_imguiPhase->registerRenderState(render_state);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_context->getInstanceHandle();
    init_info.PhysicalDevice = m_devices[1]->getPhysicalHandle();
    init_info.Device = deviceHandle;
    init_info.Queue = m_devices[1]->getGraphicsQueue();
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

void Application::runLoop()
{
    std::shared_ptr<Device> mainDevice = m_devices[1];

    m_window->makeContextCurrent();

    bool show_demo_window = true;

    m_scene = std::make_unique<SampleScene>(mainDevice, m_window.get());

    // material
    UniformDescriptorBuilder phongUdb;
    phongUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    });
    phongUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    });
    phongUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    });
    phongUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
        .binding = 3,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    });
    phongUdb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
        .binding = 4,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    });

    PipelineBuilder phongPb;
    phongPb.setDevice(mainDevice);
    phongPb.addVertexShaderStage("phong");
    phongPb.addFragmentShaderStage("phong");
    phongPb.setRenderPass(m_phongPhase->getRenderPass());
    phongPb.setExtent(m_window->getSwapChain()->getExtent());

    PipelineDirector phongPd;
    phongPd.createColorDepthRasterizerBuilder(phongPb);
    phongPb.setUniformDescriptorPack(phongUdb.buildAndRestart());

    std::shared_ptr<Pipeline> phongPipeline = phongPb.build();

    auto objects = m_scene->getObjects();

    std::shared_ptr<Skybox> skybox = m_scene->getSkybox();
    for (int i = 0; i < objects.size(); ++i)
    {
        MeshRenderStateBuilder mrsb;
        mrsb.setFrameInFlightCount(m_renderer->getFrameInFlightCount());
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        mrsb.setDevice(mainDevice);
        mrsb.setTexture(objects[i]->getTexture());

        if (skybox)
            mrsb.setEnvironmentMap(skybox->getTexture());

        mrsb.setMesh(objects[i]);

        mrsb.setPipeline(phongPipeline);

        m_phongPhase->registerRenderState(mrsb.build());
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

    PipelineBuilder skyboxPb;
    PipelineDirector skyboxPd;
    skyboxPd.createColorDepthRasterizerBuilder(skyboxPb);
    skyboxPb.setDevice(mainDevice);
    skyboxPb.addVertexShaderStage("skybox");
    skyboxPb.addFragmentShaderStage("skybox");
    skyboxPb.setRenderPass(m_phongPhase->getRenderPass());
    skyboxPb.setExtent(m_window->getSwapChain()->getExtent());
    skyboxPb.setDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL);

    skyboxPb.setUniformDescriptorPack(skyboxUdb.buildAndRestart());

    std::shared_ptr<Pipeline> skyboxPipeline = skyboxPb.build();

    if (skybox)
    {
        SkyboxRenderStateBuilder srsb;
        srsb.setFrameInFlightCount(m_renderer->getFrameInFlightCount());
        srsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        srsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        srsb.setDevice(mainDevice);
        srsb.setSkybox(skybox);
        srsb.setTexture(skybox->getTexture());

        srsb.setPipeline(skyboxPipeline);

        m_skyboxPhase->registerRenderState(srsb.build());
    }

    MeshBuilder mb;
    mb.setDevice(mainDevice);
    mb.setVertices({{{-1.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 0.0f, 0.0f, 1.f}, {0.f, 0.f}},
                    {{1.f, -1.f, 0.f}, {0.f, 0.f, 1.f}, {0.0f, 1.0f, 0.0f, 1.f}, {1.f, 0.f}},
                    {{1.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {0.0f, 0.0f, 1.0f, 1.f}, {1.f, 1.f}},
                    {{-1.f, 1.f, 0.f}, {0.f, 0.f, 1.f}, {1.0f, 1.0f, 1.0f, 1.f}, {0.f, 1.f}}});
    mb.setIndices({0, 1, 2, 2, 3, 0});
    std::shared_ptr<Mesh> postProcessQuad = mb.buildAndRestart();
    MeshRenderStateBuilder quadRsb;
    quadRsb.setDevice(mainDevice);
    quadRsb.setLightDescriptorEnable(false);
    quadRsb.setTextureDescriptorEnable(false);
    quadRsb.setMVPDescriptorEnable(false);
    quadRsb.setFrameInFlightCount(m_renderer->getFrameInFlightCount());
    quadRsb.setMesh(postProcessQuad);
    quadRsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    quadRsb.setDescriptorSetUpdatePred([&](const RenderPhase *parentPhase, uint32_t imageIndex, const VkDescriptorSet set) {
        auto deviceHandle = mainDevice->getHandle();
        const auto &sampler = m_window->getSwapChain()->getSampler();
        if (!sampler.has_value())
            return;

        VkDescriptorImageInfo imageInfo = {
            .sampler = *sampler.value(),
            .imageView = *parentPhase->getRenderPass()->getImageView(imageIndex),
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
    postProcessPb.setUniformDescriptorPack(postProcessUdb.buildAndRestart());
    quadRsb.setPipeline(postProcessPb.build());
    m_postProcessPhase->registerRenderState(quadRsb.build());

    initImgui();

    auto &lights = m_scene->getLights();

    CameraABC *mainCamera = m_scene->getMainCamera();

    m_scene->beginSimulation();
    while (!m_window->shouldClose())
    {
        m_timeManager.markFrame();
        float deltaTime = m_timeManager.deltaTime();

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        m_inputManager.UpdateInputStates();
        m_window->pollEvents();

        m_scene->updateSimulation(deltaTime);

        VkResult res = m_renderer->renderFrame(
            VkRect2D{
                .offset = {0, 0},
                .extent = m_window->getSwapChain()->getExtent(),
            },
            *mainCamera, lights);
        if (res == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // TODO : recreate framebuffers
            m_window->recreateSwapChain();
            m_renderer->setSwapChain(m_window->getSwapChain());
        }

        m_window->swapBuffers();
    }
}