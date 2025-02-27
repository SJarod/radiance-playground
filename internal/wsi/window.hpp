#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include "graphics/surface.hpp"
#include "graphics/swapchain.hpp"

class WindowI
{
    virtual void makeContextCurrent() = 0;
    virtual bool shouldClose() = 0;

    virtual void swapBuffers() = 0;
    virtual void pollEvents() = 0;

    virtual const std::vector<const char *> getRequiredExtensions() const = 0;
};

class WindowGLFW : public WindowI
{
  private:
    GLFWwindow *m_handle;

    int m_width = 1366;
    int m_height = 768;

    std::unique_ptr<Surface> m_surface;
    std::unique_ptr<SwapChain> m_swapchain;

  public:
    WindowGLFW();
    ~WindowGLFW();

    WindowGLFW(const WindowGLFW &) = delete;
    WindowGLFW &operator=(const WindowGLFW &) = delete;
    WindowGLFW(WindowGLFW &&) = delete;
    WindowGLFW &operator=(WindowGLFW &&) = delete;

    static int init();
    static void terminate();

    void makeContextCurrent() override;
    bool shouldClose() override;

    void swapBuffers() override;
    void pollEvents() override;

    const std::vector<const char *> getRequiredExtensions() const override;

    static VkResult createSurfacePredicate(VkInstance instance, void *windowHandle, VkAllocationCallbacks *allocator,
                                           VkSurfaceKHR *surface);

    void recreateSwapChain();

  public:
    [[nodiscard]] inline GLFWwindow *getHandle() const
    {
        return m_handle;
    }

    [[nodiscard]] inline const Surface *getSurface() const
    {
        return m_surface.get();
    }
    [[nodiscard]] inline const SwapChain *getSwapChain() const
    {
        return m_swapchain.get();
    }

  public:
    void setSurface(std::unique_ptr<Surface> surface)
    {
        m_surface = std::move(surface);
    }
    void setSwapChain(std::unique_ptr<SwapChain> swapchain)
    {
        m_swapchain = std::move(swapchain);
    }
};