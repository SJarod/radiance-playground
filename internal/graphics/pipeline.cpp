#include <array>
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

#include "device.hpp"
#include "render_pass.hpp"

#include "engine/uniform.hpp"
#include "engine/vertex.hpp"

#include "pipeline.hpp"

VkShaderModule create_shader_module(VkDevice device, const std::vector<char> &code)
{
    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.size(),
        .pCode = reinterpret_cast<const uint32_t *>(code.data()),
    };

    VkShaderModule module;
    VkResult res = vkCreateShaderModule(device, &createInfo, nullptr, &module);
    if (res != VK_SUCCESS)
        std::cerr << "Failed to create shader module : " << res << std::endl;

    return module;
}
void destroy_shader_module(VkDevice device, VkShaderModule module)
{
    vkDestroyShaderModule(device, module, nullptr);
}

bool read_binary_file(const std::string &filename, std::vector<char> &out)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "Failed to open file : " << filename << std::endl;
        return false;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    out.resize(fileSize);

    file.seekg(0);
    file.read(out.data(), fileSize);

    file.close();
    return true;
}

Pipeline::~Pipeline()
{
    if (!m_device.lock())
        return;

    auto deviceHandle = m_device.lock()->getHandle();

    vkDestroyPipelineLayout(deviceHandle, m_pipelineLayout, nullptr);

    for (uint32_t i = 0u; i < m_descriptorSetLayouts.size(); i++)
    {
        vkDestroyDescriptorSetLayout(deviceHandle, m_descriptorSetLayouts[i], nullptr);
    }

    vkDestroyPipeline(deviceHandle, m_handle, nullptr);
}

void BasePipelineBuilder::restart()
{
    m_modules.clear();
    m_shaderStageCreateInfos.clear();

    m_pushConstantRanges.clear();

    m_product = std::unique_ptr<Pipeline>(new Pipeline);
}

bool BasePipelineBuilder::createPipelineLayout()
{
    const VkDevice &deviceHandle = m_device.lock()->getHandle();

    for (uint32_t i = 0u; i < m_uniformDescriptorPacks.size(); i++)
    {
        std::vector<VkDescriptorSetLayoutBinding> layoutBindings = m_uniformDescriptorPacks[i]->getSetLayoutBindings();
        VkDescriptorSetLayoutCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = static_cast<uint32_t>(layoutBindings.size()),
            .pBindings = layoutBindings.data(),
        };
        VkDescriptorSetLayout descriptorSetLayout;
        VkResult res = vkCreateDescriptorSetLayout(deviceHandle, &createInfo, nullptr, &descriptorSetLayout);
        if (res != VK_SUCCESS)
        {
            std::cerr << "Failed to create descriptor set layout : " << res << std::endl;
            return false;
        }

        m_product->m_descriptorSetLayouts.push_back(descriptorSetLayout);
    }

    std::vector<VkDescriptorSetLayout> setLayouts = m_product->m_descriptorSetLayouts;
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = static_cast<uint32_t>(setLayouts.size()),
        .pSetLayouts = setLayouts.data(),
        .pushConstantRangeCount = static_cast<uint32_t>(m_pushConstantRanges.size()),
        .pPushConstantRanges = m_pushConstantRanges.data(),
    };
    VkResult res =
        vkCreatePipelineLayout(deviceHandle, &pipelineLayoutCreateInfo, nullptr, &m_product->m_pipelineLayout);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create pipeline layout : " << res << std::endl;
        return false;
    }

    return true;
}

void PipelineBuilder<PipelineType::GRAPHICS>::restart()
{
    BasePipelineBuilder::restart();
    m_dynamicStates.clear();
}

void PipelineBuilder<PipelineType::GRAPHICS>::addVertexShaderStage(const char *shaderName, const char *entryPoint)
{
    std::vector<char> shader;
    if (!read_binary_file("shaders/" + std::string(shaderName) + ".vert.spv", shader))
        return;

    m_modules.emplace_back(create_shader_module(m_device.lock()->getHandle(), shader));

    m_shaderStageCreateInfos.emplace_back(VkPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = m_modules.back(),
        .pName = entryPoint,
    });
}

void PipelineBuilder<PipelineType::GRAPHICS>::addFragmentShaderStage(const char *shaderName, const char *entryPoint)
{
    std::vector<char> shader;
    if (!read_binary_file("shaders/" + std::string(shaderName) + ".frag.spv", shader))
        return;

    m_modules.emplace_back(create_shader_module(m_device.lock()->getHandle(), shader));

    m_shaderStageCreateInfos.emplace_back(VkPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = m_modules.back(),
        .pName = entryPoint,
    });
}

void PipelineBuilder<PipelineType::COMPUTE>::addComputeShaderStage(const char *shaderName, const char *entryPoint)
{
    assert(m_shaderStageCreateInfos.empty());

    std::vector<char> shader;
    if (!read_binary_file("shaders/" + std::string(shaderName) + ".comp.spv", shader))
        return;

    m_modules.emplace_back(create_shader_module(m_device.lock()->getHandle(), shader));

    m_shaderStageCreateInfos.emplace_back(VkPipelineShaderStageCreateInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = m_modules.back(),
        .pName = entryPoint,
    });
}
void PipelineBuilder<PipelineType::GRAPHICS>::addDynamicState(VkDynamicState state)
{
    m_dynamicStates.emplace_back(state);
}

void PipelineBuilder<PipelineType::GRAPHICS>::addPushConstantRange(VkPushConstantRange pushConstantRange)
{
    m_pushConstantRanges.push_back(pushConstantRange);
}

void PipelineBuilder<PipelineType::GRAPHICS>::setDrawTopology(VkPrimitiveTopology topology,
                                                              bool bPrimitiveRestartEnable)
{
    m_topology = topology;
    m_bPrimitiveRestartEnable = bPrimitiveRestartEnable;
}

void PipelineBuilder<PipelineType::GRAPHICS>::setExtent(VkExtent2D extent)
{
    m_extent = extent;
}

std::unique_ptr<Pipeline> PipelineBuilder<PipelineType::GRAPHICS>::build()
{
    assert(m_device.lock());
    assert(m_renderPass);

    m_product->m_type = PipelineType::GRAPHICS;

    const VkDevice &deviceHandle = m_device.lock()->getHandle();

    VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = static_cast<uint32_t>(m_dynamicStates.size()),
        .pDynamicStates = m_dynamicStates.data(),
    };

    // vertex buffer (enabling the binding for our Vertex structure)
    auto binding = Vertex::get_vertex_input_binding_description();
    auto attribs = Vertex::get_vertex_input_attribute_description();
    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = static_cast<uint32_t>(attribs.size()),
        .pVertexAttributeDescriptions = attribs.data(),
    };

    // draw mode
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = m_topology,
        .primitiveRestartEnable = static_cast<VkBool32>(m_bPrimitiveRestartEnable),
    };

    // viewport
    VkViewport viewport = {
        .x = 0.f,
        .y = 0.f,
        .width = static_cast<float>(m_extent.width),
        .height = static_cast<float>(m_extent.height),
        .minDepth = 0.f,
        .maxDepth = 1.f,
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = m_extent,
    };

    VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    // rasterizer
    VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = m_depthClampEnable,
        .rasterizerDiscardEnable = m_rasterizerDiscardEnable,
        .polygonMode = m_polygonMode,
        .cullMode = m_cullMode,
        .frontFace = m_frontFace,
        .depthBiasEnable = m_depthBiasEnable,
        .depthBiasConstantFactor = m_depthBiasConstantFactor,
        .depthBiasClamp = m_depthBiasClamp,
        .depthBiasSlopeFactor = m_depthBiasSlopeFactor,
        .lineWidth = m_lineWidth,
    };

    // multisampling, anti-aliasing
    VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = m_rasterizationSamples,
        .sampleShadingEnable = m_sampleShadingEnable,
        .minSampleShading = m_minSampleShading,
        .pSampleMask = m_pSampleMask,
        .alphaToCoverageEnable = m_alphaToCoverageEnable,
        .alphaToOneEnable = m_alphaToOneEnable,
    };

    VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = m_depthTestEnable,
        .depthWriteEnable = m_depthWriteEnable,
        .depthCompareOp = m_depthCompareOp,
        .depthBoundsTestEnable = m_depthBoundsTestEnable,
        .stencilTestEnable = m_stencilTestEnable,
        .front = m_front,
        .back = m_back,
        .minDepthBounds = m_minDepthBounds,
        .maxDepthBounds = m_maxDepthBounds,
    };

    // color blending
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = m_srcColorBlendFactor,
        .dstColorBlendFactor = m_dstColorBlendFactor,
        .colorBlendOp = m_colorBlendOp,
        .srcAlphaBlendFactor = m_srcAlphaBlendFactor,
        .dstAlphaBlendFactor = m_dstAlphaBlendFactor,
        .alphaBlendOp = m_alphaBlendOp,
        .colorWriteMask = m_colorWriteMask,
    };

    VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = m_logicOpEnable,
        .logicOp = m_logicOp,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
        .blendConstants =
            {
                m_blendConstants[0],
                m_blendConstants[1],
                m_blendConstants[2],
                m_blendConstants[3],
            },
    };

    // descriptor set layout
    if (!createPipelineLayout())
        return nullptr;

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        // shader stage
        .stageCount = static_cast<uint32_t>(m_shaderStageCreateInfos.size()),
        .pStages = m_shaderStageCreateInfos.data(),
        // fixed function stage
        .pVertexInputState = &vertexInputCreateInfo,
        .pInputAssemblyState = &inputAssemblyCreateInfo,
        .pViewportState = &viewportStateCreateInfo,
        .pRasterizationState = &rasterizerCreateInfo,
        .pMultisampleState = &multisamplingCreateInfo,
        .pDepthStencilState = &depthStencilCreateInfo,
        .pColorBlendState = &colorBlendCreateInfo,
        .pDynamicState = &dynamicStateCreateInfo,
        // pipeline layout
        .layout = m_product->m_pipelineLayout,
        // render pass
        .renderPass = m_renderPass->getHandle(),
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    VkResult res =
        vkCreateGraphicsPipelines(deviceHandle, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &m_product->m_handle);

    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create graphics pipeline : " << res << std::endl;
        return nullptr;
    }

    for (int i = 0; i < m_modules.size(); ++i)
    {
        destroy_shader_module(m_device.lock()->getHandle(), m_modules[i]);
    }

    return std::move(m_product);
}

void PipelineDirector<PipelineType::GRAPHICS>::configureColorDepthRasterizerBuilder(
    PipelineBuilder<PipelineType::GRAPHICS> &builder)
{
    builder.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
    builder.addDynamicState(VK_DYNAMIC_STATE_SCISSOR);

    builder.setDrawTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE);

    builder.setDepthClampEnable(VK_FALSE);
    builder.setRasterizerDiscardEnable(VK_FALSE);
    builder.setPolygonMode(VK_POLYGON_MODE_FILL);
    builder.setCullMode(VK_CULL_MODE_BACK_BIT);
    builder.setFrontFace(VK_FRONT_FACE_COUNTER_CLOCKWISE);
    builder.setDepthBiasEnable(VK_FALSE);
    builder.setDepthBiasConstantFactor(0.f);
    builder.setDepthBiasClamp(0.f);
    builder.setDepthBiasSlopeFactor(0.f);
    builder.setLineWidth(1.f);

    builder.setRasterizationSamples(VK_SAMPLE_COUNT_1_BIT);
    builder.setSampleShadingEnable(VK_FALSE);
    builder.setMinSampleShading(1.f);
    builder.setPSampleMask(nullptr);
    builder.setAlphaToCoverageEnable(VK_FALSE);
    builder.setAlphaToOneEnable(VK_FALSE);

    builder.setDepthTestEnable(VK_TRUE);
    builder.setDepthWriteEnable(VK_TRUE);
    builder.setDepthCompareOp(VK_COMPARE_OP_LESS);
    builder.setDepthBoundsTestEnable(VK_FALSE);
    builder.setStencilTestEnable(VK_FALSE);
    builder.setFront({});
    builder.setBack({});
    builder.setMinDepthBounds(0.0f);
    builder.setMaxDepthBounds(1.0f);

    builder.setBlendEnable(VK_TRUE);
    builder.setSrcColorBlendFactor(VK_BLEND_FACTOR_SRC_ALPHA);
    builder.setDstColorBlendFactor(VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    builder.setColorBlendOp(VK_BLEND_OP_ADD);
    builder.setSrcAlphaBlendFactor(VK_BLEND_FACTOR_ONE);
    builder.setDstAlphaBlendFactor(VK_BLEND_FACTOR_ZERO);
    builder.setAlphaBlendOp(VK_BLEND_OP_ADD);
    builder.setColorWriteMask(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT);

    builder.setLogicOpEnable(VK_FALSE);
    builder.setLogicOp(VK_LOGIC_OP_COPY);
    builder.setBlendConstants(0.f, 0.f, 0.f, 0.f);
}

void PipelineDirector<PipelineType::COMPUTE>::configureComputeBuilder(PipelineBuilder<PipelineType::COMPUTE> &builder)
{
    // do nothing
}

std::unique_ptr<Pipeline> PipelineBuilder<PipelineType::COMPUTE>::build()
{
    assert(!m_device.expired());

    m_product->m_type = PipelineType::COMPUTE;

    auto deviceHandle = m_device.lock()->getHandle();

    if (!createPipelineLayout())
        return nullptr;

    VkComputePipelineCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = m_shaderStageCreateInfos[0],
        .layout = m_product->m_pipelineLayout,
    };
    vkCreateComputePipelines(deviceHandle, VK_NULL_HANDLE, 1, &createInfo, nullptr, &m_product->m_handle);

    for (int i = 0; i < m_modules.size(); ++i)
    {
        destroy_shader_module(m_device.lock()->getHandle(), m_modules[i]);
    }

    return std::move(m_product);
}

void Pipeline::recordBind(const VkCommandBuffer &commandBuffer, VkRect2D extent)
{
    switch (m_type)
    {
    case PipelineType::GRAPHICS: {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_handle);

        VkViewport viewport = {.x = 0.f,
                               .y = 0.f,
                               .width = static_cast<float>(extent.extent.width),
                               .height = static_cast<float>(extent.extent.height),
                               .minDepth = 0.f,
                               .maxDepth = 1.f};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &extent);
    }
    break;

    case PipelineType::COMPUTE: {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_handle);
    }
    break;

    default: {
        assert(false);
    }
    break;
    }
}
