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
    m_window->setSwapChain(scb.build());

    RenderPassBuilder phongRpb;
    phongRpb.setDevice(mainDevice);
    RenderPassAttachmentBuilder rpab;
    RenderPassAttachmentDirector rpad;
    rpad.configureAttachmentClearBuilder(rpab);
    rpab.setFormat(m_window->getSwapChain()->getImageFormat());
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    auto clearColorAttachment = rpab.buildAndRestart();
    phongRpb.addAttachmentView(m_window->getSwapChain()->getImageViews()[0]);
    phongRpb.addAttachment({});
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

    RendererBuilder rb;
    rb.setDevice(mainDevice);
    rb.setSwapChain(m_window->getSwapChain());
    std::unique_ptr<RenderGraph> renderGraph = std::make_unique<RenderGraph>();
    renderGraph->addRenderPhase(std::move(phongPhase));
    rb.setRenderGraph(std::move(renderGraph));
    m_renderer = rb.build();
}

Application::~Application()
{
    vkDeviceWaitIdle(m_devices[1]->getHandle());

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
    for (int i = 0; i < objects.size(); ++i)
    {
        MeshRenderStateBuilder mrsb;
        mrsb.setFrameInFlightCount(m_renderer->getFrameInFlightCount());
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
        mrsb.setDevice(mainDevice);
        mrsb.setTexture(objects[i]->getTexture());
        mrsb.setMesh(objects[i]);
        mrsb.setPipeline(phongPipeline);
        m_phongPhase->registerRenderState(mrsb.build());
    }

    auto &lights = m_scene->getLights();

    CameraABC *mainCamera = m_scene->getMainCamera();

    m_scene->beginSimulation();
    while (!m_window->shouldClose())
    {
        m_timeManager.markFrame();
        float deltaTime = m_timeManager.deltaTime();


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