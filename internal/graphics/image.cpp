#include <cassert>
#include <iostream>

#include "buffer.hpp"
#include "device.hpp"

#include "image.hpp"

Image::~Image()
{
    if (m_device.expired())
        return;

    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    vmaDestroyImage(devicePtr->getAllocator(), m_handle, m_allocation);
    devicePtr->addImageCount(-1);
    devicePtr->untrackImageName(m_name);
}

void Image::transitionImageLayout(ImageLayoutTransition transition)
{
    auto devicePtr = m_device.lock();
    VkCommandBuffer commandBuffer = devicePtr->cmdBeginOneTimeSubmit();

    vkCmdPipelineBarrier(commandBuffer, transition.srcStageMask, transition.dstStageMask, 0, 0, nullptr, 0, nullptr, 1,
                         &transition.barrier);

    devicePtr->cmdEndOneTimeSubmit(commandBuffer);
}

void Image::copyBufferToImage2D(VkBuffer buffer)
{
    auto devicePtr = m_device.lock();
    VkCommandBuffer commandBuffer = devicePtr->cmdBeginOneTimeSubmit();

    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            {
                .aspectMask = m_aspectFlags,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .imageOffset =
            {
                .x = 0,
                .y = 0,
                .z = 0,
            },
        .imageExtent =
            {
                .width = m_width,
                .height = m_height,
                .depth = 1,
            },
    };

    vkCmdCopyBufferToImage(commandBuffer, buffer, m_handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    devicePtr->cmdEndOneTimeSubmit(commandBuffer);
}

void Image::copyBufferToImageCube(VkBuffer buffer)
{
    auto devicePtr = m_device.lock();
    VkCommandBuffer commandBuffer = devicePtr->cmdBeginOneTimeSubmit();

    VkBufferImageCopy region = {
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            {
                .aspectMask = m_aspectFlags,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 6,
            },
        .imageOffset =
            {
                .x = 0,
                .y = 0,
                .z = 0,
            },
        .imageExtent =
            {
                .width = m_width,
                .height = m_height,
                .depth = 1,
            },
    };

    vkCmdCopyBufferToImage(commandBuffer, buffer, m_handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    devicePtr->cmdEndOneTimeSubmit(commandBuffer);
}

VkImageView Image::createImageView2D()
{
    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    VkImageViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = m_format,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_G,
                .b = VK_COMPONENT_SWIZZLE_B,
                .a = VK_COMPONENT_SWIZZLE_A,
            },
        .subresourceRange =
            {
                .aspectMask = m_aspectFlags,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkImageView imageView;
    VkResult res = vkCreateImageView(deviceHandle, &createInfo, nullptr, &imageView);
    if (res != VK_SUCCESS)
        std::cerr << "Failed to create 2D image view : " << res << std::endl;
    devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
        .objectHandle = (uint64_t)imageView,
        .pObjectName = std::string(m_name + " Image View").c_str(),
    });

    return imageView;
}

VkImageView Image::createImageViewCube()
{
    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    VkImageViewCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_handle,
        .viewType = VK_IMAGE_VIEW_TYPE_CUBE,
        .format = m_format,
        .components =
            {
                .r = VK_COMPONENT_SWIZZLE_R,
                .g = VK_COMPONENT_SWIZZLE_G,
                .b = VK_COMPONENT_SWIZZLE_B,
                .a = VK_COMPONENT_SWIZZLE_A,
            },
        .subresourceRange =
            {
                .aspectMask = m_aspectFlags,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 6,
            },
    };

    VkImageView imageView;
    VkResult res = vkCreateImageView(deviceHandle, &createInfo, nullptr, &imageView);
    if (res != VK_SUCCESS)
        std::cerr << "Failed to create cube image view : " << res << std::endl;
    devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
        .objectHandle = (uint64_t)imageView,
        .pObjectName = std::string(m_name + " Cubemap Image View").c_str(),
    });

    return imageView;
}

std::unique_ptr<Image> ImageBuilder::build()
{
    assert(m_device.lock());

    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    VkImageCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = m_flags,
        .imageType = m_imageType,
        .format = m_product->m_format,
        .extent =
            {
                .width = m_product->m_width,
                .height = m_product->m_height,
                .depth = m_product->m_depth,
            },
        .mipLevels = m_mipLevels,
        .arrayLayers = m_arrayLayers,
        .samples = m_samples,
        .tiling = m_tiling,
        .usage = m_usage,
        .sharingMode = m_sharingMode,
        .initialLayout = m_initialLayout,
    };

    VmaAllocationCreateInfo allocInfo = {
        .requiredFlags = m_properties,
    };

    VkResult res = vmaCreateImage(devicePtr->getAllocator(), &createInfo, &allocInfo, &m_product->m_handle,
                                  &m_product->m_allocation, nullptr);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create image : " << res << std::endl;
        return nullptr;
    }

    m_product->m_name += std::string(" Image " + std::to_string(devicePtr->getImageCount()));
    devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_IMAGE,
        .objectHandle = (uint64_t)m_product->m_handle,
        .pObjectName = m_product->m_name.c_str(),
    });
    devicePtr->addImageCount(1);
    devicePtr->trackImageName(m_product->m_name);

    VmaAllocationInfo info;
    vmaGetAllocationInfo(devicePtr->getAllocator(), m_product->m_allocation, &info);

    static int imageMemoryCount = 0;
    devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_DEVICE_MEMORY,
        .objectHandle = (uint64_t)info.deviceMemory,
        .pObjectName = std::string("Image Memory " + std::to_string(imageMemoryCount++)).c_str(),
    });

    return std::move(m_product);
}

void ImageDirector::configureImage2DBuilder(ImageBuilder &builder)
{
    builder.setFlags(0);
    builder.setImageType(VK_IMAGE_TYPE_2D);
    builder.setDepth(1U);
    builder.setMipLevels(1U);
    builder.setArrayLayers(1U);
    builder.setSamples(VK_SAMPLE_COUNT_1_BIT);
    builder.setSharingMode(VK_SHARING_MODE_EXCLUSIVE);
    builder.setInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
}

void ImageDirector::configureImageCubeBuilder(ImageBuilder &builder)
{
    builder.setFlags(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
    builder.setImageType(VK_IMAGE_TYPE_2D);
    builder.setDepth(1U);
    builder.setMipLevels(1U);
    builder.setArrayLayers(6U);
    builder.setSamples(VK_SAMPLE_COUNT_1_BIT);
    builder.setSharingMode(VK_SHARING_MODE_EXCLUSIVE);
    builder.setInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
}

void ImageDirector::configureDepthImage2DBuilder(ImageBuilder &builder)
{
    configureImage2DBuilder(builder);
    builder.setFormat(VK_FORMAT_D32_SFLOAT_S8_UINT);
    builder.setTiling(VK_IMAGE_TILING_OPTIMAL);
    builder.setUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    builder.setAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
}

void ImageDirector::configureSampledImage2DBuilder(ImageBuilder &builder)
{
    configureImage2DBuilder(builder);
    builder.setUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    builder.setAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT);
}

void ImageDirector::configureSampledImageCubeBuilder(ImageBuilder &builder)
{
    configureImageCubeBuilder(builder);
    builder.setUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    builder.setAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT);
}

void ImageDirector::configureNonSampledImageCubeBuilder(ImageBuilder &builder)
{
    configureImageCubeBuilder(builder);
    builder.setUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    builder.setAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT);
}

void ImageDirector::configureDepthImageCubeBuilder(ImageBuilder &builder)
{
    configureImageCubeBuilder(builder);
    builder.setFormat(VK_FORMAT_D32_SFLOAT_S8_UINT);
    builder.setTiling(VK_IMAGE_TILING_OPTIMAL);
    builder.setUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    builder.setAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
}

void ImageDirector::configureSampledResolveImageCubeBuilder(ImageBuilder &builder)
{
    configureImageCubeBuilder(builder);
    builder.setUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    builder.setAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT);
}

void ImageLayoutTransitionBuilder::restart()
{
    m_product = std::unique_ptr<ImageLayoutTransition>(new ImageLayoutTransition);
    m_product->barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    m_product->barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    m_product->barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    m_product->barrier.subresourceRange.baseMipLevel = 0;
    m_product->barrier.subresourceRange.levelCount = 1;
    m_product->barrier.subresourceRange.baseArrayLayer = 0;
    m_product->barrier.subresourceRange.layerCount = 1;
}

std::unique_ptr<ImageLayoutTransition> ImageLayoutTransitionBuilder::buildAndRestart()
{
    // image must be set using setImage()
    assert(m_product->barrier.image);

    auto result = std::move(m_product);
    restart();
    return result;
}

std::unique_ptr<VkSampler> SamplerBuilder::build()
{
    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    VkSamplerCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = m_magFilter,
        .minFilter = m_minFilter,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = m_addressmodeX,
        .addressModeV = m_addressmodeY,
        .addressModeW = m_addressmodeZ,
        .mipLodBias = 0.f,
        .anisotropyEnable = devicePtr->getPhysicalDeviceFeatures2().features.samplerAnisotropy,
        .maxAnisotropy = devicePtr->getPhysicalDeviceProperties().limits.maxSamplerAnisotropy,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .minLod = 0.f,
        .maxLod = 0.f,
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    VkResult res = vkCreateSampler(deviceHandle, &createInfo, nullptr, m_product.get());
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create image sampler : " << res << std::endl;
        return nullptr;
    }
    static int samplerCount = 0;
    devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_SAMPLER,
        .objectHandle = (uint64_t)(*m_product),
        .pObjectName = std::string("Image Sampler " + std::to_string(samplerCount++)).c_str(),
    });

    auto result = std::move(m_product);
    return result;
}
