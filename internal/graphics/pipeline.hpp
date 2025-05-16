#pragma once

#include <string>
#include <vector>

#include <vulkan/vulkan.h>

class Device;
class RenderPass;
class BasePipelineBuilder;
enum class PipelineTypeE
{
    GRAPHICS = 0,
    COMPUTE = 1,
    COUNT = 2,
};
template <PipelineTypeE TType> class PipelineBuilder;
class UniformDescriptor;

class Pipeline
{
    friend BasePipelineBuilder;
    friend PipelineBuilder<PipelineTypeE::GRAPHICS>;
    friend PipelineBuilder<PipelineTypeE::COMPUTE>;

  private:
    std::weak_ptr<Device> m_device;

    std::vector<VkDescriptorSetLayout> m_descriptorSetLayouts;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_handle;

    PipelineTypeE m_type;

    Pipeline() = default;

  public:
    ~Pipeline();

    Pipeline(const Pipeline &) = delete;
    Pipeline &operator=(const Pipeline &) = delete;
    Pipeline(Pipeline &&) = delete;
    Pipeline &operator=(Pipeline &&) = delete;

    /**
     * @brief bind the pipeline whether it is a graphics or a compute pipeline
     *
     * @param commandBuffer
     * @param extent not used if it is a graphics pipeline
     */
    void recordBind(const VkCommandBuffer &commandBuffer, VkRect2D extent);

  public:
    [[nodiscard]] const VkPipelineLayout &getPipelineLayout() const
    {
        return m_pipelineLayout;
    }
    [[nodiscard]] const std::vector<VkDescriptorSetLayout> &getDescriptorSetLayouts() const
    {
        return m_descriptorSetLayouts;
    }

    [[nodiscard]] const std::optional<VkDescriptorSetLayout> getDescriptorSetLayoutAtIndex(uint32_t index = 0u) const
    {
        if (index >= m_descriptorSetLayouts.size())
            return std::nullopt;

        return m_descriptorSetLayouts[index];
    }
};

class BasePipelineBuilder
{
  protected:
    std::unique_ptr<Pipeline> m_product;

    std::weak_ptr<Device> m_device;

    // shaders
    std::vector<VkShaderModule> m_modules;
    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStageCreateInfos;

    // descriptor set layout
    std::vector<VkPushConstantRange> m_pushConstantRanges;

    std::vector<std::shared_ptr<UniformDescriptor>> m_uniformDescriptorPacks;

    // TODO : move to PipelineBuilder<PipelineTypeE::GRAPHICS>
    const RenderPass *m_renderPass;

    virtual void restart();

    virtual bool createPipelineLayout();

  public:
    virtual ~BasePipelineBuilder() = default;

    BasePipelineBuilder() = default;
    BasePipelineBuilder(const BasePipelineBuilder &) = delete;
    BasePipelineBuilder &operator=(const BasePipelineBuilder &) = delete;
    BasePipelineBuilder(BasePipelineBuilder &&) = delete;
    BasePipelineBuilder &operator=(BasePipelineBuilder &&) = delete;

    void setDevice(std::weak_ptr<Device> device)
    {
        this->m_device = device;
        m_product->m_device = device;
    }

    void addUniformDescriptorPack(std::shared_ptr<UniformDescriptor> desc)
    {
        m_uniformDescriptorPacks.push_back(desc);
    }
    // TODO : move to PipelineBuilder<PipelineTypeE::GRAPHICS>
    void setRenderPass(const RenderPass *a)
    {
        m_renderPass = a;
    }
};

template <PipelineTypeE TType> class PipelineBuilder : public BasePipelineBuilder
{
};

template <> class PipelineBuilder<PipelineTypeE::GRAPHICS> : public BasePipelineBuilder
{
  private:
    // dynamic states
    std::vector<VkDynamicState> m_dynamicStates;

    // draw mode
    VkPrimitiveTopology m_topology;
    bool m_bPrimitiveRestartEnable;

    VkExtent2D m_extent;

    // rasterizer
    VkBool32 m_depthClampEnable;
    VkBool32 m_rasterizerDiscardEnable;
    VkPolygonMode m_polygonMode;
    VkCullModeFlags m_cullMode;
    VkFrontFace m_frontFace;
    VkBool32 m_depthBiasEnable;
    float m_depthBiasConstantFactor;
    float m_depthBiasClamp;
    float m_depthBiasSlopeFactor;
    float m_lineWidth;

    // multisampling
    VkSampleCountFlagBits m_rasterizationSamples;
    VkBool32 m_sampleShadingEnable;
    float m_minSampleShading;
    const VkSampleMask *m_pSampleMask;
    VkBool32 m_alphaToCoverageEnable;
    VkBool32 m_alphaToOneEnable;

    // depth test
    VkBool32 m_depthTestEnable;
    VkBool32 m_depthWriteEnable;
    VkCompareOp m_depthCompareOp;
    VkBool32 m_depthBoundsTestEnable;
    VkBool32 m_stencilTestEnable;
    VkStencilOpState m_front;
    VkStencilOpState m_back;
    float m_minDepthBounds;
    float m_maxDepthBounds;

    // color blending
    VkBool32 m_blendEnable;
    VkBlendFactor m_srcColorBlendFactor;
    VkBlendFactor m_dstColorBlendFactor;
    VkBlendOp m_colorBlendOp;
    VkBlendFactor m_srcAlphaBlendFactor;
    VkBlendFactor m_dstAlphaBlendFactor;
    VkBlendOp m_alphaBlendOp;
    VkColorComponentFlags m_colorWriteMask;

    VkBool32 m_logicOpEnable;
    VkLogicOp m_logicOp;
    float m_blendConstants[4];

    void restart() override;

  public:
    PipelineBuilder()
    {
        restart();
    }

    void addVertexShaderStage(const char *shaderName, const char *entryPoint = "main");
    void addFragmentShaderStage(const char *shaderName, const char *entryPoint = "main");
    void addDynamicState(VkDynamicState state);
    void addPushConstantRange(VkPushConstantRange pushConstantRange);
    void setDrawTopology(VkPrimitiveTopology topology, bool bPrimitiveRestartEnable = false);
    void setExtent(VkExtent2D extent);
    void setDepthClampEnable(VkBool32 a)
    {
        m_depthClampEnable = a;
    }
    void setRasterizerDiscardEnable(VkBool32 a)
    {
        m_rasterizerDiscardEnable = a;
    }
    void setPolygonMode(VkPolygonMode a)
    {
        m_polygonMode = a;
    }
    void setCullMode(VkCullModeFlags a)
    {
        m_cullMode = a;
    }
    void setFrontFace(VkFrontFace a)
    {
        m_frontFace = a;
    }
    void setDepthBiasEnable(VkBool32 a)
    {
        m_depthBiasEnable = a;
    }
    void setDepthBiasConstantFactor(float a)
    {
        m_depthBiasConstantFactor = a;
    }
    void setDepthBiasClamp(float a)
    {
        m_depthBiasClamp = a;
    }
    void setDepthBiasSlopeFactor(float a)
    {
        m_depthBiasSlopeFactor = a;
    }
    void setLineWidth(float a)
    {
        m_lineWidth = a;
    }
    void setRasterizationSamples(VkSampleCountFlagBits a)
    {
        m_rasterizationSamples = a;
    }
    void setSampleShadingEnable(VkBool32 a)
    {
        m_sampleShadingEnable = a;
    }
    void setMinSampleShading(float a)
    {
        m_minSampleShading = a;
    }
    void setPSampleMask(const VkSampleMask *a)
    {
        m_pSampleMask = a;
    }
    void setAlphaToCoverageEnable(VkBool32 a)
    {
        m_alphaToCoverageEnable = a;
    }
    void setAlphaToOneEnable(VkBool32 a)
    {
        m_alphaToOneEnable = a;
    }
    void setDepthTestEnable(VkBool32 a)
    {
        m_depthTestEnable = a;
    }
    void setDepthWriteEnable(VkBool32 a)
    {
        m_depthWriteEnable = a;
    }
    void setDepthCompareOp(VkCompareOp a)
    {
        m_depthCompareOp = a;
    }
    void setDepthBoundsTestEnable(VkBool32 a)
    {
        m_depthBoundsTestEnable = a;
    }
    void setStencilTestEnable(VkBool32 a)
    {
        m_stencilTestEnable = a;
    }
    void setFront(VkStencilOpState a)
    {
        m_front = a;
    }
    void setBack(VkStencilOpState a)
    {
        m_back = a;
    }
    void setMinDepthBounds(float a)
    {
        m_minDepthBounds = a;
    }
    void setMaxDepthBounds(float a)
    {
        m_maxDepthBounds = a;
    }
    void setBlendEnable(VkBool32 a)
    {
        m_blendEnable = a;
    }
    void setSrcColorBlendFactor(VkBlendFactor a)
    {
        m_srcColorBlendFactor = a;
    }
    void setDstColorBlendFactor(VkBlendFactor a)
    {
        m_dstColorBlendFactor = a;
    }
    void setColorBlendOp(VkBlendOp a)
    {
        m_colorBlendOp = a;
    }
    void setSrcAlphaBlendFactor(VkBlendFactor a)
    {
        m_srcAlphaBlendFactor = a;
    }
    void setDstAlphaBlendFactor(VkBlendFactor a)
    {
        m_dstAlphaBlendFactor = a;
    }
    void setAlphaBlendOp(VkBlendOp a)
    {
        m_alphaBlendOp = a;
    }
    void setColorWriteMask(VkColorComponentFlags a)
    {
        m_colorWriteMask = a;
    }
    void setLogicOpEnable(VkBool32 a)
    {
        m_logicOpEnable = a;
    }
    void setLogicOp(VkLogicOp a)
    {
        m_logicOp = a;
    }
    void setBlendConstants(float a, float b, float c, float d)
    {
        m_blendConstants[0] = a;
        m_blendConstants[1] = b;
        m_blendConstants[2] = c;
        m_blendConstants[3] = d;
    }

    std::unique_ptr<Pipeline> build();
};

template <> class PipelineBuilder<PipelineTypeE::COMPUTE> : public BasePipelineBuilder
{
  private:
  public:
    PipelineBuilder()
    {
        restart();
    }

    void addComputeShaderStage(const char *shaderName, const char *entryPoint = "main");

    std::unique_ptr<Pipeline> build();
};

template <PipelineTypeE TType> class PipelineDirector
{
};

template <> class PipelineDirector<PipelineTypeE::GRAPHICS>
{
  public:
    void configureColorDepthRasterizerBuilder(PipelineBuilder<PipelineTypeE::GRAPHICS> &builder);
};

template <> class PipelineDirector<PipelineTypeE::COMPUTE>
{
  public:
    void configureComputeBuilder(PipelineBuilder<PipelineTypeE::COMPUTE> &builder);
};