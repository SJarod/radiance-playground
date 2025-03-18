#pragma once

#include <string>
#include <vector>

#include <vulkan/vulkan.h>

class Device;
class RenderPass;
class PipelineBuilder;
class UniformDescriptor;

class Pipeline
{
    friend PipelineBuilder;

  private:
    std::weak_ptr<Device> m_device;

    VkDescriptorSetLayout m_descriptorSetLayout;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_handle;

    VkExtent2D m_extent;

    Pipeline() = default;

  public:
    ~Pipeline();

    void recordBind(const VkCommandBuffer &commandBuffer);

  public:
    [[nodiscard]] const VkPipelineLayout &getPipelineLayout() const
    {
        return m_pipelineLayout;
    }
    [[nodiscard]] const VkDescriptorSetLayout &getDescriptorSetLayout() const
    {
        return m_descriptorSetLayout;
    }
};

class PipelineBuilder
{
  private:
    std::unique_ptr<Pipeline> m_product;

    std::weak_ptr<Device> m_device;

    // shaders
    std::vector<VkShaderModule> m_modules;
    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStageCreateInfos;

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

    // descriptor set layout
    std::vector<VkPushConstantRange> m_pushConstantRanges;

    std::shared_ptr<UniformDescriptor> m_uniformDescriptorPack;

    const RenderPass *m_renderPass;

    void restart();

  public:
    PipelineBuilder()
    {
        restart();
    }

    void setDevice(std::weak_ptr<Device> device)
    {
        this->m_device = device;
        m_product->m_device = device;
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
    void setUniformDescriptorPack(std::shared_ptr<UniformDescriptor> desc)
    {
        m_uniformDescriptorPack = desc;
    }
    void setRenderPass(const RenderPass *a)
    {
        m_renderPass = a;
    }

    std::unique_ptr<Pipeline> build();
};

class PipelineDirector
{
  public:
    void createColorDepthRasterizerBuilder(PipelineBuilder &build);
};
