#pragma once

#include <vector>
#include <optional>

#include <vulkan/vulkan.h>

class Device;
class SwapChain;
class RenderPassBuilder;
class RenderPass;
class Texture;

class RenderPassFramebufferBuilder
{
private:
    std::unique_ptr<VkFramebuffer> m_product;

    VkImageView m_colorAttachment;
    std::optional<VkImageView> m_depthAttachment;
    std::weak_ptr<Device> m_device;
    VkRenderPass m_renderPassHandle;
    bool m_hasDepthAttachment = false;
    VkExtent2D m_extent;
    std::vector<VkImageView> m_attachments;

    void restart()
    {
        m_extent = { 0, 0 };
        m_product = std::unique_ptr<VkFramebuffer>(new VkFramebuffer);
    }
    std::unique_ptr<VkFramebuffer> build();

public:
    RenderPassFramebufferBuilder()
    {
        restart();
    }

    void setDevice(std::weak_ptr<Device> device)
    {
        m_device = device;
    }

    void setColorAttachment(const VkImageView& colorAttachment)
    {
        m_colorAttachment = colorAttachment;
    }

    void setDepthAttachment(const VkImageView& depthAttachment)
    {
        m_depthAttachment = depthAttachment;
    }

    void setRenderPass(VkRenderPass renderPass)
    {
        m_renderPassHandle = renderPass;
    }

    void setHasDepthAttached(bool value)
    {
        m_hasDepthAttachment = value;
    }

    void setExtent(const VkExtent2D& extents)
    {
        m_extent = extents;
    }

    void addAttachment(const VkImageView& attachment)
    {
        m_attachments.push_back(attachment);
    }

    std::unique_ptr<VkFramebuffer> buildAndRestart()
    {
        auto result = build();
        restart();
        return result;
    }
};


class RenderPass
{
    friend RenderPassBuilder;

  private:
    std::weak_ptr<Device> m_device;

    VkRenderPass m_handle;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<VkImageView> m_views;
    RenderPassFramebufferBuilder m_framebufferBuilder;

    VkRect2D m_minRenderArea;

    RenderPass() = default;

  public:
    ~RenderPass();

    RenderPass(const RenderPass &) = delete;
    RenderPass &operator=(const RenderPass &) = delete;
    RenderPass(RenderPass &&) = delete;
    RenderPass &operator=(RenderPass &&) = delete;
    
    void buildFramebuffers(const std::vector<VkImageView>& imageViews, const std::optional<VkImageView>& depthAttachment, VkExtent2D extent, bool clearOldFramebuffers = false);
  public:
    [[nodiscard]] const VkRenderPass &getHandle() const
    {
        return m_handle;
    }

    [[nodiscard]] const VkFramebuffer &getFramebuffer(uint32_t imageIndex) const
    {
        return m_framebuffers[imageIndex];
    }

    [[nodiscard]] const VkImageView &getImageView(uint32_t imageIndex) const
    {
        return m_views[imageIndex];
    }
    [[nodiscard]] const uint32_t getImageCount() const
    {
        return static_cast<uint32_t>(m_framebuffers.size());
    }

    [[nodiscard]] const VkRect2D getMinRenderArea() const
    {
        return m_minRenderArea;
    }
};

class RenderPassBuilder
{
  private:
    std::unique_ptr<RenderPass> m_product;

    std::vector<VkAttachmentDescription> m_attachments;

    std::vector<VkAttachmentReference> m_colorAttachmentReferences;
    std::optional<VkAttachmentReference> m_depthAttachmentReference;

    std::vector<VkImageView> m_imageViews;
    std::optional<VkImageView> m_depthAttachment;
    VkExtent2D m_extent;

    VkSubpassDescription m_subpass = {};
    VkSubpassDependency m_subpassDependency = {};

    std::weak_ptr<Device> m_device;

    void restart()
    {
        m_product = std::unique_ptr<RenderPass>(new RenderPass);

        m_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        m_subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        m_subpassDependency.dstSubpass = 0;
        m_subpassDependency.srcAccessMask = 0;
    }

  public:
    RenderPassBuilder()
    {
        restart();
    }

    void addColorAttachment(VkAttachmentDescription attachment);
    void addDepthAttachment(VkAttachmentDescription attachment);

    void setDevice(std::weak_ptr<Device> device)
    {
        m_device = device;
        m_product->m_device = device;
        m_product->m_framebufferBuilder.setDevice(device);
    }
    void setImageViews(const std::vector<VkImageView>& imageViews)
    {
        m_imageViews = imageViews;
    }
    void setDepthAttachment(const VkImageView& depthAttachment)
    {
        m_depthAttachment = depthAttachment;
    }
    void setExtent(const VkExtent2D& extent)
    {
        m_extent = extent;
    }

    std::unique_ptr<RenderPass> build();
};

class RenderPassDirector
{
  public:
      void configureSwapChainRenderPassBuilder(RenderPassBuilder &builder, const SwapChain &swapchain, bool hasDepthAttachment = true);
      void configureCubemapRenderPassBuilder(RenderPassBuilder &builder, const Texture &cubemap);
};

class RenderPassAttachmentBuilder
{
  private:
    std::unique_ptr<VkAttachmentDescription> m_product;

    void restart()
    {
        m_product = std::unique_ptr<VkAttachmentDescription>(new VkAttachmentDescription);
        m_product->flags = 0;
    }

  public:
    RenderPassAttachmentBuilder()
    {
        restart();
    }

    void setFlags(VkAttachmentDescriptionFlags flags)
    {
        m_product->flags = flags;
    }
    void setFormat(VkFormat format)
    {
        m_product->format = format;
    }
    void setSamples(VkSampleCountFlagBits samples)
    {
        m_product->samples = samples;
    }
    void setLoadOp(VkAttachmentLoadOp loadOp)
    {
        m_product->loadOp = loadOp;
    }
    void setStoreOp(VkAttachmentStoreOp storeOp)
    {
        m_product->storeOp = storeOp;
    }
    void setStencilLoadOp(VkAttachmentLoadOp stencilLoadOp)
    {
        m_product->stencilLoadOp = stencilLoadOp;
    }
    void setStencilStoreOp(VkAttachmentStoreOp stencilStoreOp)
    {
        m_product->stencilStoreOp = stencilStoreOp;
    }
    void setInitialLayout(VkImageLayout initialLayout)
    {
        m_product->initialLayout = initialLayout;
    }
    void setFinalLayout(VkImageLayout finalLayout)
    {
        m_product->finalLayout = finalLayout;
    }

    std::unique_ptr<VkAttachmentDescription> buildAndRestart()
    {
        auto result = std::move(m_product);
        restart();
        return result;
    }
};

class RenderPassAttachmentDirector
{
  public:
    void configureAttachmentDontCareBuilder(RenderPassAttachmentBuilder &builder);
    void configureAttachmentClearBuilder(RenderPassAttachmentBuilder &builder);
    void configureAttachmentLoadBuilder(RenderPassAttachmentBuilder &builder);
};