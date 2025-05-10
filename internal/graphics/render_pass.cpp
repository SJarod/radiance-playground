#include <array>
#include <cassert>
#include <iostream>

#include "device.hpp"
#include "renderer/texture.hpp"
#include "swapchain.hpp"

#include "render_pass.hpp"

RenderPass::~RenderPass()
{
    if (!m_device.lock())
        return;

    const VkDevice &deviceHandle = m_device.lock()->getHandle();

    for (size_t i = 0; i < m_pooledFramebuffers.size(); i++)
    {
        for (VkFramebuffer &framebuffer : m_pooledFramebuffers[i])
        {
            vkDestroyFramebuffer(deviceHandle, framebuffer, nullptr);
        }
    }

    vkDestroyRenderPass(deviceHandle, m_handle, nullptr);
}

void RenderPass::buildFramebuffers(const std::vector<std::vector<VkImageView>> &pooledImageViews,
                                   const std::optional<std::vector<VkImageView>> &pooledDepthAttachments,
                                   VkExtent2D extent, uint32_t layerCount, bool clearOldFramebuffers)
{
    const VkDevice &deviceHandle = m_device.lock()->getHandle();

    m_minRenderArea.offset = {0, 0};
    m_minRenderArea.extent = extent;

    for (size_t i = 0; i < m_poolSize; i++)
    {
        std::vector<VkImageView> &views = m_pooledViews[i];
        std::vector<VkFramebuffer> &framebuffers = m_pooledFramebuffers[i];

        if (clearOldFramebuffers)
        {
            for (VkFramebuffer &framebuffer : framebuffers)
            {
                vkDestroyFramebuffer(deviceHandle, framebuffer, nullptr);
            }

            views.clear();
        }

        framebuffers.resize(pooledImageViews[i].size());

        RenderPassFramebufferBuilder &framebufferBuilder = m_pooledFramebufferBuilders[i];

        if (pooledDepthAttachments.has_value())
            framebufferBuilder.setDepthAttachment(pooledDepthAttachments.value()[i]);

        for (size_t j = 0; j < pooledImageViews[i].size(); ++j)
        {
            framebufferBuilder.setExtent(extent);
            framebufferBuilder.setLayerCount(layerCount);
            framebufferBuilder.setColorAttachment(pooledImageViews[i][j]);

            framebuffers[j] = *framebufferBuilder.buildAndRestart();
            views.push_back(pooledImageViews[i][j]);
        }
    }
}

std::unique_ptr<RenderPass> RenderPassBuilder::build()
{
    assert(m_device.lock());

    const VkDevice &deviceHandle = m_device.lock()->getHandle();

    m_subpass.colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentReferences.size());
    m_subpass.pColorAttachments = m_colorAttachmentReferences.data();
    m_subpass.pDepthStencilAttachment =
        m_depthAttachmentReference.has_value() ? &m_depthAttachmentReference.value() : nullptr;
    ;

    VkRenderPassCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<uint32_t>(m_attachments.size()),
        .pAttachments = m_attachments.data(),
        .subpassCount = 1,
        .pSubpasses = &m_subpass,
        .dependencyCount = static_cast<uint32_t>(m_subpassDependency.size()),
        .pDependencies = m_subpassDependency.data(),
    };

    VkRenderPass handle;
    VkResult res;

    if (m_multiviewEnable == false)
    {
        res = vkCreateRenderPass(deviceHandle, &createInfo, nullptr, &handle);
    }
    else
    {
        std::vector<uint32_t> viewMasks = {0b00111111};
        VkRenderPassMultiviewCreateInfo multiviewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
            .subpassCount = 1,
            .pViewMasks = viewMasks.data(),
        };

        createInfo.pNext = &multiviewCreateInfo;

        res = vkCreateRenderPass(deviceHandle, &createInfo, nullptr, &handle);
    }

    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create render pass : " << res << std::endl;
        return nullptr;
    }

    m_product->m_handle = handle;
    m_product->m_poolSize = m_poolSize;

    m_product->m_pooledFramebufferBuilders.reserve(m_product->m_poolSize);
    for (size_t i = 0; i < m_product->m_poolSize; i++)
    {
        RenderPassFramebufferBuilder framebufferBuilder;

        if (m_depthAttachmentReference.has_value())
            framebufferBuilder.setHasDepthAttached(true);

        framebufferBuilder.setDevice(m_device);
        framebufferBuilder.setRenderPass(handle);

        m_product->m_pooledFramebufferBuilders.push_back(std::move(framebufferBuilder));

        m_product->m_pooledViews.push_back({});
        m_product->m_pooledFramebuffers.push_back({});
    }

    m_product->buildFramebuffers(m_pooledImageViews, m_pooledDepthAttachments, m_extent,
                                 m_multiviewEnable ? 1u : m_layers);

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

    m_subpassDependency[0].srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    m_subpassDependency[0].dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    m_subpassDependency[0].dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
}
void RenderPassBuilder::addDepthAttachment(VkAttachmentDescription attachment)
{
    VkAttachmentReference depthAttachmentRef = {
        .attachment = static_cast<uint32_t>(m_attachments.size()),
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    m_attachments.emplace_back(attachment);
    m_depthAttachmentReference = depthAttachmentRef;

    m_subpassDependency[0].srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    m_subpassDependency[0].dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    m_subpassDependency[0].dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
}

void RenderPassDirector::configureSwapChainRenderPassBuilder(RenderPassBuilder &builder, const SwapChain &swapchain,
                                                             bool hasDepthAttachment)
{
    builder.setExtent(swapchain.getExtent());
    builder.setLayerCount(1u);
    builder.setImageResources(swapchain.getImages());
    builder.setImageViews(swapchain.getImageViews());

    if (hasDepthAttachment)
        builder.setDepthAttachment(swapchain.getDepthImageView());
}

void RenderPassDirector::configureCubemapRenderPassBuilder(RenderPassBuilder &builder, const Texture &cubemap,
                                                           bool useMultiview, bool hasDepthAttachment)
{
    builder.setExtent({cubemap.getWidth(), cubemap.getHeight()});
    builder.setImageViews({cubemap.getImageView()});
    builder.setLayerCount(6u);

    if (useMultiview)
        builder.setMultiviewUsageEnable(useMultiview);

    if (hasDepthAttachment && cubemap.getDepthImageView().has_value())
        builder.setDepthAttachment(cubemap.getDepthImageView().value());
}

void RenderPassDirector::configurePooledCubemapsRenderPassBuilder(RenderPassBuilder &builder,
                                                                  const std::vector<std::shared_ptr<Texture>> &cubemaps,
                                                                  bool useMultiview, bool hasDepthAttachment)
{
    const std::shared_ptr<Texture> &firstCubemap = cubemaps.front();
    builder.setExtent({firstCubemap->getWidth(), firstCubemap->getHeight()});
    builder.setLayerCount(6u);

    if (useMultiview)
        builder.setMultiviewUsageEnable(useMultiview);

    for (size_t i = 0u; i < cubemaps.size(); i++)
    {
        builder.addPooledImageViews({cubemaps[i]->getImageView()});

        if (hasDepthAttachment && cubemaps[i]->getDepthImageView().has_value())
            builder.addPooledDepthAttachment(cubemaps[i]->getDepthImageView().value());
    }
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

    const VkDevice &deviceHandle = m_device.lock()->getHandle();

    std::vector<VkImageView> framebufferAttachments{m_colorAttachment};

    if (m_depthAttachment.has_value())
        framebufferAttachments.push_back(m_depthAttachment.value());

    VkFramebufferCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = m_renderPassHandle,
        .attachmentCount = static_cast<uint32_t>(framebufferAttachments.size()),
        .pAttachments = framebufferAttachments.data(),
        .width = m_extent.width,
        .height = m_extent.height,
        .layers = m_layers,
    };

    VkResult res = vkCreateFramebuffer(deviceHandle, &createInfo, nullptr, m_product.get());
    if (res != VK_SUCCESS)
        std::cerr << "Failed to create framebuffer : " << res << std::endl;

    return std::move(m_product);
}
