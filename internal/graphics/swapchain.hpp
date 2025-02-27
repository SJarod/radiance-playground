#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

class Device;
class Image;

class SwapChain
{
  private:
    std::weak_ptr<Device> m_device;

    VkSwapchainKHR m_handle;

    VkFormat m_imageFormat;
    VkExtent2D m_extent;

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;

    std::unique_ptr<Image> m_depthImage;
    VkImageView m_depthImageView;

    // TODO : rename (swapchain image count)
    uint32_t m_frameInFlightCount;

  public:
    SwapChain(std::weak_ptr<Device> device);
    ~SwapChain();

    SwapChain(const SwapChain &) = delete;
    SwapChain &operator=(const SwapChain &) = delete;
    SwapChain(SwapChain &&) = delete;
    SwapChain &operator=(SwapChain &&) = delete;

  public:
    [[nodiscard]] inline const VkSwapchainKHR &getHandle() const
    {
        return m_handle;
    }

    [[nodiscard]] inline const std::vector<VkImageView> &getImageViews() const
    {
        return m_imageViews;
    }
    [[nodiscard]] inline const VkFormat &getImageFormat() const
    {
        return m_imageFormat;
    }

    [[nodiscard]] inline const VkImageView &getDepthImageView() const
    {
        return m_depthImageView;
    }
    [[nodiscard]] const VkFormat getDepthImageFormat() const;

    [[nodiscard]] inline const VkExtent2D &getExtent() const
    {
        return m_extent;
    }

    [[nodiscard]] inline const uint32_t getFrameInFlightCount() const
    {
        return m_frameInFlightCount;
    }
};