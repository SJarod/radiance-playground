#include <iostream>

#include "graphics/buffer.hpp"
#include "graphics/device.hpp"
#include "graphics/image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "texture.hpp"

Texture::~Texture()
{
    m_image.reset();
    if (!m_device.lock())
        return;

    auto deviceHandle = m_device.lock()->getHandle();
    vkDestroySampler(deviceHandle, m_sampler, nullptr);
    vkDestroyImageView(deviceHandle, m_imageView, nullptr);
}

std::unique_ptr<Texture> TextureBuilder::buildAndRestart()
{
    assert(m_device.lock());

    size_t imageSize = m_product->m_width * m_product->m_height * 4;

    if (m_bLoadFromFile)
    {
        int texWidth, texHeight, texChannels;
        stbi_uc *textureData =
            stbi_load(m_textureFilename.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        if (!textureData)
        {
            std::cerr << "Failed to load texture : " << m_textureFilename << std::endl;
            return nullptr;
        }
        m_product->m_width = texWidth;
        m_product->m_height = texHeight;
        imageSize = m_product->m_width * m_product->m_height * 4;

        m_product->m_imageData.resize(imageSize);
        memcpy(m_product->m_imageData.data(), textureData, imageSize);

        stbi_image_free(textureData);
    }

    BufferBuilder bb;
    BufferDirector bd;
    bd.createStagingBufferBuilder(bb);
    bb.setDevice(m_device);
    bb.setSize(imageSize);
    std::unique_ptr<Buffer> stagingBuffer = bb.build();

    stagingBuffer->copyDataToMemory(m_product->m_imageData.data());

    ImageBuilder ib;
    ImageDirector id;
    id.createSampledImage2DBuilder(ib);
    ib.setDevice(m_device);
    ib.setFormat(m_format);
    ib.setWidth(m_product->m_width);
    ib.setHeight(m_product->m_height);
    ib.setTiling(m_tiling);
    m_product->m_image = ib.build();

    ImageLayoutTransitionBuilder iltb;
    ImageLayoutTransitionDirector iltd;

    iltd.createBuilder<VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL>(iltb);
    iltb.setImage(*m_product->m_image);
    m_product->m_image->transitionImageLayout(*iltb.buildAndRestart());

    m_product->m_image->copyBufferToImage2D(stagingBuffer->getHandle());

    iltd.createBuilder<VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL>(iltb);
    iltb.setImage(*m_product->m_image);
    m_product->m_image->transitionImageLayout(*iltb.buildAndRestart());

    // image view

    m_product->m_imageView = m_product->m_image->createImageView2D();

    // sampler

    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    VkSamplerCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = m_samplerFilter,
        .minFilter = m_samplerFilter,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
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

    VkResult res = vkCreateSampler(deviceHandle, &createInfo, nullptr, &m_product->m_sampler);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create image sampler : " << res << std::endl;
        return nullptr;
    }

    auto result = std::move(m_product);
    restart();
    return result;
}

std::unique_ptr<Texture> CubemapBuilder::buildAndRestart()
{
    assert(m_device.lock());


    std::array<std::string, 6> filepath = { m_rightTextureFilename, m_leftTextureFilename,
                                          m_topTextureFilename, m_bottomTextureFilename,
                                          m_frontTextureFilename, m_backTextureFilename
                                        };

    size_t currentTotalSize = 0u;

    if (m_bLoadFromFile)
    {
        for (int i = 0; i < filepath.size(); i++)
        {
            const char* currentFilepath = filepath[i].c_str();
            int texWidth, texHeight, texChannels;
            stbi_uc* textureData = stbi_load(currentFilepath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            if (!textureData)
            {
                std::cerr << "Failed to load texture : " << m_rightTextureFilename << std::endl;
                return nullptr;
            }
            m_product->m_width = texWidth;
            m_product->m_height = texHeight;
            size_t currentImageSize = m_product->m_width * m_product->m_height * 4;

            m_product->m_imageData.resize(m_product->m_imageData.size() + currentImageSize);
            memcpy(m_product->m_imageData.data() + currentTotalSize, textureData, currentImageSize);

            stbi_image_free(textureData);

            currentTotalSize += currentImageSize;
        }
    }

    size_t totalSize = currentTotalSize;

    BufferBuilder bb;
    BufferDirector bd;
    bd.createStagingBufferBuilder(bb);
    bb.setDevice(m_device);
    bb.setSize(totalSize);
    std::unique_ptr<Buffer> stagingBuffer = bb.build();

    stagingBuffer->copyDataToMemory(m_product->m_imageData.data());

    ImageBuilder ib;
    ImageDirector id;
    id.createSampledImage3DBuilder(ib);
    ib.setDevice(m_device);
    ib.setFormat(m_format);
    ib.setWidth(m_product->m_width);
    ib.setHeight(m_product->m_height);
    ib.setTiling(m_tiling);
    m_product->m_image = ib.build();

    ImageLayoutTransitionBuilder iltb;
    ImageLayoutTransitionDirector iltd;

    iltd.createBuilder<VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL>(iltb);
    iltb.setImage(*m_product->m_image);
    iltb.setLayerCount(6U);
    m_product->m_image->transitionImageLayout(*iltb.buildAndRestart());

    m_product->m_image->copyBufferToImageCube(stagingBuffer->getHandle());

    iltd.createBuilder<VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL>(iltb);
    iltb.setImage(*m_product->m_image);
    iltb.setLayerCount(6U);
    m_product->m_image->transitionImageLayout(*iltb.buildAndRestart());

    // image view

    m_product->m_imageView = m_product->m_image->createImageViewCube();

    // sampler

    auto devicePtr = m_device.lock();
    auto deviceHandle = devicePtr->getHandle();

    VkSamplerCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = m_samplerFilter,
        .minFilter = m_samplerFilter,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
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

    VkResult res = vkCreateSampler(deviceHandle, &createInfo, nullptr, &m_product->m_sampler);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create image sampler : " << res << std::endl;
        return nullptr;
    }

    auto result = std::move(m_product);
    restart();
    return result;
}

void TextureDirector::createSRGBTextureBuilder(TextureBuilder& builder)
{
    builder.setFormat(VK_FORMAT_R8G8B8A8_SRGB);
    builder.setTiling(VK_IMAGE_TILING_OPTIMAL);
    builder.setSamplerFilter(VK_FILTER_NEAREST);
}

void TextureDirector::createSRGBTextureBuilder(CubemapBuilder& builder)
{
    builder.setFormat(VK_FORMAT_R8G8B8A8_SRGB);
    builder.setTiling(VK_IMAGE_TILING_OPTIMAL);
    builder.setSamplerFilter(VK_FILTER_NEAREST);
}