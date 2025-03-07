#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

class Pipeline;
class Device;
class Buffer;
class CameraABC;
class Mesh;
class Skybox;
class Light;
class MeshRenderStateBuilder;
class ImGuiRenderStateBuilder;
class SkyboxRenderStateBuilder;
class Texture;

class RenderStateABC
{
  protected:
    struct PointLightContainer
    {
        struct PointLight
        {
            glm::vec3 diffuseColor;
            float diffusePower;
            glm::vec3 specularColor;
            float specularPower;
            glm::vec3 position;
            float pad0[1];
        };

        int pointLightCount;
        alignas(16) PointLight pointLights[2];
    };

    struct DirectionalLightContainer
    {
        struct DirectionalLight
        {
            glm::vec3 diffuseColor;
            float diffusePower;
            glm::vec3 specularColor;
            float specularPower;
            glm::vec3 direction;
            float pad0[1];
        };

        int directionalLightCount;
        alignas(16) DirectionalLight directionalLights[2];
    };

    struct MVP
    {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    std::weak_ptr<Device> m_device;

    std::shared_ptr<Pipeline> m_pipeline;

    VkDescriptorPool m_descriptorPool;
    std::vector<VkDescriptorSet> m_descriptorSets;
    std::vector<std::unique_ptr<Buffer>> m_mvpUniformBuffers;
    std::vector<void *> m_mvpUniformBuffersMapped;
    std::vector<std::unique_ptr<Buffer>> m_pointLightStorageBuffers;
    std::vector<void *> m_pointLightStorageBuffersMapped;
    std::vector<std::unique_ptr<Buffer>> m_directionalLightStorageBuffers;
    std::vector<void *> m_directionalLightStorageBuffersMapped;

    RenderStateABC() = default;

  public:
    virtual ~RenderStateABC();

    virtual void updateUniformBuffers(uint32_t imageIndex, const CameraABC &camera,
                                      const std::vector<std::shared_ptr<Light>> &lights);

    virtual void recordBackBufferDescriptorSetsCommands(const VkCommandBuffer &commandBuffer, uint32_t imageIndex);
    virtual void recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer) = 0;

  public:
    [[nodiscard]] std::shared_ptr<Pipeline> getPipeline() const
    {
        return m_pipeline;
    }
    [[nodiscard]] VkDescriptorPool getDescriptorPool() const
    {
        return m_descriptorPool;
    }
};

class RenderStateBuilderI
{
  public:
    virtual void restart() = 0;

    virtual void setDevice(std::weak_ptr<Device> device) = 0;
    virtual void setPipeline(std::shared_ptr<Pipeline> pipeline) = 0;
    virtual void addPoolSize(VkDescriptorType poolSizeType) = 0;
    virtual void setFrameInFlightCount(uint32_t a) = 0;
    virtual void setTexture(std::weak_ptr<Texture> texture) = 0;

    virtual std::unique_ptr<RenderStateABC> build() = 0;
};

class MeshRenderState : public RenderStateABC
{
    friend MeshRenderStateBuilder;

  private:
    std::weak_ptr<Mesh> m_mesh;

  public:
    void recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer) override;
};

class MeshRenderStateBuilder : public RenderStateBuilderI
{
  private:
    std::unique_ptr<MeshRenderState> m_product;

    std::weak_ptr<Device> m_device;

    std::vector<VkDescriptorPoolSize> m_poolSizes;
    uint32_t m_frameInFlightCount;

    std::weak_ptr<Texture> m_texture;

    bool m_lightDescriptorEnable = true;
    bool m_textureDescriptorEnable = true;
    bool m_mvpDescriptorEnable = true;

    void restart() override
    {
        m_product = std::unique_ptr<MeshRenderState>(new MeshRenderState);
    }

  public:
    MeshRenderStateBuilder()
    {
        restart();
    }

    void setDevice(std::weak_ptr<Device> device) override
    {
        m_device = device;
        m_product->m_device = device;
    }
    void setPipeline(std::shared_ptr<Pipeline> pipeline) override;
    void addPoolSize(VkDescriptorType poolSizeType) override;
    void setFrameInFlightCount(uint32_t a) override
    {
        m_frameInFlightCount = a;
    }
    void setTexture(std::weak_ptr<Texture> texture) override
    {
        m_texture = texture;
    }

    void setMesh(std::shared_ptr<Mesh> mesh)
    {
        m_product->m_mesh = mesh;
    }

    void setLightDescriptorEnable(bool a)
    {
        m_lightDescriptorEnable = a;
    }
    void setTextureDescriptorEnable(bool a)
    {
        m_textureDescriptorEnable = a;
    }
    void setMVPDescriptorEnable(bool a)
    {
        m_mvpDescriptorEnable = a;
    }

    std::unique_ptr<RenderStateABC> build() override;
};

class ImGuiRenderState : public RenderStateABC
{
    friend ImGuiRenderStateBuilder;

  public:
    void recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer) override;
};

class ImGuiRenderStateBuilder : public RenderStateBuilderI
{
  private:
    std::unique_ptr<ImGuiRenderState> m_product;

    std::weak_ptr<Device> m_device;

    std::vector<VkDescriptorPoolSize> m_poolSizes;
    uint32_t m_frameInFlightCount;

    std::weak_ptr<Texture> m_texture;

  public:
    ImGuiRenderStateBuilder()
    {
        restart();
    }

    void restart() override
    {
        m_product = std::unique_ptr<ImGuiRenderState>(new ImGuiRenderState);
    }

    void setDevice(std::weak_ptr<Device> device) override
    {
        m_device = device;
        m_product->m_device = device;
    }
    void setPipeline(std::shared_ptr<Pipeline> pipeline) override;
    void addPoolSize(VkDescriptorType poolSizeType) override;
    void setFrameInFlightCount(uint32_t a) override
    {
        m_frameInFlightCount = a;
    }

    void setTexture(std::weak_ptr<Texture> texture) override
    {
    }

    std::unique_ptr<RenderStateABC> build() override;
};

class SkyboxRenderState : public RenderStateABC
{
    friend SkyboxRenderStateBuilder;

  private:
    std::weak_ptr<Skybox> m_skybox;

  public:
    void updateUniformBuffers(uint32_t imageIndex, const CameraABC &camera,
                              const std::vector<std::shared_ptr<Light>> &lights) override;

    void recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer) override;
};

class SkyboxRenderStateBuilder : public RenderStateBuilderI
{
  private:
    std::unique_ptr<SkyboxRenderState> m_product;

    std::weak_ptr<Device> m_device;

    std::vector<VkDescriptorPoolSize> m_poolSizes;
    uint32_t m_frameInFlightCount;

    std::weak_ptr<Texture> m_texture;

  public:
    SkyboxRenderStateBuilder()
    {
        restart();
    }

    void restart() override
    {
        m_product = std::unique_ptr<SkyboxRenderState>(new SkyboxRenderState);
    }

    void setDevice(std::weak_ptr<Device> device) override
    {
        m_device = device;
        m_product->m_device = device;
    }
    void setPipeline(std::shared_ptr<Pipeline> pipeline) override;
    void addPoolSize(VkDescriptorType poolSizeType) override;
    void setFrameInFlightCount(uint32_t a) override
    {
        m_frameInFlightCount = a;
    }
    void setTexture(std::weak_ptr<Texture> texture) override
    {
        m_texture = texture;
    }

    void setSkybox(std::shared_ptr<Skybox> skybox)
    {
        m_product->m_skybox = skybox;
    }

    std::unique_ptr<RenderStateABC> build() override;
};