#include <cassert>
#include <iostream>

#include "buffer.hpp"
#include "device.hpp"

#include "image.hpp"

Image::~Image()
{
    if (!m_device.lock())
        return;

    auto deviceHandle = m_device.lock()->getHandle();
    vkFreeMemory(deviceHandle, m_memory, nullptr);
    vkDestroyImage(deviceHandle, m_handle, nullptr);
}

void Image::transitionImageLayout(ImageLayoutTransition transition)
{
    auto devicePtr = m_device.lock();
    VkCommandBuffer commandBuffer = devicePtr->cmdBeginOneTimeSubmit();

    vkCmdPipelineBarrier(commandBuffer, transition.srcStageMask, transition.dstStageMask, 0, 0, nullptr, 0, nullptr, 1,
                         &transition.barrier);

    devicePtr->cmdEndOneTimeSubmit(commandBuffer);
}

void Image::copyBufferToImage(VkBuffer buffer)
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

VkImageView Image::createImageView()
{
    auto deviceHandle = m_device.lock()->getHandle();

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
        std::cerr << "Failed to create image view : " << res << std::endl;

    return imageView;
}

std::unique_ptr<Image> ImageBuilder::build()
{
    assert(m_device.lock());

    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    VkImageCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .flags = 0,
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

    VkResult res = vkCreateImage(deviceHandle, &createInfo, nullptr, &m_product->m_handle);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create image : " << res << std::endl;
        return nullptr;
    }

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(deviceHandle, m_product->m_handle, &memReq);
    std::optional<uint32_t> memoryTypeIndex = devicePtr->findMemoryTypeIndex(memReq, m_properties);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReq.size,
        .memoryTypeIndex = memoryTypeIndex.value(),
    };

    res = vkAllocateMemory(deviceHandle, &allocInfo, nullptr, &m_product->m_memory);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to allocate memory : " << res << std::endl;
        return nullptr;
    }

    vkBindImageMemory(deviceHandle, m_product->m_handle, m_product->m_memory, 0);

    return std::move(m_product);
}

void ImageDirector::createImage2DBuilder(ImageBuilder &builder)
{
    builder.setImageType(VK_IMAGE_TYPE_2D);
    builder.setDepth(1U);
    builder.setMipLevels(1U);
    builder.setArrayLayers(1U);
    builder.setSamples(VK_SAMPLE_COUNT_1_BIT);
    builder.setSharingMode(VK_SHARING_MODE_EXCLUSIVE);
    builder.setInitialLayout(VK_IMAGE_LAYOUT_UNDEFINED);
}

void ImageDirector::createDepthImage2DBuilder(ImageBuilder &builder)
{
    createImage2DBuilder(builder);
    builder.setFormat(VK_FORMAT_D32_SFLOAT_S8_UINT);
    builder.setTiling(VK_IMAGE_TILING_OPTIMAL);
    builder.setUsage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    builder.setProperties(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    builder.setAspectFlags(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
}

void ImageDirector::createSampledImage2DBuilder(ImageBuilder &builder)
{
    createImage2DBuilder(builder);
    builder.setUsage(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
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

std::unique_ptr<ImageLayoutTransition> ImageLayoutTransitionBuilder::build()
{
    // image must be set using setImage()
    assert(m_product->barrier.image);

    auto result = std::move(m_product);
    restart();
    return result;
}
