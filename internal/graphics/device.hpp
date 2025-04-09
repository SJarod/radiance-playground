#pragma once

#include <cassert>
#include <memory>
#include <optional>
#include <vector>

#include <vulkan/vulkan.h>

#include "surface.hpp"

class Context;
class DeviceBuilder;

class Device
{
    friend DeviceBuilder;

  private:
    std::weak_ptr<Context> m_cx;

    std::vector<const char *> m_deviceExtensions;

    const Surface *m_surface = nullptr;

    // physical device
    VkPhysicalDevice m_physicalHandle;
    VkPhysicalDeviceFeatures2 m_features;
    VkPhysicalDeviceMultiviewFeatures m_multiviewFeature;
    VkPhysicalDeviceBufferDeviceAddressFeatures m_bufferDeviceAddressFeature;
    VkPhysicalDeviceUniformBufferStandardLayoutFeatures m_uniformBuffersStandardLayoutFeature;
    VkPhysicalDeviceProperties m_props;

    // logical device
    VkDevice m_handle;

    std::optional<uint32_t> m_graphicsFamilyIndex;
    std::optional<uint32_t> m_presentFamilyIndex;

    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;

    VkCommandPool m_commandPool;
    VkCommandPool m_commandPoolTransient;

    Device() = default;

  public:
    ~Device();

    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;
    Device(Device &&) = delete;
    Device &operator=(Device &&) = delete;

    std::optional<uint32_t> findQueueFamilyIndex(const VkQueueFlags &capabilities) const;
    std::optional<uint32_t> findPresentQueueFamilyIndex() const;

    std::optional<uint32_t> findMemoryTypeIndex(VkMemoryRequirements requirements,
                                                VkMemoryPropertyFlags properties) const;

    VkCommandBuffer cmdBeginOneTimeSubmit() const;
    void cmdEndOneTimeSubmit(VkCommandBuffer commandBuffer) const;

  public:
    [[nodiscard]] std::vector<VkQueueFamilyProperties> getQueueFamilyProperties() const;

    [[nodiscard]] inline int getDeviceExtensionCount() const
    {
        return m_deviceExtensions.size();
    }
    [[nodiscard]] inline const char *const *getDeviceExtensions() const
    {
        return m_deviceExtensions.data();
    }
    [[nodiscard]] inline const VkPhysicalDeviceFeatures2 &getPhysicalDeviceFeatures2()
    {
        return m_features;
    }
    [[nodiscard]] inline const VkPhysicalDeviceProperties &getPhysicalDeviceProperties()
    {
        return m_props;
    }

    [[nodiscard]] inline const VkPhysicalDevice &getPhysicalHandle() const
    {
        return m_physicalHandle;
    }
    [[nodiscard]] inline const VkDevice &getHandle() const
    {
        return m_handle;
    }

    [[nodiscard]] inline const VkCommandPool &getCommandPool() const
    {
        return m_commandPool;
    }

    [[nodiscard]] inline const VkSurfaceKHR getSurfaceHandle() const
    {
        assert(m_surface);
        return m_surface->getHandle();
    }

    [[nodiscard]] inline const std::optional<uint32_t> &getGraphicsFamilyIndex() const
    {
        return m_graphicsFamilyIndex;
    }
    [[nodiscard]] inline const std::optional<uint32_t> &getPresentFamilyIndex() const
    {
        return m_presentFamilyIndex;
    }

    [[nodiscard]] inline const VkQueue &getGraphicsQueue() const
    {
        return m_graphicsQueue;
    }

    [[nodiscard]] inline const VkQueue &getPresentQueue() const
    {
        return m_presentQueue;
    }
};

class DeviceBuilder
{
  private:
    std::unique_ptr<Device> m_product;

    std::weak_ptr<Context> m_cx;

    std::vector<const char *> m_deviceExtensions;

    void restart()
    {
        m_product = std::unique_ptr<Device>(new Device);
    }

  public:
    DeviceBuilder()
    {
        restart();
    }

    void setContext(std::weak_ptr<Context> context)
    {
        m_product->m_cx = context;
        m_cx = context;
    }

    void addDeviceExtension(const char *extension)
    {
        m_deviceExtensions.push_back(extension);
        m_product->m_deviceExtensions.push_back(extension);
    }

    void setPhysicalDevice(VkPhysicalDevice a)
    {
        m_product->m_physicalHandle = a;

        m_product->m_multiviewFeature = VkPhysicalDeviceMultiviewFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
            .multiview = true,
            .multiviewGeometryShader = false,
            .multiviewTessellationShader = false
        };

        m_product->m_bufferDeviceAddressFeature = VkPhysicalDeviceBufferDeviceAddressFeatures{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
            .pNext = &m_product->m_multiviewFeature
        };

        m_product->m_features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &m_product->m_bufferDeviceAddressFeature,
        };

        vkGetPhysicalDeviceFeatures2(a, &m_product->m_features);
        vkGetPhysicalDeviceProperties(a, &m_product->m_props);

        m_product->m_graphicsFamilyIndex = m_product->findQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
    }

    void setSurface(const Surface *surface)
    {
        m_product->m_surface = surface;
        m_product->m_presentFamilyIndex = m_product->findPresentQueueFamilyIndex();
    }

    std::unique_ptr<Device> build();
};