#include <iostream>
#include <string>

#include "device.hpp"
#include "image.hpp"

#include "swapchain.hpp"

SwapChain::~SwapChain()
{
    auto deviceHandle = m_device.lock()->getHandle();

    if (m_sampler.has_value())
    {
        vkDestroySampler(deviceHandle, **m_sampler, nullptr);
    }
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
    std::vector<VkSurfaceFormatKHR> supportedFormats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalHandle, surfaceHandle, &formatCount, supportedFormats.data());
    VkSurfaceFormatKHR surfaceFormat = supportedFormats[0];
    if (std::find_if(supportedFormats.begin(), supportedFormats.end(), [this](VkSurfaceFormatKHR s) {
            return s.format == m_swapchainSurfaceFormat.format && s.colorSpace == m_swapchainSurfaceFormat.colorSpace;
        }) != supportedFormats.end())
        surfaceFormat = m_swapchainSurfaceFormat;

    uint32_t modeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalHandle, surfaceHandle, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> supportedPresentModes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalHandle, surfaceHandle, &modeCount, supportedPresentModes.data());
    VkPresentModeKHR presentMode = supportedPresentModes[0];
    if (std::find(supportedPresentModes.begin(), supportedPresentModes.end(), m_swapchainPresentMode) !=
        supportedPresentModes.end())
        presentMode = m_swapchainPresentMode;

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && capabilities.maxImageCount < imageCount)
        imageCount = capabilities.maxImageCount;

    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (m_useImagesAsSamplers)
        usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surfaceHandle,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = m_product->m_extent,
        .imageArrayLayers = 1,
        .imageUsage = usage,
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

        devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VK_OBJECT_TYPE_IMAGE,
            .objectHandle = (uint64_t)m_product->m_images[i],
            .pObjectName = std::string("Swapchain Image Resource " + std::to_string(i)).c_str(),
        });

        devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
            .objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
            .objectHandle = (uint64_t)m_product->m_imageViews[i],
            .pObjectName = std::string("Swapchain Image View " + std::to_string(i)).c_str(),
        });
    }

    ImageBuilder ib;
    ImageDirector id;
    id.configureDepthImage2DBuilder(ib);
    ib.setDevice(m_product->m_device);
    ib.setWidth(m_product->m_extent.width);
    ib.setHeight(m_product->m_extent.height);
    m_product->m_depthImage = ib.build();

    ImageLayoutTransitionBuilder iltb;
    ImageLayoutTransitionDirector iltd;
    iltd.configureBuilder<VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL>(iltb);
    iltb.setImage(*m_product->m_depthImage);
    m_product->m_depthImage->transitionImageLayout(*iltb.buildAndRestart());

    m_product->m_depthImageView = m_product->m_depthImage->createImageView2D();

    if (m_useImagesAsSamplers)
    {
        SamplerBuilder sb;
        sb.setDevice(m_product->m_device);
        sb.setMagFilter(VK_FILTER_LINEAR);
        sb.setMinFilter(VK_FILTER_LINEAR);
        m_product->m_sampler = sb.build();
    }

    return std::move(m_product);
}
