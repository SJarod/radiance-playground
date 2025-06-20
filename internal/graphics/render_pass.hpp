#pragma once

#include <cassert>
#include <memory>
#include <optional>
#include <vector>

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
    uint32_t m_layers;

    std::vector<VkImageView> m_attachments;

    void restart()
    {
        m_extent = {0, 0};
        m_layers = 0u;
        m_product = std::unique_ptr<VkFramebuffer>(new VkFramebuffer);
    }
    std::unique_ptr<VkFramebuffer> build();

  public:
    RenderPassFramebufferBuilder()
    {
        restart();
    }

  public:
    [[nodiscard]] inline bool hasDepthAttachment() const
    {
        return m_depthAttachment.has_value();
    }

  public:
    void setDevice(std::weak_ptr<Device> device)
    {
        m_device = device;
    }

    void setColorAttachment(const VkImageView &colorAttachment)
    {
        m_colorAttachment = colorAttachment;
    }

    void setDepthAttachment(const VkImageView &depthAttachment)
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

    void setExtent(const VkExtent2D &extents)
    {
        m_extent = extents;
    }

    void setLayerCount(const uint32_t count)
    {
        m_layers = count;
    }

    void addAttachment(const VkImageView &attachment)
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

    /**
     * @brief image resources (in which the framebuffers will draw)
     * the render pass object is not required to possess a reference to the image resources
     * it can be though
     *
     */
    std::optional<std::vector<VkImage>> m_imageResources;

    VkRenderPass m_handle;
    size_t m_poolSize;
    std::vector<std::vector<VkFramebuffer>> m_pooledFramebuffers;
    std::vector<std::vector<VkImageView>> m_pooledViews;
    std::vector<RenderPassFramebufferBuilder> m_pooledFramebufferBuilders;

    VkRect2D m_minRenderArea;

    bool m_bHasDepthAttachment = false;
    uint32_t m_layerCount;

    uint32_t m_colorAttachmentCount = 0;

    RenderPass() = default;

  public:
    ~RenderPass();

    RenderPass(const RenderPass &) = delete;
    RenderPass &operator=(const RenderPass &) = delete;
    RenderPass(RenderPass &&) = delete;
    RenderPass &operator=(RenderPass &&) = delete;

    void buildFramebuffers(const std::vector<std::vector<VkImageView>> &pooledImageViews,
                           const std::optional<std::vector<VkImageView>> &pooledDepthAttachments, VkExtent2D extent,
                           uint32_t layerCount, bool clearOldFramebuffers = false);

  public:
    [[nodiscard]] const VkRenderPass &getHandle() const
    {
        return m_handle;
    }

    [[nodiscard]] uint32_t getColorAttachmentCount() const
    {
        assert(m_colorAttachmentCount > 0);
        return m_colorAttachmentCount;
    }

    [[nodiscard]] VkFramebuffer getFramebuffer(uint32_t poolIndex, uint32_t imageIndex) const
    {
        return m_pooledFramebuffers[poolIndex][imageIndex];
    }

    [[nodiscard]] inline std::optional<VkImage> getImageResource(uint32_t imageIndex) const
    {
        if (m_imageResources.has_value())
            return std::optional<VkImage>(m_imageResources.value()[imageIndex]);
        else
            return nullptr;
    }
    [[nodiscard]] VkImageView getImageView(uint32_t poolIndex, uint32_t imageIndex) const
    {
        return m_pooledViews[poolIndex][imageIndex];
    }
    [[nodiscard]] const uint32_t getImageCount(uint32_t poolIndex) const
    {
        return static_cast<uint32_t>(m_pooledFramebuffers[poolIndex].size());
    }
    [[nodiscard]] const uint32_t getFramebufferPoolSize() const
    {
        return static_cast<uint32_t>(m_poolSize);
    }

    [[nodiscard]] const VkRect2D getMinRenderArea() const
    {
        return m_minRenderArea;
    }

    [[nodiscard]] inline uint32_t getLayerCount() const
    {
        return m_layerCount;
    }

    [[nodiscard]] inline bool getHasDepthAttachment() const
    {
        return m_bHasDepthAttachment || m_pooledFramebufferBuilders[0].hasDepthAttachment();
    }
};

class RenderPassBuilder
{
  private:
    std::unique_ptr<RenderPass> m_product;

    std::vector<VkAttachmentDescription> m_attachments;

    std::vector<VkAttachmentReference> m_colorAttachmentReferences;
    std::optional<VkAttachmentReference> m_depthAttachmentReference;

    size_t m_poolSize = 0u;
    std::vector<std::vector<VkImageView>> m_pooledImageViews;
    std::optional<std::vector<VkImageView>> m_pooledDepthAttachments;
    VkExtent2D m_extent;
    uint32_t m_layers;
    bool m_multiviewEnable = false;

    // TODO : multiple subpasses
    VkSubpassDescription m_subpass = {};
    std::vector<VkSubpassDependency> m_subpassDependency = {};

    std::weak_ptr<Device> m_device;

    void restart()
    {
        m_product = std::unique_ptr<RenderPass>(new RenderPass);

        m_subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        m_subpassDependency.clear();
        m_subpassDependency.push_back({});
        m_subpassDependency[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        m_subpassDependency[0].dstSubpass = 0;
        m_subpassDependency[0].srcAccessMask = 0;
    }

  public:
    RenderPassBuilder()
    {
        restart();
    }

    void addFragmentShaderSubpassDependencyToItself()
    {
        m_subpassDependency.push_back(VkSubpassDependency{
            .srcSubpass = 0,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        });
    }
    void addColorAttachment(VkAttachmentDescription attachment);
    void addDepthAttachment(VkAttachmentDescription attachment);

    void setDevice(std::weak_ptr<Device> device)
    {
        m_device = device;
        m_product->m_device = device;
    }
    /**
     * @brief Set the Image Resources object
     * the images are the actual resource in which the render pass will draw using the framebuffers
     * it is not used with the pooling system
     *
     * @param images
     */
    void setImageResources(const std::vector<VkImage> &images)
    {
        m_poolSize = 1u;
        if (m_product->m_imageResources.has_value())
            m_product->m_imageResources.value().clear();
        m_product->m_imageResources = images;
    }
    /**
     * @brief Set the Image Views object
     * the image views are views towards the image resources
     * used to create the framebuffers
     *
     * @param imageViews
     */
    void setImageViews(const std::vector<VkImageView> &imageViews)
    {
        m_poolSize = 1u;
        m_pooledImageViews.clear();
        m_pooledImageViews.push_back(imageViews);
    }
    void addPooledImageViews(const std::vector<VkImageView> &imageViews)
    {
        m_pooledImageViews.push_back(imageViews);
        m_poolSize++;
    }
    void setDepthAttachment(const VkImageView &depthAttachment)
    {
        m_poolSize = 1u;

        if (!m_pooledDepthAttachments.has_value())
        {
            m_pooledDepthAttachments = {depthAttachment};
            return;
        }

        m_pooledDepthAttachments.value().clear();
        m_pooledDepthAttachments.value().push_back(depthAttachment);
    }
    void addPooledDepthAttachment(const VkImageView &depthAttachment)
    {
        if (!m_pooledDepthAttachments.has_value())
        {
            m_pooledDepthAttachments = {depthAttachment};
            return;
        }

        m_pooledDepthAttachments.value().push_back(depthAttachment);
    }
    void setExtent(const VkExtent2D &extent)
    {
        m_extent = extent;
    }
    void setLayerCount(const uint32_t &count)
    {
        m_layers = count;
        m_product->m_layerCount = count;
    }
    void setMultiviewUsageEnable(const bool enable)
    {
        m_multiviewEnable = enable;
    }

    std::unique_ptr<RenderPass> build();
};

class RenderPassDirector
{
  public:
    void configureSwapChainRenderPassBuilder(RenderPassBuilder &builder, const SwapChain &swapchain,
                                             bool hasDepthAttachment = true);
    void configureCubemapRenderPassBuilder(RenderPassBuilder &builder, const Texture &cubemap, bool useMultiview,
                                           bool hasDepthAttachment = true);
    void configurePooledCubemapsRenderPassBuilder(RenderPassBuilder &builder,
                                                  const std::vector<std::shared_ptr<Texture>> &cubemaps,
                                                  bool useMultiview, bool hasDepthAttachment = true);
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