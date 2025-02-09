#pragma once

#include <memory>
#include <vector>

#include "time_manager.hpp"

class WindowGLFW;
class Context;
class Device;
class Renderer;
class Scene;

class Application
{
  private:
    std::unique_ptr<WindowGLFW> m_window;

    std::shared_ptr<Context> m_context;
    std::vector<std::shared_ptr<Device>> m_devices;

    std::shared_ptr<Renderer> m_renderer;

    std::unique_ptr<Scene> m_scene;

    Time::TimeManager m_timeManager;

  public:
    Application();
    ~Application();

    void runLoop();
};