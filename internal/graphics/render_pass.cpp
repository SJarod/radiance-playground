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

void RenderPass::buildFramebuffers(const SwapChain* newSwapchain, bool clearOldFramebuffers)
{
    const VkDevice& deviceHandle = m_device.lock()->getHandle();

    if (clearOldFramebuffers)
    {
        for (VkFramebuffer& framebuffer : m_framebuffers)
        {
            vkDestroyFramebuffer(deviceHandle, framebuffer, nullptr);
        }
        
        m_views.clear();
    }

    const auto& imageViews = newSwapchain->getImageViews();
    m_framebuffers.resize(imageViews.size());
    m_framebufferBuilder.setSwapChain(newSwapchain);

    for (size_t i = 0; i < imageViews.size(); ++i)
    {
        m_framebufferBuilder.setImageView(imageViews[i]);
        m_framebuffers[i] = *m_framebufferBuilder.buildAndRestart();
        m_views.push_back(&imageViews[i]);
    }
}

std::unique_ptr<RenderPass> RenderPassBuilder::build()
{
    assert(m_device.lock());
    assert(m_swapchain);

    const VkDevice &deviceHandle = m_device.lock()->getHandle();

    m_subpass.colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentReferences.size());
    m_subpass.pColorAttachments = m_colorAttachmentReferences.data();
    m_subpass.pDepthStencilAttachment = m_depthAttachmentReferences.data();

    VkRenderPassCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(m_attachments.size()),
        .pAttachments = m_attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &m_subpass,
        .dependencyCount = 1,
        .pDependencies = &m_subpassDependency,
    };

    VkRenderPass handle;
    VkResult res = vkCreateRenderPass(deviceHandle, &createInfo, nullptr, &handle);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create render pass : " << res << std::endl;
        return nullptr;
    }

    m_product->m_handle = handle;

    m_product->m_framebufferBuilder.setRenderPass(handle);
    m_product->buildFramebuffers(m_swapchain, false);

    return std::move(m_product);
}

void RenderPassBuilder::addColorAttachment(VkAttachmentDescription attachment)
{
    VkAttachmentReference colorAttachmentRef = {
        .attachment = static_cast<uint32_t>(m_attachments.size()),
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    m_attachments.emplace_back(attachment);
    m_colorAttachmentReferences.emplace_back(colorAttachmentRef);

    m_subpassDependency.srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    m_subpassDependency.dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    m_subpassDependency.dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
}
void RenderPassBuilder::addDepthAttachment(VkAttachmentDescription attachment)
{
    VkAttachmentReference depthAttachmentRef = {
        .attachment = static_cast<uint32_t>(m_attachments.size()),
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    m_attachments.emplace_back(attachment);
    m_depthAttachmentReferences.emplace_back(depthAttachmentRef);

    m_subpassDependency.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    m_subpassDependency.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    m_subpassDependency.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    m_product->m_framebufferBuilder.setHasDepthAttached(true);
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

std::unique_ptr<VkFramebuffer> RenderPassFramebufferBuilder::build() 
{
    assert(m_device.lock());

    const VkDevice& deviceHandle = m_device.lock()->getHandle();

    std::vector<VkImageView> framebufferAttachments{ m_imageView};

    if (m_hasDepthAttachment)
        framebufferAttachments.push_back(m_swapchain->getDepthImageView());

    VkFramebufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = m_renderPassHandle,
        .attachmentCount = static_cast<uint32_t>(framebufferAttachments.size()),
        .pAttachments = framebufferAttachments.data(),
        .width = m_swapchain->getExtent().width,
        .height = m_swapchain->getExtent().height,
        .layers = 1,
    };

    VkResult res = vkCreateFramebuffer(deviceHandle, &createInfo, nullptr, m_product.get());
    if (res != VK_SUCCESS)
        std::cerr << "Failed to create framebuffer : " << res << std::endl;

    return std::move(m_product);
}