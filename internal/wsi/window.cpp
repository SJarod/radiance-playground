#include <iostream>

#include <tracy/Tracy.hpp>

#include <window.hpp>

#include "graphics/device.hpp"
#include "graphics/surface.hpp"
#include "graphics/swapchain.hpp"

int WindowGLFW::init()
{
    return glfwInit();
}

void WindowGLFW::terminate()
{
    glfwTerminate();
}

WindowGLFW::WindowGLFW()
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_handle = glfwCreateWindow(m_width, m_height, "Playground", NULL, NULL);
    if (!m_handle)
        throw;
}

WindowGLFW::~WindowGLFW()
{
    m_swapchain.reset();
    m_surface.reset();
}

void WindowGLFW::makeContextCurrent()
{
    glfwMakeContextCurrent(m_handle);
}

bool WindowGLFW::shouldClose()
{
    return glfwWindowShouldClose(m_handle);
}

void WindowGLFW::swapBuffers()
{
    glfwSwapBuffers(m_handle);
}

void WindowGLFW::pollEvents()
{
    ZoneScoped;

    glfwPollEvents();
}

const std::vector<const char *> WindowGLFW::getRequiredExtensions() const
{
    uint32_t count = 0;
    const char **extensions;

    extensions = glfwGetRequiredInstanceExtensions(&count);

    return std::vector<const char *>(extensions, extensions + count);
}

VkResult WindowGLFW::createSurfacePredicate(VkInstance instance, void *windowHandle, VkAllocationCallbacks *allocator,
                                            VkSurfaceKHR *surface)
{
    VkResult res = glfwCreateWindowSurface(instance, (GLFWwindow *)windowHandle, allocator, surface);
    return res;
}

void WindowGLFW::recreateSwapChain()
{
    ZoneScoped;

    vkDeviceWaitIdle(m_swapchain->getDevice().lock()->getHandle());

    SwapChainBuilder scb;
    scb.setDevice(m_swapchain->getDevice());
    glfwGetWindowSize(m_handle, &m_width, &m_height);
    scb.setWidth(m_width);
    scb.setHeight(m_height);
    scb.setSwapchainImageFormat(VkSurfaceFormatKHR{
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
    });
    scb.setSwapchainPresentMode(VK_PRESENT_MODE_IMMEDIATE_KHR);
    scb.setUseImagesAsSamplers(true);
    m_swapchain.reset();
    m_swapchain = scb.build();
}
