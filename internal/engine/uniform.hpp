#pragma once

#include <glm/glm.hpp>

#include <vulkan/vulkan.h>

class UniformDescriptorBuilder;

class UniformDescriptor
{
    friend UniformDescriptorBuilder;

  private:
    std::vector<VkDescriptorSetLayoutBinding> m_setLayoutBindings;
    std::vector<VkWriteDescriptorSet> m_setWrites;

    UniformDescriptor() = default;

  public:
    [[nodiscard]] const std::vector<VkDescriptorSetLayoutBinding> &getSetLayoutBindings() const
    {
        return m_setLayoutBindings;
    }
    [[nodiscard]] const std::vector<VkWriteDescriptorSet> &getSetWrites() const
    {
        return m_setWrites;
    }
};

class UniformDescriptorBuilder
{
  private:
    std::unique_ptr<UniformDescriptor> m_product;

    void restart()
    {
        m_product = std::unique_ptr<UniformDescriptor>(new UniformDescriptor);
    }

  public:
    UniformDescriptorBuilder()
    {
        restart();
    }

    void addSetLayoutBinding(VkDescriptorSetLayoutBinding binding)
    {
        m_product->m_setLayoutBindings.push_back(binding);
    }
    void addSetWrites(VkWriteDescriptorSet write)
    {
        m_product->m_setWrites.push_back(write);
    }

    std::unique_ptr<UniformDescriptor> buildAndRestart()
    {
        auto result = std::move(m_product);
        restart();
        return result;
    }
};
