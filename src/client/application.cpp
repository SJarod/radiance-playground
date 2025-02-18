#include <assimp/Importer.hpp>

#include "graphics/context.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"

#include "wsi/window.hpp"

#include "renderer/light.hpp"
#include "renderer/mesh.hpp"
#include "renderer/render_state.hpp"
#include "renderer/renderer.hpp"
#include "renderer/scene.hpp"
#include "renderer/texture.hpp"

#include "engine/camera.hpp"
#include "engine/uniform.hpp"
#include "engine/vertex.hpp"

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

    RendererBuilder rb;
    rb.setDevice(mainDevice);
    rb.setSwapChain(m_window->getSwapChain());
    m_renderer = rb.build();
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

    m_scene = std::make_unique<Scene>(mainDevice);
    auto objects = m_scene->getObjects();
    for (int i = 0; i < objects.size(); ++i)
    {
        MeshRenderStateBuilder mrsb;
        mrsb.setFrameInFlightCount(m_window->getSwapChain()->getFrameInFlightCount());
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        mrsb.addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        mrsb.setDevice(mainDevice);
        mrsb.setTexture(objects[i]->getTexture());
        mrsb.setMesh(objects[i]);

        // material
        PipelineBuilder pb;
        PipelineDirector pd;
        pd.createColorDepthRasterizerBuilder(pb);
        pb.setDevice(mainDevice);
        pb.addVertexShaderStage("phong");
        pb.addFragmentShaderStage("phong");
        pb.setRenderPass(m_renderer->getRenderPass());
        pb.setExtent(m_window->getSwapChain()->getExtent());
        UniformDescriptorBuilder udb;
        udb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        });
        udb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
        udb.addSetLayoutBinding(VkDescriptorSetLayoutBinding{
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        });
        pb.setUniformDescriptorPack(udb.build());

        mrsb.setPipeline(pb.build());

        m_renderer->registerRenderState(mrsb.build());
    }

    auto& lights = m_scene->getLights();
    PointLight* light = static_cast<PointLight*>(lights[0].get());

    Camera camera;

    std::pair<double, double> mousePos;
    glfwGetCursorPos(m_window->getHandle(), &mousePos.first, &mousePos.second);
    glfwSetInputMode(m_window->getHandle(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    while (!m_window->shouldClose())
    {
        m_timeManager.markFrame();
        float deltaTime = m_timeManager.deltaTime();

        double xpos, ypos;
        glfwGetCursorPos(m_window->getHandle(), &xpos, &ypos);
        std::pair<double, double> deltaMousePos;
        deltaMousePos.first = mousePos.first - xpos;
        deltaMousePos.second = mousePos.second - ypos;
        mousePos.first = xpos;
        mousePos.second = ypos;

        m_window->pollEvents();

        float pitch = (float)deltaMousePos.second * camera.getSensitivity() * deltaTime;
        float yaw = (float)deltaMousePos.first * camera.getSensitivity() * deltaTime;
        Transform cameraTransform = camera.getTransform();

        cameraTransform.rotation =
            glm::quat(glm::vec3(-pitch, 0.f, 0.f)) * cameraTransform.rotation * glm::quat(glm::vec3(0.f, -yaw, 0.f));

        float xaxisInput = (glfwGetKey(m_window->getHandle(), GLFW_KEY_A) == GLFW_PRESS) -
                           (glfwGetKey(m_window->getHandle(), GLFW_KEY_D) == GLFW_PRESS);
        float zaxisInput = (glfwGetKey(m_window->getHandle(), GLFW_KEY_W) == GLFW_PRESS) -
                           (glfwGetKey(m_window->getHandle(), GLFW_KEY_S) == GLFW_PRESS);
        float yaxisInput = (glfwGetKey(m_window->getHandle(), GLFW_KEY_Q) == GLFW_PRESS) -
                           (glfwGetKey(m_window->getHandle(), GLFW_KEY_E) == GLFW_PRESS);
        glm::vec3 dir = glm::vec3(xaxisInput, yaxisInput, zaxisInput) * glm::mat3_cast(cameraTransform.rotation);
        if (!(xaxisInput == 0.f && zaxisInput == 0.f && yaxisInput == 0.f))
            dir = glm::normalize(dir);
        cameraTransform.position += camera.getSpeed() * dir * deltaTime;

        camera.setTransform(cameraTransform);

        uint32_t imageIndex = m_renderer->acquireBackBuffer();

        m_renderer->recordRenderers(imageIndex, camera, *light);

        m_renderer->submitBackBuffer();
        m_renderer->presentBackBuffer(imageIndex);

        m_renderer->swapBuffers();

        m_window->swapBuffers();
    }
}