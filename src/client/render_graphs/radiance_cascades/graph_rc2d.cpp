#include "graphics/render_pass.hpp"

#include "renderer/render_phase.hpp"
#include "renderer/texture.hpp"

#include "wsi/window.hpp"

#include "graph_rc2d.hpp"

void GraphRC2D::load(std::weak_ptr<Device> device, WindowGLFW *window, uint32_t frameInFlightCount,
                     uint32_t maxProbeCount)
{
    RenderPassAttachmentBuilder rpab;
    RenderPassAttachmentDirector rpad;
    RenderPassDirector rpd;

    TextureDirector td;

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

    RenderPhaseBuilder<RenderTypeE::RASTER> opaqueRb;
    opaqueRb.setDevice(device);
    opaqueRb.setRenderPass(opaqueRpb.build());
    opaqueRb.setPhaseName("Opaque");
    opaqueRb.setBufferingType(frameInFlightCount);
    auto opaquePhase = opaqueRb.build();
    m_opaquePhase = opaquePhase.get();

    std::unique_ptr<RenderPhase> postProcessPhase;
    {
        RenderPassBuilder passb;
        passb.setDevice(device);
        rpd.configureSwapChainRenderPassBuilder(passb, *window->getSwapChain(), false);
        rpad.configureAttachmentLoadBuilder(rpab);
        rpab.setFormat(window->getSwapChain()->getImageFormat());
        rpab.setInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        rpab.setFinalLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        auto finalLoadColorAttachment = rpab.buildAndRestart();
        passb.addColorAttachment(*finalLoadColorAttachment);
        RenderPhaseBuilder<RenderTypeE::RASTER> phaseb;
        phaseb.setDevice(device);
        phaseb.setRenderPass(passb.build());
        phaseb.setBufferingType(frameInFlightCount);
        phaseb.setPhaseName("Final direct");
        postProcessPhase = phaseb.build();
        m_finalImageDirect = postProcessPhase.get();
    }

    std::unique_ptr<ComputePhase> computePhase;
    {
        ComputePhaseBuilder cpb;
        cpb.setDevice(device);
        cpb.setBufferingType(frameInFlightCount);
        cpb.setPhaseName("Compute");
        computePhase = cpb.build();
        m_computePhase = computePhase.get();
    }

    std::unique_ptr<RenderPhase> postProcess2Phase;
    {
        RenderPassBuilder passb;
        passb.setDevice(device);
        rpd.configureSwapChainRenderPassBuilder(passb, *window->getSwapChain(), false);
        rpad.configureAttachmentLoadBuilder(rpab);
        rpab.setFormat(window->getSwapChain()->getImageFormat());
        rpab.setInitialLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        rpab.setFinalLayout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        auto finalLoadColorAttachment = rpab.buildAndRestart();
        passb.addColorAttachment(*finalLoadColorAttachment);
        passb.addFragmentShaderSubpassDependencyToItself();
        RenderPhaseBuilder<RenderTypeE::RASTER> phaseb;
        phaseb.setDevice(device);
        phaseb.setRenderPass(passb.build());
        phaseb.setPhaseName("Final direct + indirect");
        phaseb.setBufferingType(frameInFlightCount);
        postProcess2Phase = phaseb.build();
        m_finalImageDirectIndirect = postProcess2Phase.get();
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

    RenderPhaseBuilder<RenderTypeE::RASTER> imguiRb;
    imguiRb.setDevice(device);
    imguiRb.setPhaseName("Dear ImGui");
    imguiRb.setRenderPass(imguiRpb.build());
    imguiRb.setBufferingType(frameInFlightCount);
    auto imguiPhase = imguiRb.build();
    m_imguiPhase = imguiPhase.get();

    addRenderPhase(std::move(opaquePhase));
    addRenderPhase(std::move(postProcessPhase));
    addPhase(std::move(computePhase));
    addRenderPhase(std::move(postProcess2Phase));
    addRenderPhase(std::move(imguiPhase));
}