#pragma once

#include <functional>
#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

class Pipeline;
class Device;
class Buffer;
class CameraABC;
class Mesh;
class Model;
class Skybox;
class Light;
class Probe;
class ModelRenderStateBuilder;
class ImGuiRenderStateBuilder;
class SkyboxRenderStateBuilder;
class EnvironmentCaptureRenderStateBuilder;
class Texture;
class RenderPhase;

using DescriptorSetUpdatePred =
    std::function<void(const RenderPhase *parentPhase, uint32_t imageIndex, const VkDescriptorSet &set)>;

class RenderStateABC
{
  protected:
    struct ProbeContainer
    {
        struct Probe
        {
            glm::vec3 position;
        };

        Probe probes[8];
    };

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
        glm::mat4 views[6];
        glm::mat4 proj;
    };

    std::weak_ptr<Device> m_device;

    std::shared_ptr<Pipeline> m_pipeline;

    VkDescriptorPool m_descriptorPool;
    std::vector<VkDescriptorSet> m_descriptorSets;
    std::vector<std::unique_ptr<Buffer>> m_mvpUniformBuffers;
    std::vector<void *> m_mvpUniformBuffersMapped;
    std::vector<std::unique_ptr<Buffer>> m_probeStorageBuffers;
    std::vector<void*> m_probeStorageBuffersMapped;
    std::vector<std::unique_ptr<Buffer>> m_pointLightStorageBuffers;
    std::vector<void *> m_pointLightStorageBuffersMapped;
    std::vector<std::unique_ptr<Buffer>> m_directionalLightStorageBuffers;
    std::vector<void *> m_directionalLightStorageBuffersMapped;

    DescriptorSetUpdatePred m_descriptorSetUpdatePredPerFrame = nullptr;
    DescriptorSetUpdatePred m_descriptorSetUpdatePred = nullptr;

    RenderStateABC() = default;

  public:
    virtual ~RenderStateABC();

    virtual void updatePushConstants(const VkCommandBuffer& commandBuffer, uint32_t imageIndex, uint32_t singleFrameRenderIndex, const CameraABC &camera,
                                      const std::vector<std::shared_ptr<Light>> &lights) { }

    virtual void updateUniformBuffers(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex, const CameraABC &camera,
                                      const std::vector<std::shared_ptr<Light>> &lights, const std::vector<std::unique_ptr<Probe>> &probes, bool captureModeEnabled);
    virtual void updateDescriptorSetsPerFrame(const RenderPhase *parentPhase, uint32_t imageIndex);
    virtual void updateDescriptorSets(const RenderPhase *parentPhase, uint32_t imageIndex);

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
  protected:
    virtual void restart() = 0;

  public:
    virtual void setDevice(std::weak_ptr<Device> device) = 0;
    virtual void setPipeline(std::shared_ptr<Pipeline> pipeline) = 0;
    virtual void addPoolSize(VkDescriptorType poolSizeType) = 0;
    virtual void setFrameInFlightCount(uint32_t a) = 0;
    virtual void setTexture(std::weak_ptr<Texture> texture) = 0;
    virtual void setDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePred pred) = 0;
    virtual void setDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) = 0;

    virtual std::unique_ptr<RenderStateABC> build() = 0;
};

class ModelRenderState : public RenderStateABC
{
    friend ModelRenderStateBuilder;

  private:
    std::weak_ptr<Model> m_model;

    bool m_pushViewPosition = true;

  public:
    void updatePushConstants(const VkCommandBuffer& commandBuffer, uint32_t imageIndex, uint32_t singleFrameRenderIndex, const CameraABC& camera,
          const std::vector<std::shared_ptr<Light>>& lights) override;
    void recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer) override;
    
    void updateUniformBuffers(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex, const CameraABC& camera,
        const std::vector<std::shared_ptr<Light>>& lights, const std::vector<std::unique_ptr<Probe>> &probes, bool captureModeEnabled) override;
};

class ModelRenderStateBuilder : public RenderStateBuilderI
{
  private:
    std::unique_ptr<ModelRenderState> m_product;

    std::weak_ptr<Device> m_device;

    std::vector<VkDescriptorPoolSize> m_poolSizes;
    uint32_t m_frameInFlightCount;

    std::weak_ptr<Texture> m_texture;
    std::vector<std::weak_ptr<Texture>> m_environmentMaps;

    bool m_probeDescriptorEnable = true;
    bool m_lightDescriptorEnable = true;
    bool m_textureDescriptorEnable = true;
    bool m_mvpDescriptorEnable = true;

  public:
    ModelRenderStateBuilder()
    {
        restart();
    }

    void restart() override
    {
        m_product = std::unique_ptr<ModelRenderState>(new ModelRenderState);
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
    void setEnvironmentMaps(const std::vector<std::shared_ptr<Texture>>& textures)
    {
        m_environmentMaps.reserve(textures.size());
        for (const std::shared_ptr<Texture>& texture : textures)
            m_environmentMaps.push_back(texture);
    }
    void setDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePred pred) override
    {
        m_product->m_descriptorSetUpdatePredPerFrame = pred;
    }
    void setDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        m_product->m_descriptorSetUpdatePred = pred;
    }


    void setModel(std::shared_ptr<Model> mesh)
    {
        m_product->m_model = mesh;
    }

    void setProbeDescriptorEnable(bool a)
    {
        m_probeDescriptorEnable = a;
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
    void setPushViewPositionEnable(bool a)
    {
        m_product->m_pushViewPosition = a;
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

    void restart() override
    {
        m_product = std::unique_ptr<ImGuiRenderState>(new ImGuiRenderState);
    }

  public:
    ImGuiRenderStateBuilder()
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
    }
    void setDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePred pred) override
    {
        m_product->m_descriptorSetUpdatePredPerFrame = pred;
    }
    void setDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        m_product->m_descriptorSetUpdatePred = pred;
    }


    std::unique_ptr<RenderStateABC> build() override;
};

class SkyboxRenderState : public RenderStateABC
{
    friend SkyboxRenderStateBuilder;

  private:
    std::weak_ptr<Skybox> m_skybox;

  public:
    void updateUniformBuffers(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex, const CameraABC &camera,
                              const std::vector<std::shared_ptr<Light>> &lights, const std::vector<std::unique_ptr<Probe>> &probes, bool captureModeEnabled) override;

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

    bool m_textureDescriptorEnable = true;

    void restart() override
    {
        m_product = std::unique_ptr<SkyboxRenderState>(new SkyboxRenderState);
    }

  public:
    SkyboxRenderStateBuilder()
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
    void setDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePred pred) override
    {
        m_product->m_descriptorSetUpdatePredPerFrame = pred;
    }
    void setDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        m_product->m_descriptorSetUpdatePred = pred;
    }


    void setSkybox(std::shared_ptr<Skybox> skybox)
    {
        m_product->m_skybox = skybox;
    }

    void setTextureDescriptorEnable(bool a)
    {
        m_textureDescriptorEnable = a;
    }

    std::unique_ptr<RenderStateABC> build() override;
};

class EnvironmentCaptureRenderState : public RenderStateABC
{
    friend EnvironmentCaptureRenderStateBuilder;

private:
    std::weak_ptr<Skybox> m_skybox;

public:
    void updateUniformBuffers(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex, const CameraABC& camera,
        const std::vector<std::shared_ptr<Light>>& lights, const std::vector<std::unique_ptr<Probe>> &probes, bool captureModeEnabled) override;

    void recordBackBufferDrawObjectCommands(const VkCommandBuffer& commandBuffer) override;
};

class EnvironmentCaptureRenderStateBuilder : public RenderStateBuilderI
{
private:
    std::unique_ptr<EnvironmentCaptureRenderState> m_product;

    std::weak_ptr<Device> m_device;

    std::vector<VkDescriptorPoolSize> m_poolSizes;
    uint32_t m_frameInFlightCount;

    std::weak_ptr<Texture> m_texture;

    bool m_textureDescriptorEnable = true;

    void restart() override
    {
        m_product = std::unique_ptr<EnvironmentCaptureRenderState>(new EnvironmentCaptureRenderState);
    }

public:
    EnvironmentCaptureRenderStateBuilder()
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
    void setDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePred pred) override
    {
        m_product->m_descriptorSetUpdatePredPerFrame = pred;
    }
    void setDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        m_product->m_descriptorSetUpdatePred = pred;
    }


    void setSkybox(std::shared_ptr<Skybox> skybox)
    {
        m_product->m_skybox = skybox;
    }

    void setTextureDescriptorEnable(bool a)
    {
        m_textureDescriptorEnable = a;
    }

    std::unique_ptr<RenderStateABC> build() override;
};