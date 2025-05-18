#pragma once

#include <functional>
#include <memory>
#include <string>
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

/**
 * @brief use this macro to easily convert a freshly created render state
 * it is necessary due to the obsolete usage of RenderStateABC in all the architecture
 * the RenderPhases use this class, the interface should be later updated
 * TODO : update the interface
 *
 */
#define RENDER_STATE_PTR(gpuStatePtr)                                                                                  \
    std::dynamic_pointer_cast<RenderStateABC>(static_cast<std::shared_ptr<GPUStateI>>(gpuStatePtr))

/**
 * @brief use this macro to easily convert a freshly created compute state
 *
 */
#define COMPUTE_STATE_PTR(gpuStatePtr)                                                                                 \
    std::dynamic_pointer_cast<ComputeState>(static_cast<std::shared_ptr<GPUStateI>>(gpuStatePtr))

/**
 * @brief used for updating descriptor sets on register
 *
 */
using DescriptorSetUpdatePred =
    std::function<void(const RenderPhase *parentPhase, const VkDescriptorSet &set, uint32_t backBufferIndex)>;
/**
 * @brief same as above but executed at each frame (between the render pass scope)
 *
 */
using DescriptorSetUpdatePredPerFrame = std::function<void(const RenderPhase *parentPhase, VkCommandBuffer cmd,
                                                           const VkDescriptorSet &set, uint32_t backBufferIndex)>;

/**
 * @brief state that can be taken into account by any phase to utilize the GPU.
 *
 */
class GPUStateI
{
  protected:
    GPUStateI() = default;

  public:
    virtual ~GPUStateI() = default;

    GPUStateI(const GPUStateI &) = delete;
    GPUStateI &operator=(const GPUStateI &) = delete;
    GPUStateI(GPUStateI &&) = delete;
    GPUStateI &operator=(GPUStateI &&) = delete;

    /**
     * @brief this function is specific for the compute state but it is needed in the highest class of polymorphisme due
     * to the renderer using GPUStateIs
     *
     * @param commandBuffer
     * @param backBufferIndex
     */
    virtual void recordBackBufferComputeCommands(const VkCommandBuffer &commandBuffer, uint32_t backBufferIndex) = 0;
    virtual void updateUniformBuffers(uint32_t backBufferIndex) = 0;

    /**
     * @brief used to take the user defined descriptor sets at each frames
     * it is used by the graphics states, hence the RenderPhase parameter
     *
     * @param parentPhase not used in the compute state because compute phases uses ComputePhase objects
     * @param backBufferIndex
     * @param pooledFramebufferIndexnot used in the compute state because compute phases uses ComputePhase objects
     */
    virtual void updateDescriptorSetsPerFrame(const RenderPhase *parentPhase, VkCommandBuffer cmd,
                                              uint32_t backBufferIndex, uint32_t pooledFramebufferIndex) = 0;

    /**
     * @brief same function as above but it is executed on the states registering
     *
     * @param parentPhase not used in the compute state because compute phases uses ComputePhase objects
     * @param backBufferIndex
     * @param pooledFramebufferIndexnot used in the compute state because compute phases uses ComputePhase objects
     */
    virtual void updateDescriptorSets(const RenderPhase *parentPhase, uint32_t backBufferIndex,
                                      uint32_t pooledFramebufferIndex) = 0;

  public:
    [[nodiscard]] virtual std::shared_ptr<Pipeline> getPipeline() const = 0;
    [[nodiscard]] virtual VkDescriptorPool getDescriptorPool() const = 0;
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
    std::vector<std::vector<VkDescriptorSet>> m_poolInstanceDescriptorSets;
    std::vector<std::vector<VkDescriptorSet>> m_materialDescriptorSetsPerSubObject;
    std::vector<std::vector<std::unique_ptr<Buffer>>> m_poolMVPUniformBuffers;
    std::vector<std::vector<void *>> m_poolMVPUniformBuffersMapped;
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

    RenderStateABC(const RenderStateABC &) = delete;
    RenderStateABC &operator=(const RenderStateABC &) = delete;
    RenderStateABC(RenderStateABC &&) = delete;
    RenderStateABC &operator=(RenderStateABC &&) = delete;

    virtual void updatePushConstants(const VkCommandBuffer &commandBuffer, uint32_t singleFrameRenderIndex,
                                     const CameraABC &camera, const std::vector<std::shared_ptr<Light>> &lights)
    {
    }

    virtual void updateUniformBuffers(uint32_t backBufferIndex, uint32_t singleFrameRenderIndex,
                                      uint32_t pooledFramebufferIndex, const CameraABC &camera,
                                      const std::vector<std::shared_ptr<Light>> &lights,
                                      const std::shared_ptr<ProbeGrid> &probeGrid, bool captureModeEnabled);
    virtual void updateDescriptorSetsPerFrame(const RenderPhase *parentPhase, VkCommandBuffer cmd,
                                              uint32_t backBufferIndex, uint32_t pooledFramebufferIndex) override;
    virtual void updateDescriptorSets(const RenderPhase *parentPhase, uint32_t backBufferIndex,
                                      uint32_t pooledFramebufferIndex) override;

    virtual void recordBackBufferDescriptorSetsCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex,
                                                        uint32_t backBufferIndex, uint32_t pooledFramebufferIndex);
    virtual void recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex) = 0;

    /**
     * @brief no implementation yet
     *
     * @param commandBuffer
     */
    void recordBackBufferComputeCommands(const VkCommandBuffer &commandBuffer, uint32_t backBufferIndex) override
    {
        assert(false);
    }
    /**
     * @brief no implementation yet
     *
     * @param backBufferIndex
     */
    void updateUniformBuffers(uint32_t backBufferIndex) override
    {
        assert(false);
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
    virtual void addPoolSize(VkDescriptorType poolSizeType, size_t size = 1) = 0;
    virtual void setFrameInFlightCount(uint32_t a) = 0;
    virtual void setTexture(std::weak_ptr<Texture> texture) = 0;

    virtual void setDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) = 0;
    virtual void setDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) = 0;

    virtual void setInstanceDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) = 0;
    virtual void setInstanceDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) = 0;

    virtual void setMaterialDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) = 0;
    virtual void setMaterialDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) = 0;

    virtual void setInstanceDescriptorEnable(bool enable) = 0;
    virtual void setMaterialDescriptorEnable(bool enable) = 0;
    virtual void setCaptureCount(uint32_t captureCount) = 0;

    virtual std::unique_ptr<GPUStateI> build() = 0;
};

class ModelRenderState : public RenderStateABC
{
    friend ModelRenderStateBuilder;

  private:
    std::weak_ptr<Model> m_model;

    bool m_pushViewPosition = true;

  public:
    static std::shared_ptr<Texture> s_defaultDiffuseTexture;

    void updatePushConstants(const VkCommandBuffer &commandBuffer, uint32_t singleFrameRenderIndex,
                             const CameraABC &camera, const std::vector<std::shared_ptr<Light>> &lights) override;
    void recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex) override;

    void updateUniformBuffers(uint32_t backBufferIndex, uint32_t singleFrameRenderIndex,
                              uint32_t pooledFramebufferIndex, const CameraABC &camera,
                              const std::vector<std::shared_ptr<Light>> &lights,
                              const std::shared_ptr<ProbeGrid> &probeGrid, bool captureModeEnabled) override;

  public:
    [[nodiscard]] uint32_t getSubObjectCount() const override;

    [[nodiscard]] const Model *getModel() const
    {
        assert(!m_model.expired());
        return m_model.lock().get();
    }
};

class ModelRenderStateBuilder : public RenderStateBuilderI
{
  private:
    std::unique_ptr<ModelRenderState> m_product;

    std::weak_ptr<Device> m_device;

    std::string m_modelName = "Unnamed";

    std::vector<VkDescriptorPoolSize> m_poolSizes;
    uint32_t m_frameInFlightCount;

    std::weak_ptr<Texture> m_texture;
    std::vector<std::weak_ptr<Texture>> m_environmentMaps;

    bool m_probeDescriptorEnable = true;
    bool m_lightDescriptorEnable = true;
    bool m_textureDescriptorEnable = true;
    bool m_mvpDescriptorEnable = true;
    uint32_t m_captureCount = 1u;

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
    void addPoolSize(VkDescriptorType poolSizeType, size_t size = 1) override;
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
    void setDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        assert(false);
    }
    void setDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        assert(false);
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
    void setCaptureCount(uint32_t captureCount) override
    {
        m_captureCount = captureCount;
    }

    void setModel(std::shared_ptr<Model> model);

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

    std::unique_ptr<GPUStateI> build() override;
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
    uint32_t m_captureCount = 1u;

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
    void addPoolSize(VkDescriptorType poolSizeType, size_t size = 1) override;
    void setFrameInFlightCount(uint32_t a) override
    {
        m_frameInFlightCount = a;
    }

    void setTexture(std::weak_ptr<Texture> texture) override
    {
        assert(false);
    }
    void setDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        assert(false);
    }
    void setDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        assert(false);
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
    void setCaptureCount(uint32_t captureCount) override
    {
    }

    std::unique_ptr<GPUStateI> build() override;
};

class SkyboxRenderState : public RenderStateABC
{
    friend SkyboxRenderStateBuilder;

  private:
    std::weak_ptr<Skybox> m_skybox;

  public:
    void updateUniformBuffers(uint32_t backBufferIndex, uint32_t singleFrameRenderIndex,
                              uint32_t pooledFramebufferIndex, const CameraABC &camera,
                              const std::vector<std::shared_ptr<Light>> &lights,
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

    uint32_t m_captureCount = 1u;

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
    void addPoolSize(VkDescriptorType poolSizeType, size_t size = 1) override;
    void setFrameInFlightCount(uint32_t a) override
    {
        m_frameInFlightCount = a;
    }
    void setTexture(std::weak_ptr<Texture> texture) override
    {
        m_texture = texture;
    }
    void setDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        assert(false);
    }
    void setDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        assert(false);
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
    void setCaptureCount(uint32_t captureCount) override
    {
        m_captureCount = captureCount;
    }
    void setSkybox(std::shared_ptr<Skybox> skybox)
    {
        m_product->m_skybox = skybox;
    }

    void setTextureDescriptorEnable(bool a)
    {
        m_textureDescriptorEnable = a;
    }

    std::unique_ptr<GPUStateI> build() override;
};

class EnvironmentCaptureRenderState : public RenderStateABC
{
    friend EnvironmentCaptureRenderStateBuilder;

  private:
    std::weak_ptr<Skybox> m_skybox;

  public:
    void updateUniformBuffers(uint32_t backBufferIndex, uint32_t singleFrameRenderIndex,
                              uint32_t pooledFramebufferIndex, const CameraABC &camera,
                              const std::vector<std::shared_ptr<Light>> &lights,
                              const std::shared_ptr<ProbeGrid> &probeGrid, bool captureModeEnabled) override;

    void recordBackBufferDrawObjectCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex) override;
    void recordBackBufferDescriptorSetsCommands(const VkCommandBuffer &commandBuffer, uint32_t subObjectIndex,
                                                uint32_t imageIndex, uint32_t pooledFramebufferIndex) override;

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
    uint32_t m_captureCount = 1u;

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
    void addPoolSize(VkDescriptorType poolSizeType, size_t size = 1) override;
    void setFrameInFlightCount(uint32_t a) override
    {
        m_frameInFlightCount = a;
    }
    void setTexture(std::weak_ptr<Texture> texture) override
    {
        m_texture = texture;
    }
    void setDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        assert(false);
    }
    void setDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        assert(false);
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
    void setCaptureCount(uint32_t captureCount) override
    {
        m_captureCount = captureCount;
    }
    void setSkybox(std::shared_ptr<Skybox> skybox)
    {
        m_product->m_skybox = skybox;
    }

    void setTextureDescriptorEnable(bool a)
    {
        m_textureDescriptorEnable = a;
    }

    std::unique_ptr<GPUStateI> build() override;
};

class ProbeGridRenderState : public RenderStateABC
{
    friend ProbeGridRenderStateBuilder;

  private:
    std::weak_ptr<ProbeGrid> m_grid;

    std::shared_ptr<Mesh> m_mesh;

  public:
    void updateUniformBuffers(uint32_t backBufferIndex, uint32_t singleFrameRenderIndex,
                              uint32_t pooledFramebufferIndex, const CameraABC &camera,
                              const std::vector<std::shared_ptr<Light>> &lights,
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
    uint32_t m_captureCount = 1u;

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
    void addPoolSize(VkDescriptorType poolSizeType, size_t size = 1) override;
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
    void setDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        assert(false);
    }
    void setDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        assert(false);
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
    void setCaptureCount(uint32_t captureCount) override
    {
    }
    void setInstanceDescriptorEnable(bool enable) override
    {
        m_product->m_instanceDescriptorSetEnable = enable;
    }
    void setMaterialDescriptorEnable(bool enable) override
    {
        m_product->m_materialDescriptorSetEnable = enable;
    }

    std::unique_ptr<GPUStateI> build() override;
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

    DescriptorSetUpdatePredPerFrame m_descriptorSetUpdatePredPerFrame = nullptr;
    DescriptorSetUpdatePred m_descriptorSetUpdatePred = nullptr;

    glm::ivec3 m_workGroup = glm::ivec3(0, 0, 0);

    ComputeState() = default;

  public:
    ~ComputeState();

    ComputeState(const ComputeState &) = delete;
    ComputeState &operator=(const ComputeState &) = delete;
    ComputeState(ComputeState &&) = delete;
    ComputeState &operator=(ComputeState &&) = delete;

    void recordBackBufferComputeCommands(const VkCommandBuffer &commandBuffer, uint32_t backBufferIndex) override;

    void updateUniformBuffers(uint32_t backBufferIndex) override;

    void updateDescriptorSetsPerFrame(const RenderPhase *parentPhase, VkCommandBuffer cmd, uint32_t backBufferIndex,
                                      uint32_t pooledFramebufferIndex = -1) override;

    void updateDescriptorSets(const RenderPhase *parentPhase, uint32_t backBufferIndex,
                              uint32_t pooledFramebufferIndex = -1) override;

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

class ComputeStateBuilder final : public RenderStateBuilderI
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
    void addPoolSize(VkDescriptorType poolSizeType, size_t size = 1) override;
    void setFrameInFlightCount(uint32_t a) override
    {
        m_frameInFlightCount = a;
    }
    void setTexture(std::weak_ptr<Texture> texture) override
    {
        assert(false);
    }
    void setDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        m_product->m_descriptorSetUpdatePredPerFrame = pred;
    }
    void setInstanceDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        assert(false);
    }
    void setDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        m_product->m_descriptorSetUpdatePred = pred;
    }
    void setInstanceDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        assert(false);
    }
    void setMaterialDescriptorSetUpdatePredPerFrame(DescriptorSetUpdatePredPerFrame pred) override
    {
        assert(false);
    }
    void setMaterialDescriptorSetUpdatePred(DescriptorSetUpdatePred pred) override
    {
        assert(false);
    }
    void setInstanceDescriptorEnable(bool enable) override
    {
        assert(false);
    }
    void setMaterialDescriptorEnable(bool enable) override
    {
        assert(false);
    }
    void setWorkGroup(glm::ivec3 workGroup)
    {
        m_product->m_workGroup = workGroup;
    }
    void setCaptureCount(uint32_t captureCount) override
    {
    }

    std::unique_ptr<GPUStateI> build() override;
};
