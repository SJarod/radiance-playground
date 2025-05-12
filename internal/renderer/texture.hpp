#pragma once

#include <optional>
#include <vulkan/vulkan.h>

#include "graphics/image.hpp"

class Device;
class TextureBuilder;
class CubemapBuilder;

class Texture
{
    friend TextureBuilder;
    friend CubemapBuilder;

  private:
    std::weak_ptr<Device> m_device;

    std::string m_name = "Unnamed";

    uint32_t m_width;
    uint32_t m_height;

    std::unique_ptr<Image> m_image;
    VkImageView m_imageView;
    std::unique_ptr<VkSampler> m_sampler;

    std::optional<std::unique_ptr<Image>> m_depthImage;
    std::optional<VkImageView> m_depthImageView;

    std::vector<unsigned char> m_imageData;

    Texture() = default;

  public:
    ~Texture();

    Texture(const Texture &) = delete;
    Texture &operator=(const Texture &) = delete;
    Texture(Texture &&) = delete;
    Texture &operator=(Texture &&) = delete;

  public:
    [[nodiscard]] inline const VkSampler *getSampler() const
    {
        return m_sampler.get();
    }
    [[nodiscard]] inline const VkImageView &getImageView() const
    {
        return m_imageView;
    }
    [[nodiscard]] inline const std::optional<VkImageView> &getDepthImageView() const
    {
        return m_depthImageView;
    }
    [[nodiscard]] inline VkFormat getImageFormat() const
    {
        return m_image->getFormat();
    }
    [[nodiscard]] inline std::optional<VkFormat> getDepthImageFormat() const
    {
        if (m_depthImage.has_value())
            return m_depthImage.value()->getFormat();

        return std::nullopt;
    }
    [[nodiscard]] inline const uint32_t &getWidth() const
    {
        return m_width;
    }

    [[nodiscard]] inline const uint32_t &getHeight() const
    {
        return m_height;
    }
    [[nodiscard]] inline std::string getName() const
    {
        return m_name;
    }
};

class TextureBuilder
{
  private:
    std::unique_ptr<Texture> m_product;

    std::weak_ptr<Device> m_device;

    VkFormat m_format;
    VkImageTiling m_tiling;
    VkFilter m_samplerFilter;
    std::optional<VkImageLayout> m_initialLayout;

    std::string m_textureFilename;
    bool m_bLoadFromFile = false;
    bool m_depthImageEnable = false;

    void restart()
    {
        m_product = std::unique_ptr<Texture>(new Texture);
    }

  public:
    TextureBuilder()
    {
        restart();
    }

    void setDevice(std::weak_ptr<Device> device)
    {
        m_device = device;
        m_product->m_device = device;
    }

    void setWidth(uint32_t a)
    {
        m_product->m_width = a;
    }
    void setHeight(uint32_t a)
    {
        m_product->m_height = a;
    }

    void setImageData(const std::vector<unsigned char> &data)
    {
        m_product->m_imageData = data;
        m_bLoadFromFile = false;
    }

    void setTextureFilename(const std::string &filename)
    {
        m_textureFilename = filename;
        m_bLoadFromFile = true;
    }

    void setFormat(VkFormat a)
    {
        m_format = a;
    }
    void setTiling(VkImageTiling a)
    {
        m_tiling = a;
    }
    void setSamplerFilter(VkFilter a)
    {
        m_samplerFilter = a;
    }
    void setInitialLayout(VkImageLayout a)
    {
        m_initialLayout = a;
    }
    void setDepthImageEnable(bool enable)
    {
        m_depthImageEnable = enable;
    }
    void setName(std::string name)
    {
        m_product->m_name = name;
    }

    std::unique_ptr<Texture> buildAndRestart();
};

class CubemapBuilder
{
  private:
    std::unique_ptr<Texture> m_product;

    std::weak_ptr<Device> m_device;

    VkFormat m_format;
    VkImageTiling m_tiling;
    VkFilter m_samplerFilter;
    std::optional<VkImageLayout> m_initialLayout;
    bool m_isResolveTexture = false;
    bool m_isSamplableTexture = true;
    bool m_createFromUserData = true;

    std::string m_rightTextureFilename;
    std::string m_leftTextureFilename;
    std::string m_topTextureFilename;
    std::string m_bottomTextureFilename;
    std::string m_frontTextureFilename;
    std::string m_backTextureFilename;
    bool m_bLoadFromFile = false;
    bool m_depthImageEnable = false;

    void restart()
    {
        m_product = std::unique_ptr<Texture>(new Texture);
    }

  public:
    CubemapBuilder()
    {
        restart();
    }

    void setDevice(std::weak_ptr<Device> device)
    {
        m_device = device;
        m_product->m_device = device;
    }

    void setWidth(uint32_t a)
    {
        m_product->m_width = a;
    }
    void setHeight(uint32_t a)
    {
        m_product->m_height = a;
    }

    void setImageData(const std::vector<unsigned char> &data)
    {
        m_product->m_imageData = data;
        m_bLoadFromFile = false;
    }

    void setRightTextureFilename(const std::string &filename)
    {
        m_rightTextureFilename = filename;
        m_bLoadFromFile = true;
    }

    void setLeftTextureFilename(const std::string &filename)
    {
        m_leftTextureFilename = filename;
        m_bLoadFromFile = true;
    }

    void setTopTextureFilename(const std::string &filename)
    {
        m_topTextureFilename = filename;
        m_bLoadFromFile = true;
    }

    void setBottomTextureFilename(const std::string &filename)
    {
        m_bottomTextureFilename = filename;
        m_bLoadFromFile = true;
    }

    void setFrontTextureFilename(const std::string &filename)
    {
        m_frontTextureFilename = filename;
        m_bLoadFromFile = true;
    }

    void setBackTextureFilename(const std::string &filename)
    {
        m_backTextureFilename = filename;
        m_bLoadFromFile = true;
    }

    void setFormat(VkFormat a)
    {
        m_format = a;
    }
    void setTiling(VkImageTiling a)
    {
        m_tiling = a;
    }
    void setSamplerFilter(VkFilter a)
    {
        m_samplerFilter = a;
    }
    void setInitialLayout(VkImageLayout a)
    {
        m_initialLayout = a;
    }
    void setResolveEnable(int isResolveTexture)
    {
        m_isResolveTexture = isResolveTexture;
    }
    void setSamplableEnable(int isSamplableTexture)
    {
        m_isSamplableTexture = isSamplableTexture;
    }
    void setCreateFromUserData(bool enable)
    {
        m_createFromUserData = enable;
    }
    void setDepthImageEnable(bool enable)
    {
        m_depthImageEnable = enable;
    }
    void setName(std::string name)
    {
        m_product->m_name = name;
    }

    std::unique_ptr<Texture> buildAndRestart();
};

class TextureDirector
{
  public:
    void configureSRGBTextureBuilder(TextureBuilder &builder);
    void configureSRGBTextureBuilder(CubemapBuilder &builder);
    void configureUNORMTextureBuilder(TextureBuilder &builder);
    void configureUNORMTextureBuilder(CubemapBuilder &builder);
};