#include <iostream>

#include "device.hpp"
#include "image.hpp"

#include "swapchain.hpp"

SwapChain::SwapChain(std::weak_ptr<Device> device) : m_device(device)
{
    auto devicePtr = m_device.lock();
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
    VkPresentModeKHR presentMode = presentModes[0];
    VkExtent2D extent = {1366, 768};

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && capabilities.maxImageCount < imageCount)
        imageCount = capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surfaceHandle,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
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

    if (vkCreateSwapchainKHR(deviceHandle, &createInfo, nullptr, &m_handle) != VK_SUCCESS)
        throw std::exception("Failed to create swapchain");

    m_imageFormat = surfaceFormat.format;
    m_extent = extent;

    // swapchain images
    vkGetSwapchainImagesKHR(deviceHandle, m_handle, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(deviceHandle, m_handle, &imageCount, m_images.data());

    m_frameInFlightCount = m_images.size();

    // image views

    m_imageViews.resize(imageCount);

    for (size_t i = 0; i < imageCount; ++i)
    {
        VkImageViewCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = m_imageFormat,
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

        VkResult res = vkCreateImageView(deviceHandle, &createInfo, nullptr, &m_imageViews[i]);
        if (res != VK_SUCCESS)
            std::cerr << "Failed to create an image view : " << res << std::endl;
    }

    m_depthFormat = VK_FORMAT_D32_SFLOAT_S8_UINT;
    m_depthImage =
        std::make_unique<Image>(device, m_depthFormat, extent.width, extent.height, VK_IMAGE_TILING_OPTIMAL,
                                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

    ImageLayoutTransitionBuilder iltb;
    ImageLayoutTransitionDirector iltd;
    iltd.createBuilder<VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL>(iltb);
    iltb.setImage(*m_depthImage);
    m_depthImage->transitionImageLayout(*iltb.build());

    m_depthImageView = m_depthImage->createImageView();
}

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