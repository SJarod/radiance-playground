#include <iostream>

#include "device.hpp"
#include "image.hpp"

#include "swapchain.hpp"

SwapChain::~SwapChain()
{
    auto deviceHandle = m_device.lock()->getHandle();

    vkDestroyImageView(deviceHandle, m_depthImageView, nullptr);
    m_depthImage.reset();
    for (VkImageView &imageView : m_imageViews)
    {
        vkDestroyImageView(deviceHandle, imageView, nullptr);
    }
    vkDestroySwapchainKHR(deviceHandle, m_handle, nullptr);
}

const VkFormat SwapChain::getDepthImageFormat() const
{
    return m_depthImage->getFormat();
}

std::unique_ptr<SwapChain> SwapChainBuilder::build()
{
    assert(m_product->m_device.lock());

    auto devicePtr = m_product->m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    VkPhysicalDevice physicalHandle = devicePtr->getPhysicalHandle();
    VkSurfaceKHR surfaceHandle = devicePtr->getSurfaceHandle();

    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalHandle, surfaceHandle, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalHandle, surfaceHandle, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalHandle, surfaceHandle, &formatCount, formats.data());

    uint32_t modeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalHandle, surfaceHandle, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalHandle, surfaceHandle, &modeCount, presentModes.data());

    VkSurfaceFormatKHR surfaceFormat = formats[0];
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && capabilities.maxImageCount < imageCount)
        imageCount = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surfaceHandle,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = m_product->m_extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    uint32_t queueFamilyIndices[] = {devicePtr->getGraphicsFamilyIndex().value(),
                                     devicePtr->getPresentFamilyIndex().value()};

    if (devicePtr->getGraphicsFamilyIndex().value() != devicePtr->getGraphicsFamilyIndex().value())
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    if (vkCreateSwapchainKHR(deviceHandle, &createInfo, nullptr, &m_product->m_handle) != VK_SUCCESS)
        throw std::exception("Failed to create swapchain");

    m_product->m_imageFormat = surfaceFormat.format;

    // swapchain images
    vkGetSwapchainImagesKHR(deviceHandle, m_product->m_handle, &imageCount, nullptr);
    m_product->m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(deviceHandle, m_product->m_handle, &imageCount, m_product->m_images.data());

    m_product->m_swapChainImageCount = m_product->m_images.size();

    // image views

    m_product->m_imageViews.resize(imageCount);

    for (size_t i = 0; i < imageCount; ++i)
    {
        VkImageViewCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_product->m_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = m_product->m_imageFormat,
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        VkResult res = vkCreateImageView(deviceHandle, &createInfo, nullptr, &m_product->m_imageViews[i]);
        if (res != VK_SUCCESS)
            std::cerr << "Failed to create an image view : " << res << std::endl;
    }

    ImageBuilder ib;
    ImageDirector id;
    id.createDepthImage2DBuilder(ib);
    ib.setDevice(m_product->m_device);
    ib.setWidth(m_product->m_extent.width);
    ib.setHeight(m_product->m_extent.height);
    m_product->m_depthImage = ib.build();

    ImageLayoutTransitionBuilder iltb;
    ImageLayoutTransitionDirector iltd;
    iltd.createBuilder<VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL>(iltb);
    iltb.setImage(*m_product->m_depthImage);
    m_product->m_depthImage->transitionImageLayout(*iltb.buildAndRestart());

    m_product->m_depthImageView = m_product->m_depthImage->createImageView2D();

    return std::move(m_product);
}
