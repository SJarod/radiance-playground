#pragma once

#include <cassert>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include <vk_mem_alloc.h>

#include "surface.hpp"

class Context;
class DeviceBuilder;

class Device
{
    friend DeviceBuilder;

  private:
    PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT = nullptr;

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
    std::optional<uint32_t> m_computeFamilyIndex;

    VkQueue m_graphicsQueue;
    VkQueue m_presentQueue;
    VkQueue m_computeQueue;

    VkCommandPool m_commandPool;
    VkCommandPool m_commandPoolTransient;

    /**
     * @brief total number of allocated buffer
     *
     */
    int m_bufferCount = 0;
    std::set<std::string> m_bufferNames;
    /**
     * @brief total number of allocated image
     *
     */
    int m_imageCount = 0;
    std::set<std::string> m_imageNames;
    VmaAllocator m_allocator;

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

    void addDebugObjectName(VkDebugUtilsObjectNameInfoEXT nameInfo);

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
    [[nodiscard]] inline const VkQueue &getComputeQueue() const
    {
        return m_computeQueue;
    }

    [[nodiscard]] inline bool isDiscrete() const
    {
        return m_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    }

    [[nodiscard]] inline const char *getDeviceName() const
    {
        return m_props.deviceName;
    }

    [[nodiscard]] inline const VmaAllocator &getAllocator() const
    {
        return m_allocator;
    }

    [[nodiscard]] const int getBufferCount() const
    {
        return m_bufferCount;
    }
    [[nodiscard]] const int getImageCount() const
    {
        return m_imageCount;
    }

  public:
    void addBufferCount(int n)
    {
        m_bufferCount += n;
    }
    void trackBufferName(std::string name)
    {
        m_bufferNames.insert(name);
    }
    void untrackBufferName(std::string name)
    {
        assert(std::find(m_bufferNames.begin(), m_bufferNames.end(), name) != m_bufferNames.end());
        m_bufferNames.erase(name);
    }

    void addImageCount(int n)
    {
        m_imageCount += n;
    }
    void trackImageName(std::string name)
    {
        m_imageNames.insert(name);
    }
    void untrackImageName(std::string name)
    {
        assert(std::find(m_imageNames.begin(), m_imageNames.end(), name) != m_imageNames.end());
        m_imageNames.erase(name);
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

    void setPhysicalDevice(VkPhysicalDevice a);

    void setSurface(const Surface *surface)
    {
        m_product->m_surface = surface;
        m_product->m_presentFamilyIndex = m_product->findPresentQueueFamilyIndex();
    }

    std::unique_ptr<Device> build();
};