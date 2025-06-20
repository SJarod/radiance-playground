#include <iostream>

#include "graphics/buffer.hpp"
#include "graphics/device.hpp"
#include "graphics/image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "texture.hpp"

Texture::~Texture()
{
    std::cout << "Destroying texture " << m_name << std::endl;
    if (m_image)
        std::cout << "\t" << m_image->getName() << std::endl;

    m_image.reset();
    if (m_device.expired())
        return;

    auto deviceHandle = m_device.lock()->getHandle();
    vkDestroySampler(deviceHandle, *m_sampler, nullptr);
    vkDestroyImageView(deviceHandle, m_imageView, nullptr);
}

std::unique_ptr<Texture> TextureBuilder::buildAndRestart()
{
    assert(m_device.lock());

    size_t imageSize = m_product->m_width * m_product->m_height * 4;

    if (m_bLoadFromFile)
    {
        stbi_set_flip_vertically_on_load(true);

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
    bd.configureStagingBufferBuilder(bb);
    bb.setDevice(m_device);
    bb.setSize(imageSize);
    bb.setName("Texture Staging Buffer");
    std::unique_ptr<Buffer> stagingBuffer = bb.build();

    stagingBuffer->copyDataToMemory(m_product->m_imageData.data());

    ImageBuilder ib;
    ImageDirector id;
    id.configureSampledImage2DBuilder(ib);
    ib.setDevice(m_device);
    ib.setFormat(m_format);
    ib.setWidth(m_product->m_width);
    ib.setHeight(m_product->m_height);
    ib.setTiling(m_tiling);
    ib.setName(m_textureFilename + m_product->m_name + " Texture");

    if (m_initialLayout.has_value())
        ib.setInitialLayout(m_initialLayout.value());

    m_product->m_image = ib.build();

    std::cout << "Creating texture " << m_product->m_name << " : " << m_product->m_image->getName() << std::endl;

    ImageLayoutTransitionBuilder iltb;
    ImageLayoutTransitionDirector iltd;

    iltd.configureBuilder<VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL>(iltb);
    iltb.setImage(*m_product->m_image);
    m_product->m_image->transitionImageLayout(*iltb.buildAndRestart());

    m_product->m_image->copyBufferToImage2D(stagingBuffer->getHandle());

    iltd.configureBuilder<VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL>(iltb);
    iltb.setImage(*m_product->m_image);
    m_product->m_image->transitionImageLayout(*iltb.buildAndRestart());

    // image view

    m_product->m_imageView = m_product->m_image->createImageView2D();

    auto devicePtr = m_device.lock();
    static int viewCount = 0;
    devicePtr->addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_IMAGE_VIEW,
        .objectHandle = (uint64_t)m_product->m_imageView,
        .pObjectName = std::string("Texture Image View" + std::to_string(viewCount++)).c_str(),
    });

    // depth view

    if (m_depthImageEnable)
    {
        ImageBuilder depthIb;
        ImageDirector depthId;
        depthId.configureDepthImage2DBuilder(depthIb);
        depthIb.setDevice(m_product->m_device);
        depthIb.setWidth(m_product->m_width);
        depthIb.setHeight(m_product->m_height);
        depthIb.setName("Depth Texture");
        m_product->m_depthImage = depthIb.build();

        ImageLayoutTransitionBuilder depthIltb;
        ImageLayoutTransitionDirector depthIltd;
        depthIltd.configureBuilder<VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL>(
            depthIltb);
        depthIltb.setImage(*m_product->m_depthImage.value());
        m_product->m_depthImage.value()->transitionImageLayout(*depthIltb.buildAndRestart());

        m_product->m_depthImageView = m_product->m_depthImage.value()->createImageView2D();
    }

    // sampler

    SamplerBuilder sb;
    sb.setDevice(m_device);
    sb.setMagFilter(m_samplerFilter);
    sb.setMinFilter(m_samplerFilter);
    sb.setAddressModeXYZ(VK_SAMPLER_ADDRESS_MODE_REPEAT);
    m_product->m_sampler = sb.build();

    auto result = std::move(m_product);
    restart();
    return result;
}

std::unique_ptr<Texture> CubemapBuilder::buildAndRestart()
{
    assert(m_device.lock());

    std::array<std::string, 6> filepath = {m_rightTextureFilename,  m_leftTextureFilename,  m_topTextureFilename,
                                           m_bottomTextureFilename, m_frontTextureFilename, m_backTextureFilename};

    size_t currentTotalSize = m_product->m_width * m_product->m_height * 6u * 4;

    if (m_createFromUserData)
    {
        if (m_bLoadFromFile)
        {
            size_t cursor = 0u;
            for (int i = 0; i < filepath.size(); i++)
            {
                stbi_set_flip_vertically_on_load(false);

                const char *currentFilepath = filepath[i].c_str();
                int texWidth, texHeight, texChannels;
                stbi_uc *textureData = stbi_load(currentFilepath, &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
                if (!textureData)
                {
                    std::cerr << "Failed to load texture : " << m_rightTextureFilename << std::endl;
                    return nullptr;
                }
                m_product->m_width = texWidth;
                m_product->m_height = texHeight;
                size_t currentImageSize = m_product->m_width * m_product->m_height * 4;

                m_product->m_imageData.resize(m_product->m_imageData.size() + currentImageSize);
                memcpy(m_product->m_imageData.data() + cursor, textureData, currentImageSize);

                stbi_image_free(textureData);

                cursor += currentImageSize;
            }

            currentTotalSize = m_product->m_width * m_product->m_height * 6u * 4;
        }
        else
        {
            m_product->m_imageData.reserve(currentTotalSize);
        }

        size_t totalSize = currentTotalSize;

        BufferBuilder bb;
        BufferDirector bd;
        bd.configureStagingBufferBuilder(bb);
        bb.setDevice(m_device);
        bb.setSize(totalSize);
        bb.setName("Cubemap Staging Buffer");
        std::unique_ptr<Buffer> stagingBuffer = bb.build();

        stagingBuffer->copyDataToMemory(m_product->m_imageData.data());

        ImageBuilder ib;
        ImageDirector id;

        if (!m_isResolveTexture)
        {
            id.configureSampledImageCubeBuilder(ib);
        }
        else
        {
            id.configureSampledResolveImageCubeBuilder(ib);
        }

        ib.setDevice(m_device);
        ib.setFormat(m_format);
        ib.setWidth(m_product->m_width);
        ib.setHeight(m_product->m_height);
        ib.setTiling(m_tiling);
        ib.setName("Cubemap Texture");

        if (m_initialLayout.has_value())
            ib.setInitialLayout(m_initialLayout.value());

        m_product->m_image = ib.build();

        ImageLayoutTransitionBuilder iltb;
        ImageLayoutTransitionDirector iltd;

        iltd.configureBuilder<VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL>(iltb);
        iltb.setImage(*m_product->m_image);
        iltb.setLayerCount(6U);
        m_product->m_image->transitionImageLayout(*iltb.buildAndRestart());

        m_product->m_image->copyBufferToImageCube(stagingBuffer->getHandle());

        if (!m_isResolveTexture)
        {
            iltd.configureBuilder<VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL>(iltb);
        }
        else
        {
            iltd.configureBuilder<VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL>(iltb);
        }

        iltb.setImage(*m_product->m_image);
        iltb.setLayerCount(6U);
        m_product->m_image->transitionImageLayout(*iltb.buildAndRestart());

        // image view

        m_product->m_imageView = m_product->m_image->createImageViewCube();
    }
    else
    {
        ImageBuilder ib;
        ImageDirector id;

        if (!m_isResolveTexture)
        {
            if (m_isSamplableTexture)
            {
                id.configureSampledImageCubeBuilder(ib);
            }
            else
            {
                id.configureNonSampledImageCubeBuilder(ib);
            }
        }
        else
        {
            id.configureSampledResolveImageCubeBuilder(ib);
        }

        ib.setDevice(m_device);
        ib.setFormat(m_format);
        ib.setWidth(m_product->m_width);
        ib.setHeight(m_product->m_height);
        ib.setTiling(m_tiling);
        ib.setName("Cubemap");

        if (m_initialLayout.has_value())
            ib.setInitialLayout(m_initialLayout.value());

        m_product->m_image = ib.build();

        m_product->m_imageView = m_product->m_image->createImageViewCube();
    }

    // depth view

    if (m_depthImageEnable)
    {
        ImageBuilder depthIb;
        ImageDirector depthId;
        depthId.configureDepthImageCubeBuilder(depthIb);
        depthIb.setDevice(m_product->m_device);
        depthIb.setWidth(m_product->m_width);
        depthIb.setHeight(m_product->m_height);
        depthIb.setName("Cubmap Depth");
        m_product->m_depthImage = depthIb.build();

        ImageLayoutTransitionBuilder depthIltb;
        ImageLayoutTransitionDirector depthIltd;
        depthIltd.configureBuilder<VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL>(
            depthIltb);
        depthIltb.setImage(*m_product->m_depthImage.value());
        depthIltb.setLayerCount(6U);
        m_product->m_depthImage.value()->transitionImageLayout(*depthIltb.buildAndRestart());

        m_product->m_depthImageView = m_product->m_depthImage.value()->createImageViewCube();
    }

    // sampler

    SamplerBuilder sb;
    sb.setDevice(m_device);
    sb.setMagFilter(m_samplerFilter);
    sb.setMinFilter(m_samplerFilter);
    sb.setAddressModeXYZ(VK_SAMPLER_ADDRESS_MODE_REPEAT);
    m_product->m_sampler = sb.build();

    auto result = std::move(m_product);
    restart();
    return result;
}

void TextureDirector::configureSRGBTextureBuilder(TextureBuilder &builder)
{
    builder.setFormat(VK_FORMAT_R8G8B8A8_SRGB);
    builder.setTiling(VK_IMAGE_TILING_OPTIMAL);
    // TODO : linear ?
    builder.setSamplerFilter(VK_FILTER_NEAREST);
}

void TextureDirector::configureSRGBTextureBuilder(CubemapBuilder &builder)
{
    builder.setFormat(VK_FORMAT_R8G8B8A8_SRGB);
    builder.setTiling(VK_IMAGE_TILING_OPTIMAL);
    builder.setSamplerFilter(VK_FILTER_NEAREST);
}

void TextureDirector::configureUNORMTextureBuilder(TextureBuilder &builder)
{
    builder.setFormat(VK_FORMAT_R8G8B8A8_UNORM);
    builder.setTiling(VK_IMAGE_TILING_OPTIMAL);
    builder.setSamplerFilter(VK_FILTER_NEAREST);
}

void TextureDirector::configureUNORMTextureBuilder(CubemapBuilder &builder)
{
    builder.setFormat(VK_FORMAT_R8G8B8A8_UNORM);
    builder.setTiling(VK_IMAGE_TILING_OPTIMAL);
    builder.setSamplerFilter(VK_FILTER_NEAREST);
}