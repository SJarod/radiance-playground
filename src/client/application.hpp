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
class Texture;

class Application
{
  private:
    std::unique_ptr<WindowGLFW> m_window;

    std::shared_ptr<Context> m_context;
    std::vector<std::shared_ptr<Device>> m_devices;

    std::shared_ptr<Renderer> m_renderer;
    RenderPhase *m_opaqueCapturePhase;
    RenderPhase *m_skyboxCapturePhase;
    RenderPhase *m_irradianceConvolutionPhase;
    RenderPhase *m_opaquePhase;
    RenderPhase *m_skyboxPhase;
    RenderPhase *m_postProcessPhase;
    RenderPhase *m_imguiPhase;
    RenderPhase *m_probesDebugPhase;

    std::shared_ptr<Texture> capturedEnvMap;
    std::shared_ptr<Texture> irradianceMap;

    std::unique_ptr<SceneABC> m_scene;

    Time::TimeManager m_timeManager;
    InputManager m_inputManager;

    void initImgui();
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