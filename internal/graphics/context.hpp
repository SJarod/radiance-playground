#pragma once

#include <memory>
#include <vector>

#include <vulkan/vulkan.h>

#include "instance.hpp"

#define PFN_DECLARE_VK(funcName) PFN_##funcName funcName

class Device;
class ContextBuilder;

class Context
{
    friend ContextBuilder;

  private:
    std::vector<const char *> m_layers;
    std::vector<const char *> m_instanceExtensions;

    std::unique_ptr<Instance> m_instance;

    Context() = default;

  public:
  public:
    [[nodiscard]] inline int getLayerCount() const
    {
        return m_layers.size();
    }
    [[nodiscard]] inline const char *const *getLayers() const
    {
        return m_layers.data();
    }
    [[nodiscard]] inline int getInstanceExtensionCount() const
    {
        return m_instanceExtensions.size();
    }
    [[nodiscard]] inline const char *const *getInstanceExtensions() const
    {
        return m_instanceExtensions.data();
    }

    [[nodiscard]] inline VkInstance getInstanceHandle() const
    {
        return m_instance->getHandle();
    }

    [[nodiscard]] std::vector<VkPhysicalDevice> getAvailablePhysicalDevices() const;
};

class ContextBuilder
{
  private:
    std::unique_ptr<Context> m_product;

    std::vector<const char *> m_layers;
    std::vector<const char *> m_instanceExtensions;

    void restart()
    {
        m_product = std::unique_ptr<Context>(new Context);
    }

  public:
    ContextBuilder()
    {
        restart();
    }

    void addLayer(const char *layer)
    {
        m_layers.push_back(layer);
        m_product->m_layers.push_back(layer);
    }
    void addInstanceExtension(const char *extension)
    {
        m_instanceExtensions.push_back(extension);
        m_product->m_instanceExtensions.push_back(extension);
    }

    std::unique_ptr<Context> build();
};