#include <assimp/Importer.hpp>

#include "graphics/context.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/render_pass.hpp"

#include "wsi/window.hpp"

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

    std::weak_ptr<Device> mainDevice = m_devices[0];

    m_window->setSwapChain(std::move(std::make_unique<SwapChain>(mainDevice)));

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

    // TODO : test load/load and dontcare/dontcare
    rpad.configureAttachmentDontCareBuilder(rpab);
    rpab.setFormat(m_window->getSwapChain()->getImageFormat());
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
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

    // TODO : renderer builder
    m_renderer = std::make_unique<Renderer>();
    m_renderer->m_device = mainDevice;
    m_renderer->m_swapchain = m_window->getSwapChain();
    // TODO : render graph builder
    m_renderer->m_renderGraph = std::make_unique<RenderGraph>();
    m_renderer->m_renderGraph->m_renderPhases.push_back(std::move(phongPhase));
    m_renderer->m_renderGraph->m_renderPhases.push_back(std::move(skyboxPhase));
}

Application::~Application()
{
    m_renderer.reset();
    m_scene.reset();

    m_window.reset();

    m_devices.clear();
    m_context.reset();

    WindowGLFW::terminate();
}

void Application::runLoop()
{
    std::shared_ptr<Device> mainDevice = m_devices[0];

    m_window->makeContextCurrent();

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
    phongPb.setUniformDescriptorPack(phongUdb.build());

    std::shared_ptr<Pipeline> phongPipeline = phongPb.build();

    m_scene = std::make_unique<SampleScene>(mainDevice, m_window.get());
    auto objects = m_scene->getObjects();
    for (int i = 0; i < objects.size(); ++i)
    {
        MeshRenderStateBuilder mrsb;
        mrsb.setFrameInFlightCount(m_window->getSwapChain()->getFrameInFlightCount());
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

    skyboxPb.setUniformDescriptorPack(skyboxUdb.build());

    std::shared_ptr<Pipeline> skyboxPipeline = skyboxPb.build();

    if (std::shared_ptr<Skybox> skybox = m_scene->getSkybox())
    {
        SkyboxRenderStateBuilder srsb;
        srsb.setFrameInFlightCount(m_window->getSwapChain()->getFrameInFlightCount());
        srsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        srsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        srsb.setDevice(mainDevice);
        srsb.setSkybox(skybox);
        srsb.setTexture(skybox->getTexture());

        srsb.setPipeline(skyboxPipeline);

        m_phongPhase->registerRenderState(srsb.build());
    }

    auto &lights = m_scene->getLights();


    CameraABC *mainCamera = m_scene->getMainCamera();

    m_scene->beginSimulation();
    while (!m_window->shouldClose())
    {
        m_timeManager.markFrame();
        float deltaTime = m_timeManager.deltaTime();

        m_window->pollEvents();

        m_scene->updateSimulation(deltaTime);

        uint32_t imageIndex = m_renderer->acquireNextSwapChainImage();

        m_renderer->m_renderGraph->processRendering(imageIndex,
                                                    VkRect2D{
                                                        .offset = {0, 0},
                                                        .extent = m_window->getSwapChain()->getExtent(),
                                                    },
                                                    *mainCamera, lights);
        m_renderer->presentBackBuffer(imageIndex);

        m_renderer->m_renderGraph->swapAllRenderPhasesBackBuffers();

        m_window->swapBuffers();
    }
}