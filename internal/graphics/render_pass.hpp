#pragma once

#include <vector>

#include <vulkan/vulkan.h>

class Device;
class SwapChain;
class RenderPassBuilder;

class AttachmentPack
{
  private:
    std::optional<std::vector<VkImageView>> m_colorAttachments;
    std::optional<VkImageView> m_depthAttachment;
};
class Framebuffer
{
  private:
    std::optional<std::vector<Image>> m_images;
    std::vector<AttachmentPack> m_attachments;
};
class SubpassDescription
{
    private:
    std::optional<std::vector<VkAttachmentReference>> m_inputAttachmentReferences;
    std::optional<std::vector<VkAttachmentReference>> m_colorAttachmentReferences;
    std::optional<VkAttachmentReference> m_depthAttachmentReference;
};
class Subpass
{
    private:
    std::vector<SubpassDescription> m_descriptions;
};

class RenderPass
{
    friend RenderPassBuilder;

  private:
    std::weak_ptr<Device> m_device;

    VkRenderPass m_handle;

    std::vector<Framebuffer> m_framebuffers;
    std::vector<Subpass> m_subpasses;

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
    [[nodiscard]] const uint32_t getImageCount() const
    {
        return static_cast<uint32_t>(m_framebuffers.size());
    }
};

class RenderPassBuilder
{
  private:
    std::unique_ptr<RenderPass> m_product;

    std::weak_ptr<Device> m_device;

    void restart()
    {
        m_product = std::unique_ptr<RenderPass>(new RenderPass);
    }

  public:
    RenderPassBuilder()
    {
        restart();
    }

    void setDevice(std::weak_ptr<Device> device)
    {
        m_device = device;
        m_product->m_device = device;
    }

    void describeAttachment(AttachmentPack pack)
    {
        m_product->m_a
    }

    void addAttachmentView(const VkImageView view)
    {
        m_product->m_attachmentViews.push_back(view);
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
