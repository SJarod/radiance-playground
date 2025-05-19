#include <assimp/Importer.hpp>
#include <format>
#include <memory>
#include <string>

#include <tracy/Tracy.hpp>

// not included in LegitProfiler ?
// needed in LegitProfiler ?
#include <chrono>
#include <glm/glm.hpp>
#include <sstream>

#include "ImGuiProfilerRenderer.h"

#include "graphics/buffer.hpp"
#include "graphics/context.hpp"
#include "graphics/device.hpp"
#include "graphics/image.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/render_pass.hpp"

#include "wsi/window.hpp"

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"

#include "engine/camera.hpp"

#include "renderer/light.hpp"
#include "renderer/mesh.hpp"
#include "renderer/model.hpp"
#include "renderer/render_graph.hpp"
#include "renderer/render_phase.hpp"
#include "renderer/render_state.hpp"
#include "renderer/renderer.hpp"
#include "renderer/scene.hpp"
#include "renderer/skybox.hpp"
#include "renderer/texture.hpp"

#include "application.hpp"

#define PROFILER_BEGINSCOPE(name)
#define PROFILER_ENDSCOPE()

constexpr uint32_t bufferingType = 3;
constexpr uint32_t maxProbeCount = 64u;

Application::Application()
{
    WindowGLFW::init();
    m_profiler = std::make_unique<ImGuiUtils::ProfilersWindow>();
    m_window = std::make_unique<WindowGLFW>();

    glfwSetKeyCallback(m_window->getHandle(), InputManager::KeyCallback);
    ContextBuilder cb;
#ifndef NDEBUG
    cb.addLayer("VK_LAYER_KHRONOS_validation");
#endif
    cb.addLayer("VK_LAYER_LUNARG_monitor");
    cb.addLayer("VK_LAYER_KHRONOS_synchronization2");
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
        db.addDeviceExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
        db.addDeviceExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
        db.addDeviceExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME);
        db.addDeviceExtension(VK_KHR_RAY_TRACING_POSITION_FETCH_EXTENSION_NAME);
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
}

Application::~Application()
{
    // ensure all commands have finished
    vkDeviceWaitIdle(m_discreteDevice->getHandle());

    m_window.reset();

    m_discreteDevice.reset();
    m_devices.clear();

    m_context.reset();

    WindowGLFW::terminate();
}

void Application::initImgui(RenderPhase *imguiPhase)
{
    ImGuiRenderStateBuilder imguirsb;

    imguirsb.setDevice(m_discreteDevice);
    imguirsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

    ImGui::CreateContext();
    if (!ImGui_ImplGlfw_InitForVulkan(m_window->getHandle(), true))
    {
        std::cerr << "Failed to initialize ImGui GLFW Implemenation For Vulkan" << std::endl;
        throw;
    }

    std::shared_ptr<RenderStateABC> render_state = RENDER_STATE_PTR(imguirsb.build());
    imguiPhase->registerRenderStateToAllPool(render_state);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = m_context->getInstanceHandle();
    init_info.PhysicalDevice = m_discreteDevice->getPhysicalHandle();
    init_info.Device = m_discreteDevice->getHandle();
    init_info.Queue = m_discreteDevice->getGraphicsQueue();
    init_info.DescriptorPool = render_state->getDescriptorPool();
    init_info.MinImageCount = 2;
    init_info.ImageCount = m_window->getSwapChain()->getImages().size();
    init_info.RenderPass = imguiPhase->getRenderPass()->getHandle();

    if (!ImGui_ImplVulkan_Init(&init_info))
    {
        std::cerr << "Failed to initialize ImGui Implementation for Vulkan" << std::endl;
        throw;
    }
}

int Application::displayImgui()
{
    ZoneScoped;

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    m_profiler->Render();

    ImGui::Begin("Radiance playground");

    if (ImGui::Button("Next scene"))
    {
        return 1;
    }

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

                ImGui::ColorEdit3("Diffuse Color", &light->diffuseColor.x);
                ImGui::DragFloat("Diffuse Power", &light->diffusePower);

                ImGui::ColorEdit3("Specular Color", &light->specularColor.x);
                ImGui::DragFloat("Specular Power", &light->specularPower);

                ImGui::PopID();

                ImGui::TreePop();
            }

            lightIndex++;
        }
    }

    ImGui::End();

    return 0;
}

#include "render_graphs/global_illumination_with_irradiance_probes/graph_g2ip.hpp"
#include "render_graphs/global_illumination_with_irradiance_probes/graph_g2iprt.hpp"
#include "render_graphs/radiance_cascades/graph_rc2d.hpp"
#include "render_graphs/radiance_cascades/graph_rc3d.hpp"
#include "render_graphs/radiance_cascades/graph_rc3drt.hpp"
#include "scenes/global_illumination_with_irradiance_probes/scene_g2ip.hpp"
#include "scenes/global_illumination_with_irradiance_probes/scene_g2iprt.hpp"
#include "scenes/radiance_cascades/scene_rc2d.hpp"
#include "scenes/radiance_cascades/scene_rc3d.hpp"
#include "scenes/radiance_cascades/scene_rc3drt.hpp"

static int sceneIndex = 4;
constexpr int sceneCount = 5;

int Application::runLoop()
{
    m_window->makeContextCurrent();

    bool show_demo_window = true;

    RendererBuilder rb;
    rb.setDevice(m_discreteDevice);
    rb.setSwapChain(m_window->getSwapChain());
    rb.setFrameInFlightCount(bufferingType);

    switch (sceneIndex)
    {
    case 0:
        rb.setRenderGraph(
            RenderGraphLoader::load<GraphG2IP>(m_discreteDevice, m_window.get(), bufferingType, maxProbeCount));
        m_renderer = rb.build();
        m_scene = SceneABC::load<SceneG2IP>(m_context, m_discreteDevice, m_window.get(), m_renderer->getRenderGraph(),
                                            bufferingType, maxProbeCount);
        break;
    case 1:
        rb.setRenderGraph(
            RenderGraphLoader::load<GraphG2IPRT>(m_discreteDevice, m_window.get(), bufferingType, maxProbeCount));
        m_renderer = rb.build();
        m_scene = SceneABC::load<SceneG2IPRT>(m_context, m_discreteDevice, m_window.get(), m_renderer->getRenderGraph(),
                                              bufferingType, maxProbeCount);
        break;
    case 2:
        rb.setRenderGraph(
            RenderGraphLoader::load<GraphRC2D>(m_discreteDevice, m_window.get(), bufferingType, maxProbeCount));
        m_renderer = rb.build();
        m_scene = SceneABC::load<SceneRC2D>(m_context, m_discreteDevice, m_window.get(), m_renderer->getRenderGraph(),
                                            bufferingType, maxProbeCount);
        break;
    case 3:
        rb.setRenderGraph(
            RenderGraphLoader::load<GraphRC3D>(m_discreteDevice, m_window.get(), bufferingType, maxProbeCount));
        m_renderer = rb.build();
        m_scene = SceneABC::load<SceneRC3D>(m_context, m_discreteDevice, m_window.get(), m_renderer->getRenderGraph(),
                                            bufferingType, maxProbeCount);
        break;
    case 4:
        rb.setRenderGraph(
            RenderGraphLoader::load<GraphRC3DRT>(m_discreteDevice, m_window.get(), bufferingType, maxProbeCount));
        m_renderer = rb.build();
        m_scene = SceneABC::load<SceneRC3DRT>(m_context, m_discreteDevice, m_window.get(), m_renderer->getRenderGraph(),
                                              bufferingType, maxProbeCount);
        break;
    default:
        assert(false);
        break;
    }

    RenderPhase *imguiPhase = nullptr;
    if (GraphG2IP *rg = dynamic_cast<GraphG2IP *>(m_renderer->getRenderGraph()))
        imguiPhase = rg->m_imguiPhase;
    else if (GraphG2IPRT *rg = dynamic_cast<GraphG2IPRT *>(m_renderer->getRenderGraph()))
        imguiPhase = rg->m_imguiPhase;
    else if (GraphRC2D *rg = dynamic_cast<GraphRC2D *>(m_renderer->getRenderGraph()))
        imguiPhase = rg->m_imguiPhase;
    else if (GraphRC3D *rg = dynamic_cast<GraphRC3D *>(m_renderer->getRenderGraph()))
        imguiPhase = rg->m_imguiPhase;
    else if (GraphRC3DRT *rg = dynamic_cast<GraphRC3DRT *>(m_renderer->getRenderGraph()))
        imguiPhase = rg->m_imguiPhase;

    if (imguiPhase)
        initImgui(imguiPhase);

    auto &lights = m_scene->getLights();
    std::shared_ptr<ProbeGrid> grid = nullptr;
    if (SceneG2IP *sc = dynamic_cast<SceneG2IP *>(m_scene.get()))
        grid = sc->m_grid;
    else if (SceneG2IPRT *sc = dynamic_cast<SceneG2IPRT *>(m_scene.get()))
        grid = sc->m_grid;
    else if (SceneRC3D *sc = dynamic_cast<SceneRC3D *>(m_scene.get()))
        grid = sc->m_grid0;
    else if (SceneRC3DRT *sc = dynamic_cast<SceneRC3DRT *>(m_scene.get()))
        grid = sc->m_grid0;

    CameraABC *mainCamera = m_scene->getMainCamera();

    m_scene->beginSimulation();
    while (!m_window->shouldClose())
    {
        ZoneScoped;

        m_timeManager.markFrame();
        float deltaTime = m_timeManager.deltaTime();

        if (displayImgui())
            break;

        m_inputManager.UpdateInputStates();
        m_window->pollEvents();

        m_scene->updateSimulation(deltaTime);

        VkResult res = m_renderer->renderFrame(
            VkRect2D{
                .offset = {0, 0},
                .extent = m_window->getSwapChain()->getExtent(),
            },
            *mainCamera, lights, grid);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
        {
            m_window->recreateSwapChain();
            m_renderer->setSwapChain(m_window->getSwapChain());
            if (auto cam = dynamic_cast<PerspectiveCamera *>(m_scene->getMainCamera()))
                cam->setAspectRatio(m_window->getAspectRatio());
        }

        m_window->swapBuffers();

        FrameMark;

        static int frameCounter = 0;
        if (++frameCounter == m_breakAfterFrameCount)
            break;
    }

    // ensure all commands have finished before destroying objects in this scope
    vkDeviceWaitIdle(m_discreteDevice->getHandle());

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    m_renderer.reset();
    m_scene.reset();

    sceneIndex = (sceneIndex + 1) % sceneCount;
    return !m_window->shouldClose();
}