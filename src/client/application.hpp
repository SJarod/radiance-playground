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

class Application
{
  private:
    std::unique_ptr<WindowGLFW> m_window;

    std::shared_ptr<Context> m_context;
    std::vector<std::shared_ptr<Device>> m_devices;
    std::shared_ptr<Device> m_discreteDevice;

    std::shared_ptr<Renderer> m_renderer;

    RenderPhase *m_opaqueCapturePhase;
    RenderPhase *m_skyboxCapturePhase;

    RenderPhase *m_irradianceConvolutionPhase;
    RenderPhase *m_opaquePhase;
    RenderPhase *m_skyboxPhase;

    /**
     * @brief final image with direct lighting
     * post process can be applied in this phase using the right shader
     *
     */
    RenderPhase *m_finalImageDirect;
    /**
     * @brief compute shader for the radiance gathering
     *
     */
    ComputePhase *m_computePhase;
    /**
     * @brief final image combining direct and indirect lighting
     *
     */
    RenderPhase *m_finalImageDirectIndirect;

    RenderPhase *m_imguiPhase;
    RenderPhase *m_probesDebugPhase;

    std::vector<std::shared_ptr<Texture>> capturedEnvMaps;
    std::vector<std::shared_ptr<Texture>> irradianceMaps;

    std::unique_ptr<SceneABC> m_scene;

    Time::TimeManager m_timeManager;
    InputManager m_inputManager;

    /**
     * @brief exit the main loop after a certain amount of frame
     * -1 to deactivate breakage
     *
     */
    int m_breakAfterFrameCount = 2;

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