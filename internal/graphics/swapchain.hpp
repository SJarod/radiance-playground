#pragma once

#include <memory>
#include <optional>
#include <vector>

#include <vulkan/vulkan.h>

#include "image.hpp"

class Device;

class SwapChainBuilder;

class SwapChain
{
    friend SwapChainBuilder;

  private:
    std::weak_ptr<Device> m_device;

    VkSwapchainKHR m_handle;

    VkFormat m_imageFormat;
    VkExtent2D m_extent;

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;

    std::optional<std::unique_ptr<VkSampler>> m_sampler;

    std::unique_ptr<Image> m_depthImage;
    VkImageView m_depthImageView;

    uint32_t m_swapChainImageCount;

    SwapChain() = default;

  public:
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

    [[nodiscard]] inline const uint32_t getSwapChainImageCount() const
    {
        return m_swapChainImageCount;
    }

    [[nodiscard]] inline std::weak_ptr<Device> getDevice() const
    {
        return m_device;
    }
};

class SwapChainBuilder
{
  private:
    std::unique_ptr<SwapChain> m_product;

    VkSurfaceFormatKHR m_swapchainSurfaceFormat;
    VkPresentModeKHR m_swapchainPresentMode;

    bool m_useImagesAsSamplers = false;

    void restart()
    {
        m_product = std::unique_ptr<SwapChain>(new SwapChain);
    }

  public:
    SwapChainBuilder()
    {
        restart();
    }

    void setDevice(std::weak_ptr<Device> device)
    {
        m_product->m_device = device;
    }
    void setWidth(uint32_t width)
    {
        m_product->m_extent.width = width;
    }
    void setHeight(uint32_t height)
    {
        m_product->m_extent.height = height;
    }
    void setSwapchainImageFormat(VkSurfaceFormatKHR swapchainSurfaceFormat)
    {
        m_swapchainSurfaceFormat = swapchainSurfaceFormat;
    };
    void setSwapchainPresentMode(VkPresentModeKHR swapchainPresentMode)
    {
        m_swapchainPresentMode = swapchainPresentMode;
    };
    void setUseImagesAsSamplers(bool a)
    {
        m_useImagesAsSamplers = a;
    }

    std::unique_ptr<SwapChain> build();
};
