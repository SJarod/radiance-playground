#include <array>
#include <cassert>
#include <iostream>

#include "device.hpp"
#include "swapchain.hpp"

#include "render_pass.hpp"

RenderPass::~RenderPass()
{
    if (!m_device.lock())
        return;

    const VkDevice &deviceHandle = m_device.lock()->getHandle();

    for (VkFramebuffer &framebuffer : m_framebuffers)
    {
        vkDestroyFramebuffer(deviceHandle, framebuffer, nullptr);
    }
    vkDestroyRenderPass(deviceHandle, m_handle, nullptr);
}

std::unique_ptr<RenderPass> RenderPassBuilder::build()
{
    assert(!m_device.expired());
    const VkDevice &deviceHandle = m_device.lock()->getHandle();



    return std::move(m_product);
}

void RenderPassAttachmentDirector::configureAttachmentDontCareBuilder(RenderPassAttachmentBuilder &builder)
{
    builder.setSamples(VK_SAMPLE_COUNT_1_BIT);
    builder.setLoadOp(VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    builder.setStoreOp(VK_ATTACHMENT_STORE_OP_STORE);
    builder.setStencilLoadOp(VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    builder.setStencilStoreOp(VK_ATTACHMENT_STORE_OP_DONT_CARE);
    builder.setInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
}
void RenderPassAttachmentDirector::configureAttachmentClearBuilder(RenderPassAttachmentBuilder &builder)
{
    builder.setSamples(VK_SAMPLE_COUNT_1_BIT);
    builder.setLoadOp(VK_ATTACHMENT_LOAD_OP_CLEAR);
    builder.setStoreOp(VK_ATTACHMENT_STORE_OP_STORE);
    builder.setStencilLoadOp(VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    builder.setStencilStoreOp(VK_ATTACHMENT_STORE_OP_DONT_CARE);
    builder.setInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
}
void RenderPassAttachmentDirector::configureAttachmentLoadBuilder(RenderPassAttachmentBuilder &builder)
{
    builder.setSamples(VK_SAMPLE_COUNT_1_BIT);
    builder.setLoadOp(VK_ATTACHMENT_LOAD_OP_LOAD);
    builder.setStoreOp(VK_ATTACHMENT_STORE_OP_STORE);
    builder.setStencilLoadOp(VK_ATTACHMENT_LOAD_OP_DONT_CARE);
    builder.setStencilStoreOp(VK_ATTACHMENT_STORE_OP_DONT_CARE);
}
