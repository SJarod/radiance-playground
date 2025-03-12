#pragma once

#include <vector>

#include <vulkan/vulkan.h>

class Device;
class SwapChain;
class RenderPassBuilder;

class RenderPass
{
    friend RenderPassBuilder;

  private:
    std::weak_ptr<Device> m_device;

    VkRenderPass m_handle;
    std::vector<VkFramebuffer> m_framebuffers;
    std::vector<const VkImageView *> m_views;

    RenderPass() = default;

  public:
    ~RenderPass();

    RenderPass(const RenderPass &) = delete;
    RenderPass &operator=(const RenderPass &) = delete;
    RenderPass(RenderPass &&) = delete;
    RenderPass &operator=(RenderPass &&) = delete;

  public:
    [[nodiscard]] const VkRenderPass &getHandle() const
    {
        return m_handle;
    }

    [[nodiscard]] const VkFramebuffer &getFramebuffer(uint32_t imageIndex) const
    {
        return m_framebuffers[imageIndex];
    }

    [[nodiscard]] const VkImageView *getImageView(uint32_t imageIndex) const
    {
        return m_views[imageIndex];
    }
    [[nodiscard]] const uint32_t getImageCount() const
    {
        return static_cast<uint32_t>(m_framebuffers.size());
    }
};

class RenderPassBuilder
{
  private:
    std::unique_ptr<RenderPass> m_product;

    std::vector<VkAttachmentDescription> m_attachments;

    std::vector<VkAttachmentReference> m_colorAttachmentReferences;
    std::vector<VkAttachmentReference> m_depthAttachmentReferences;

    VkSubpassDescription m_subpass = {};
    VkSubpassDependency m_subpassDependency = {};

    std::weak_ptr<Device> m_device;
    const SwapChain *m_swapchain;

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
    }
    void setSwapChain(const SwapChain *swapchain)
    {
        m_swapchain = swapchain;
    }

    std::unique_ptr<RenderPass> build();
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
