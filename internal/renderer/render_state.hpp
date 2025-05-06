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
class ProbeGrid;
class ModelRenderStateBuilder;
class ImGuiRenderStateBuilder;
class SkyboxRenderStateBuilder;
class EnvironmentCaptureRenderStateBuilder;
class ProbeGridRenderStateBuilder;
class Texture;
class RenderPhase;

using DescriptorSetUpdatePred =
    std::function<void(const RenderPhase *parentPhase, uint32_t imageIndex, const VkDescriptorSet &set)>;
using DescriptorSetUpdatePredPerFrame = std::function<void(const RenderPhase *parentPhase, uint32_t imageIndex,
                                                           const VkDescriptorSet &set, uint32_t backBufferIndex)>;

/**
 * @brief state that can be taken into account by any phase to utilize the GPU.
 *
 */
class GPUStateI
{
  public:
    virtual void recordBackBufferComputeCommands(const VkCommandBuffer &commandBuffer) = 0;
    virtual void updateUniformBuffers(uint32_t imageIndex) = 0;

    virtual [[nodiscard]] std::shared_ptr<Pipeline> getPipeline() const = 0;
    virtual [[nodiscard]] VkDescriptorPool getDescriptorPool() const = 0;
};

/**
 * @brief state used by graphics pipelines
 *
 */
class RenderStateABC : public GPUStateI
{
  protected:
    struct ProbeContainer
    {
        struct Probe
        {
            glm::vec3 position;
            float pad0[1];
        };

        glm::uvec3 dimensions;
        float pad0[1];
        glm::vec3 extent;
        float pad1[1];
        glm::vec3 cornerPosition;
        float pad2[1];
        Probe probes[64];
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
            glm::vec3 attenuation;
            float pad1[1];
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
    std::vector<VkDescriptorSet> m_instanceDescriptorSets;
    std::vector<std::vector<VkDescriptorSet>> m_materialDescriptorSetsPerSubObject;
    std::vector<std::unique_ptr<Buffer>> m_mvpUniformBuffers;
    std::vector<void *> m_mvpUniformBuffersMapped;
    std::vector<std::unique_ptr<Buffer>> m_probeStorageBuffers;
    std::vector<void *> m_probeStorageBuffersMapped;
    std::vector<std::unique_ptr<Buffer>> m_pointLightStorageBuffers;
    std::vector<void *> m_pointLightStorageBuffersMapped;
    std::vector<std::unique_ptr<Buffer>> m_directionalLightStorageBuffers;
    std::vector<void *> m_directionalLightStorageBuffersMapped;

    DescriptorSetUpdatePredPerFrame m_instanceDescriptorSetUpdatePredPerFrame = nullptr;
    DescriptorSetUpdatePred m_instanceDescriptorSetUpdatePred = nullptr;
    DescriptorSetUpdatePredPerFrame m_materialDescriptorSetUpdatePredPerFrame = nullptr;
    DescriptorSetUpdatePred m_materialDescriptorSetUpdatePred = nullptr;

    bool m_instanceDescriptorSetEnable = true;
    bool m_materialDescriptorSetEnable = true;

    RenderStateABC() = default;

  public:
    virtual ~RenderStateABC();

    virtual void updatePushConstants(const VkCommandBuffer &commandBuffer, uint32_t imageIndex,
                                     uint32_t singleFrameRenderIndex, const CameraABC &camera,
                                     const std::vector<std::shared_ptr<Light>> &lights)
    {
    }

    virtual void updateUniformBuffers(uint32_t imageIndex, uint32_t singleFrameRenderIndex,
                                      uint32_t pooledFramebufferIndex, const CameraABC &camera,
                                      const std::vector<std::shared_ptr<Light>> &lights,
                                      const std::shared_ptr<ProbeGrid> &probeGrid, bool captureModeEnabled);
    virtual void updateDescriptorSetsPerFrame(const RenderPhase *parentPhase, uint32_t imageIndex,
                                              uint32_t backBufferIndex);
    virtual void updateDescriptorSets(const RenderPhase *parentPhase, uint32_t imageIndex);

    virtual void recordBackBufferDescriptorSetsCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex,
                                                        uint32_t imageIndex);
    virtual void recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex) = 0;

    /**
     * @brief no implementation yet
     *
     * @param commandBuffer
     */
    [[deprecated]] void recordBackBufferComputeCommands(const VkCommandBuffer &commandBuffer) override
    {
    }
    /**
     * @brief no implementation yet
     *
     * @param imageIndex
     */
    [[deprecated]] void updateUniformBuffers(uint32_t imageIndex) override
    {
    }

    virtual uint32_t getSubObjectCount() const = 0;

  public:
    [[nodiscard]] std::shared_ptr<Pipeline> getPipeline() const override
    {
        return m_pipeline;
    }
    [[nodiscard]] VkDescriptorPool getDescriptorPool() const override
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
    virtual void setInstanceDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) = 0;
    virtual void setInstanceDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) = 0;
    virtual void setMaterialDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) = 0;
    virtual void setMaterialDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) = 0;
    virtual void setInstanceDescriptorEnable(bool enable) = 0;
    virtual void setMaterialDescriptorEnable(bool enable) = 0;

    virtual std::unique_ptr<RenderStateABC> build() = 0;
};

class ModelRenderState : public RenderStateABC
{
    friend ModelRenderStateBuilder;

  private:
    std::weak_ptr<Model> m_model;

    bool m_pushViewPosition = true;

  public:
    void updatePushConstants(const VkCommandBuffer &commandBuffer, uint32_t imageIndex, uint32_t singleFrameRenderIndex,
                             const CameraABC &camera, const std::vector<std::shared_ptr<Light>> &lights) override;
    void recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex) override;

    void updateUniformBuffers(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex,
                              const CameraABC &camera, const std::vector<std::shared_ptr<Light>> &lights,
                              const std::shared_ptr<ProbeGrid> &probeGrid, bool captureModeEnabled) override;

    uint32_t getSubObjectCount() const override;

    static std::shared_ptr<Texture> s_defaultDiffuseTexture;
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

    void restart() override
    {
        m_product = std::unique_ptr<ModelRenderState>(new ModelRenderState);
    }

  public:
    ModelRenderStateBuilder()
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
    void setEnvironmentMaps(const std::vector<std::shared_ptr<Texture>> &textures)
    {
        m_environmentMaps.reserve(textures.size());
        for (const std::shared_ptr<Texture> &texture : textures)
            m_environmentMaps.push_back(texture);
    }
    void setInstanceDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        m_product->m_instanceDescriptorSetUpdatePredPerFrame = pred;
    }
    void setInstanceDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        m_product->m_instanceDescriptorSetUpdatePred = pred;
    }
    void setMaterialDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        m_product->m_materialDescriptorSetUpdatePredPerFrame = pred;
    }
    void setMaterialDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        m_product->m_materialDescriptorSetUpdatePred = pred;
    }
    void setInstanceDescriptorEnable(bool enable) override
    {
        m_product->m_instanceDescriptorSetEnable = enable;
    }
    void setMaterialDescriptorEnable(bool enable) override
    {
        m_product->m_materialDescriptorSetEnable = enable;
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
    void recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex) override;

    uint32_t getSubObjectCount() const override
    {
        return 1u;
    }
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
    void setInstanceDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        m_product->m_instanceDescriptorSetUpdatePredPerFrame = pred;
    }
    void setInstanceDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        m_product->m_instanceDescriptorSetUpdatePred = pred;
    }
    void setMaterialDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        m_product->m_materialDescriptorSetUpdatePredPerFrame = pred;
    }
    void setMaterialDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        m_product->m_materialDescriptorSetUpdatePred = pred;
    }
    void setInstanceDescriptorEnable(bool enable) override
    {
        m_product->m_instanceDescriptorSetEnable = enable;
    }
    void setMaterialDescriptorEnable(bool enable) override
    {
        m_product->m_materialDescriptorSetEnable = enable;
    }

    std::unique_ptr<RenderStateABC> build() override;
};

class SkyboxRenderState : public RenderStateABC
{
    friend SkyboxRenderStateBuilder;

  private:
    std::weak_ptr<Skybox> m_skybox;

  public:
    void updateUniformBuffers(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex,
                              const CameraABC &camera, const std::vector<std::shared_ptr<Light>> &lights,
                              const std::shared_ptr<ProbeGrid> &probeGrid, bool captureModeEnabled) override;

    void recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex) override;

    uint32_t getSubObjectCount() const override
    {
        return 1u;
    }
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
    void setInstanceDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        m_product->m_instanceDescriptorSetUpdatePredPerFrame = pred;
    }
    void setInstanceDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        m_product->m_instanceDescriptorSetUpdatePred = pred;
    }
    void setMaterialDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        m_product->m_materialDescriptorSetUpdatePredPerFrame = pred;
    }
    void setMaterialDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        m_product->m_materialDescriptorSetUpdatePred = pred;
    }
    void setInstanceDescriptorEnable(bool enable) override
    {
        m_product->m_instanceDescriptorSetEnable = enable;
    }
    void setMaterialDescriptorEnable(bool enable) override
    {
        m_product->m_materialDescriptorSetEnable = enable;
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
    void updateUniformBuffers(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex,
                              const CameraABC &camera, const std::vector<std::shared_ptr<Light>> &lights,
                              const std::shared_ptr<ProbeGrid> &probeGrid, bool captureModeEnabled) override;

    void recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex) override;

    uint32_t getSubObjectCount() const override
    {
        return 1u;
    }
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
    void setInstanceDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        m_product->m_instanceDescriptorSetUpdatePredPerFrame = pred;
    }
    void setInstanceDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        m_product->m_instanceDescriptorSetUpdatePred = pred;
    }
    void setMaterialDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        m_product->m_materialDescriptorSetUpdatePredPerFrame = pred;
    }
    void setMaterialDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        m_product->m_materialDescriptorSetUpdatePred = pred;
    }
    void setInstanceDescriptorEnable(bool enable) override
    {
        m_product->m_instanceDescriptorSetEnable = enable;
    }
    void setMaterialDescriptorEnable(bool enable) override
    {
        m_product->m_materialDescriptorSetEnable = enable;
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

class ProbeGridRenderState : public RenderStateABC
{
    friend ProbeGridRenderStateBuilder;

  private:
    std::weak_ptr<ProbeGrid> m_grid;

    std::shared_ptr<Mesh> m_mesh;

  public:
    void updateUniformBuffers(uint32_t imageIndex, uint32_t singleFrameRenderIndex, uint32_t pooledFramebufferIndex,
                              const CameraABC &camera, const std::vector<std::shared_ptr<Light>> &lights,
                              const std::shared_ptr<ProbeGrid> &probeGrid, bool captureModeEnabled) override;

    void recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex) override;

    uint32_t getSubObjectCount() const override;
};

class ProbeGridRenderStateBuilder : public RenderStateBuilderI
{
  private:
    std::unique_ptr<ProbeGridRenderState> m_product;

    std::weak_ptr<Device> m_device;

    std::vector<std::weak_ptr<Texture>> m_environmentMaps;

    std::vector<VkDescriptorPoolSize> m_poolSizes;
    uint32_t m_frameInFlightCount;

    void restart() override
    {
        m_product = std::unique_ptr<ProbeGridRenderState>(new ProbeGridRenderState);
    }

  public:
    ProbeGridRenderStateBuilder()
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

    void setProbeGrid(std::weak_ptr<ProbeGrid> grid)
    {
        m_product->m_grid = grid;
    }

    void setMesh(std::shared_ptr<Mesh> mesh)
    {
        m_product->m_mesh = mesh;
    }
    void setTexture(std::weak_ptr<Texture> texture) override
    {
    }
    void setEnvironmentMaps(const std::vector<std::shared_ptr<Texture>> &textures)
    {
        m_environmentMaps.reserve(textures.size());
        for (const std::shared_ptr<Texture> &texture : textures)
            m_environmentMaps.push_back(texture);
    }
    void setInstanceDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        m_product->m_instanceDescriptorSetUpdatePredPerFrame = pred;
    }
    void setInstanceDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        m_product->m_instanceDescriptorSetUpdatePred = pred;
    }
    void setMaterialDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        m_product->m_materialDescriptorSetUpdatePredPerFrame = pred;
    }
    void setMaterialDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        m_product->m_materialDescriptorSetUpdatePred = pred;
    }
    void setInstanceDescriptorEnable(bool enable) override
    {
        m_product->m_instanceDescriptorSetEnable = enable;
    }
    void setMaterialDescriptorEnable(bool enable) override
    {
        m_product->m_materialDescriptorSetEnable = enable;
    }

    std::unique_ptr<RenderStateABC> build() override;
};

class ComputeStateBuilder;

/**
 * @brief state used by compute pipelines
 *
 */
class ComputeState : public GPUStateI
{
    friend ComputeStateBuilder;

  private:
    std::weak_ptr<Device> m_device;

    std::shared_ptr<Pipeline> m_pipeline;

    VkDescriptorPool m_descriptorPool;
    std::vector<VkDescriptorSet> m_descriptorSets;

  public:
    void recordBackBufferComputeCommands(const VkCommandBuffer &commandBuffer) override;

    void updateUniformBuffers(uint32_t imageIndex) override;

  public:
    [[nodiscard]] std::shared_ptr<Pipeline> getPipeline() const override
    {
        return m_pipeline;
    }
    [[nodiscard]] VkDescriptorPool getDescriptorPool() const override
    {
        return m_descriptorPool;
    }
};

class ComputeStateBuilder : public RenderStateBuilderI
{
  private:
    std::unique_ptr<ComputeState> m_product;

    std::weak_ptr<Device> m_device;

    std::vector<VkDescriptorPoolSize> m_poolSizes;
    uint32_t m_frameInFlightCount;

    void restart() override
    {
        m_product = std::unique_ptr<ComputeState>(new ComputeState);
    }

  public:
    ComputeStateBuilder()
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
    void setInstanceDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
    }
    void setInstanceDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
    }
    void setMaterialDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
    }
    void setMaterialDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
    }
    void setInstanceDescriptorEnable(bool enable) override
    {
    }
    void setMaterialDescriptorEnable(bool enable) override
    {
    }

    std::unique_ptr<RenderStateABC> build() override;
};
