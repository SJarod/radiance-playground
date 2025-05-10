#include "graphics/device.hpp"
#include "graphics/render_pass.hpp"

#include "renderer/render_phase.hpp"
#include "renderer/texture.hpp"

#include "wsi/window.hpp"

#include "irradiance_baked_graph.hpp"

void BakedGraph::load(std::weak_ptr<Device> device, WindowGLFW *window, uint32_t frameInFlightCount,
                      uint32_t maxProbeCount)
{
    RenderPassAttachmentBuilder rpab;
    RenderPassAttachmentDirector rpad;
    RenderPassDirector rpd;

    TextureDirector td;

    // Capture environment map
    for (int i = 0; i < maxProbeCount; i++)
    {
        CubemapBuilder captureEnvMapBuilder;
        captureEnvMapBuilder.setDevice(device);
        captureEnvMapBuilder.setWidth(256);
        captureEnvMapBuilder.setHeight(256);
        captureEnvMapBuilder.setCreateFromUserData(false);
        captureEnvMapBuilder.setDepthImageEnable(true);
        td.configureUNORMTextureBuilder(captureEnvMapBuilder);
        m_capturedEnvMaps.push_back(captureEnvMapBuilder.buildAndRestart());
    }

    // Opaque capture
    RenderPassBuilder opaqueCaptureRpb;
    opaqueCaptureRpb.setDevice(device);
    rpd.configurePooledCubemapsRenderPassBuilder(opaqueCaptureRpb, m_capturedEnvMaps, true);

    rpad.configureAttachmentClearBuilder(rpab);
    rpab.setFormat(m_capturedEnvMaps[0]->getImageFormat());
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    auto opaqueCaptureColorAttachment = rpab.buildAndRestart();
    opaqueCaptureRpb.addColorAttachment(*opaqueCaptureColorAttachment);

    rpad.configureAttachmentClearBuilder(rpab);
    rpab.setFormat(m_capturedEnvMaps[0]->getDepthImageFormat().value());
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    auto opaqueCaptureDepthAttachment = rpab.buildAndRestart();
    opaqueCaptureRpb.addDepthAttachment(*opaqueCaptureDepthAttachment);

    RenderPhaseBuilder opaqueCaptureRb;
    opaqueCaptureRb.setDevice(device);
    opaqueCaptureRb.setRenderPass(opaqueCaptureRpb.build());
    opaqueCaptureRb.setCaptureEnable(true);
    opaqueCaptureRb.setBufferingType(frameInFlightCount);
    auto opaqueCapturePhase = opaqueCaptureRb.build();
    m_opaqueCapturePhase = opaqueCapturePhase.get();

    // Skybox capture
    RenderPassBuilder skyboxCaptureRpb;
    skyboxCaptureRpb.setDevice(device);
    rpd.configurePooledCubemapsRenderPassBuilder(skyboxCaptureRpb, m_capturedEnvMaps, true);

    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(m_capturedEnvMaps[0]->getImageFormat());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto skyboxCaptureColorAttachment = rpab.buildAndRestart();
    skyboxCaptureRpb.addColorAttachment(*skyboxCaptureColorAttachment);

    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(m_capturedEnvMaps[0]->getDepthImageFormat().value());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    auto skyboxCaptureDepthAttachment = rpab.buildAndRestart();
    skyboxCaptureRpb.addDepthAttachment(*skyboxCaptureDepthAttachment);

    RenderPhaseBuilder skyboxCaptureRb;
    skyboxCaptureRb.setDevice(device);
    skyboxCaptureRb.setRenderPass(skyboxCaptureRpb.build());
    skyboxCaptureRb.setCaptureEnable(true);
    skyboxCaptureRb.setBufferingType(frameInFlightCount);
    auto skyboxCapturePhase = skyboxCaptureRb.build();
    m_skyboxCapturePhase = skyboxCapturePhase.get();

    // Irradiance cubemap
    for (int i = 0; i < maxProbeCount; i++)
    {
        CubemapBuilder irradianceMapBuilder;
        irradianceMapBuilder.setDevice(device);
        irradianceMapBuilder.setWidth(128);
        irradianceMapBuilder.setHeight(128);
        irradianceMapBuilder.setCreateFromUserData(false);
        irradianceMapBuilder.setResolveEnable(true);
        td.configureUNORMTextureBuilder(irradianceMapBuilder);
        m_irradianceMaps.push_back(irradianceMapBuilder.buildAndRestart());
    }

    // Irradiance convolution
    RenderPassBuilder irradianceConvolutionRpb;
    irradianceConvolutionRpb.setDevice(device);
    rpd.configurePooledCubemapsRenderPassBuilder(irradianceConvolutionRpb, m_irradianceMaps, true, false);
    rpad.configureAttachmentDontCareBuilder(rpab);
    rpab.setFormat(m_irradianceMaps[0]->getImageFormat());
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto irradianceColorAttachment = rpab.buildAndRestart();
    irradianceConvolutionRpb.addColorAttachment(*irradianceColorAttachment);

    RenderPhaseBuilder irradianceConvolutionRb;
    irradianceConvolutionRb.setDevice(device);
    irradianceConvolutionRb.setRenderPass(irradianceConvolutionRpb.build());
    irradianceConvolutionRb.setCaptureEnable(true);
    irradianceConvolutionRb.setBufferingType(frameInFlightCount);
    auto irradianceConvolutionPhase = irradianceConvolutionRb.build();
    m_irradianceConvolutionPhase = irradianceConvolutionPhase.get();

    // Opaque
    RenderPassBuilder opaqueRpb;
    opaqueRpb.setDevice(device);
    rpd.configureSwapChainRenderPassBuilder(opaqueRpb, *window->getSwapChain());

    rpad.configureAttachmentClearBuilder(rpab);
    rpab.setFormat(window->getSwapChain()->getImageFormat());
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    auto clearColorAttachment = rpab.buildAndRestart();
    opaqueRpb.addColorAttachment(*clearColorAttachment);

    rpad.configureAttachmentClearBuilder(rpab);
    rpab.setFormat(window->getSwapChain()->getDepthImageFormat());
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    auto clearDepthAttachment = rpab.buildAndRestart();
    opaqueRpb.addDepthAttachment(*clearDepthAttachment);

    RenderPhaseBuilder opaqueRb;
    opaqueRb.setDevice(device);
    opaqueRb.setRenderPass(opaqueRpb.build());
    opaqueRb.setBufferingType(frameInFlightCount);
    auto opaquePhase = opaqueRb.build();
    m_opaquePhase = opaquePhase.get();

    RenderPassBuilder probesDebugRpb;
    probesDebugRpb.setDevice(device);
    rpd.configureSwapChainRenderPassBuilder(probesDebugRpb, *window->getSwapChain());

    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(window->getSwapChain()->getImageFormat());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    auto probesDebugColorAttachment = rpab.buildAndRestart();
    probesDebugRpb.addColorAttachment(*probesDebugColorAttachment);

    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(window->getSwapChain()->getDepthImageFormat());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    auto probesDebugDepthAttachment = rpab.buildAndRestart();
    probesDebugRpb.addDepthAttachment(*probesDebugDepthAttachment);

    RenderPhaseBuilder probesDebugRb;
    probesDebugRb.setDevice(device);
    probesDebugRb.setRenderPass(probesDebugRpb.build());
    auto probesDebugPhase = probesDebugRb.build();
    m_probesDebugPhase = probesDebugPhase.get();

    // Skybox
    RenderPassBuilder skyboxRpb;
    skyboxRpb.setDevice(device);
    rpd.configureSwapChainRenderPassBuilder(skyboxRpb, *window->getSwapChain());

    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(window->getSwapChain()->getImageFormat());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    auto loadColorAttachment = rpab.buildAndRestart();
    skyboxRpb.addColorAttachment(*loadColorAttachment);

    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(window->getSwapChain()->getDepthImageFormat());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    auto loadDepthAttachment = rpab.buildAndRestart();
    skyboxRpb.addDepthAttachment(*loadDepthAttachment);

    RenderPhaseBuilder skyboxRb;
    skyboxRb.setDevice(device);
    skyboxRb.setRenderPass(skyboxRpb.build());
    skyboxRb.setBufferingType(frameInFlightCount);
    auto skyboxPhase = skyboxRb.build();
    m_skyboxPhase = skyboxPhase.get();

    std::unique_ptr<RenderPhase> postProcessPhase;
    {
        RenderPassBuilder passb;
        passb.setDevice(device);
        rpd.configureSwapChainRenderPassBuilder(passb, *window->getSwapChain(), false);
        rpad.configureAttachmentLoadBuilder(rpab);
        rpab.setFormat(window->getSwapChain()->getImageFormat());
        rpab.setInitialLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        rpab.setFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        auto finalLoadColorAttachment = rpab.buildAndRestart();
        passb.addColorAttachment(*finalLoadColorAttachment);
        RenderPhaseBuilder phaseb;
        phaseb.setDevice(device);
        phaseb.setRenderPass(passb.build());
        phaseb.setParentPhase(m_skyboxPhase);
        phaseb.setBufferingType(frameInFlightCount);
        phaseb.setPhaseName("Final direct");
        postProcessPhase = phaseb.build();
        m_finalImageDirect = postProcessPhase.get();
    }

    RenderPassBuilder imguiRpb;
    imguiRpb.setDevice(device);
    rpd.configureSwapChainRenderPassBuilder(imguiRpb, *window->getSwapChain(), false);

    rpad.configureAttachmentLoadBuilder(rpab);
    rpab.setFormat(window->getSwapChain()->getImageFormat());
    rpab.setInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    rpab.setFinalLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    auto imguiLoadColorAttachment = rpab.buildAndRestart();
    imguiRpb.addColorAttachment(*imguiLoadColorAttachment);

    RenderPhaseBuilder imguiRb;
    imguiRb.setDevice(device);
    imguiRb.setRenderPass(imguiRpb.build());
    imguiRb.setBufferingType(frameInFlightCount);
    auto imguiPhase = imguiRb.build();
    m_imguiPhase = imguiPhase.get();

    addOneTimeRenderPhase(std::move(opaqueCapturePhase));
    addOneTimeRenderPhase(std::move(skyboxCapturePhase));
    addOneTimeRenderPhase(std::move(irradianceConvolutionPhase));

    addRenderPhase(std::move(opaquePhase));
    addRenderPhase(std::move(probesDebugPhase));
    addRenderPhase(std::move(skyboxPhase));
    addRenderPhase(std::move(postProcessPhase));
    addRenderPhase(std::move(imguiPhase));
}