#pragma once

#include <memory>
#include <vector>

#include "input_manager.hpp"
#include "time_manager.hpp"

class WindowGLFW;
class Context;
class Device;
class Renderer;
class SceneABC;
class RenderPhase;
class ComputePhase;
class Texture;

namespace ImGuiUtils
{
    class ProfilersWindow;
}

class Application
{
  private:
    std::unique_ptr<WindowGLFW> m_window;

    std::shared_ptr<Context> m_context;
    std::vector<std::shared_ptr<Device>> m_devices;
    std::shared_ptr<Device> m_discreteDevice;

    std::shared_ptr<Renderer> m_renderer;

    std::unique_ptr<SceneABC> m_scene;

    std::shared_ptr<ImGuiUtils::ProfilersWindow> m_profiler;

    Time::TimeManager m_timeManager;
    InputManager m_inputManager;

    /**
     * @brief exit the main loop after a certain amount of frame
     * -1 to deactivate breakage
     *
     */
    int m_breakAfterFrameCount = -1;

    void initImgui(RenderPhase *imguiPhase);
    void displayImgui();

  public:
    Application();
    ~Application();

    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    Application(Application &&) = delete;
    Application &operator=(Application &&) = delete;

    void runLoop();
};