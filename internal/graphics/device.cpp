#include <iostream>
#include <set>
#include <vector>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "context.hpp"
#include "surface.hpp"

#include "device.hpp"

Device::~Device()
{
    std::cout << "Destroying device : " << getDeviceName() << std::endl;

    // ensure all the resources have been freed
    if (m_bufferCount != 0)
        std::for_each(std::next(m_bufferNames.begin()), m_bufferNames.end(),
                      [&](const std::string &element) { std::cout << element << std::endl; });
    if (m_imageCount != 0)
        std::for_each(std::next(m_imageNames.begin()), m_imageNames.end(),
                      [&](const std::string &element) { std::cout << element << std::endl; });
    assert(m_bufferCount == 0);
    assert(m_imageCount == 0);

    vmaDestroyAllocator(m_allocator);

    vkDestroyCommandPool(m_handle, m_commandPool, nullptr);
    vkDestroyCommandPool(m_handle, m_commandPoolTransient, nullptr);

    vkDestroyDevice(m_handle, nullptr);
}

std::optional<uint32_t> Device::findMemoryTypeIndex(VkMemoryRequirements requirements,
                                                    VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memProp;
    vkGetPhysicalDeviceMemoryProperties(m_physicalHandle, &memProp);

    for (uint32_t i = 0; i < memProp.memoryTypeCount; ++i)
    {
        bool rightType = requirements.memoryTypeBits & (1 << i);
        bool rightFlag = (memProp.memoryTypes[i].propertyFlags & properties) == properties;
        if (rightType && rightFlag)
            return std::optional<uint32_t>(i);
    }

    std::cerr << "Failed to find suitable memory type" << std::endl;
    return std::optional<uint32_t>();
}

std::vector<VkQueueFamilyProperties> Device::getQueueFamilyProperties() const
{
    uint32_t queueFamilyPropertiesCount;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalHandle, &queueFamilyPropertiesCount, nullptr);
    std::vector<VkQueueFamilyProperties> out(queueFamilyPropertiesCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalHandle, &queueFamilyPropertiesCount, out.data());
    return out;
}

std::optional<uint32_t> Device::findQueueFamilyIndex(const VkQueueFlags &capabilities) const
{
    auto props = getQueueFamilyProperties();
    for (uint32_t i = 0; i < props.size(); ++i)
    {
        if (props[i].queueFlags & capabilities)
            return std::optional<uint32_t>(i);
    }
    return std::optional<uint32_t>();
}
std::optional<uint32_t> Device::findPresentQueueFamilyIndex() const
{
    if (!m_surface)
        return std::optional<uint32_t>();

    auto props = getQueueFamilyProperties();
    for (uint32_t i = 0; i < props.size(); ++i)
    {
        VkBool32 supported;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalHandle, i, m_surface->getHandle(), &supported);
        if (supported)
            return std::optional<uint32_t>(i);
    }
    return std::optional<uint32_t>();
}

VkCommandBuffer Device::cmdBeginOneTimeSubmit() const
{
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_commandPoolTransient,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(m_handle, &allocInfo, &commandBuffer);
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    VkResult res = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (res != VK_SUCCESS)
        std::cerr << "Failed to begin one time submit command buffer : " << res << std::endl;

    return commandBuffer;
}
void Device::cmdEndOneTimeSubmit(VkCommandBuffer commandBuffer) const
{
    vkEndCommandBuffer(commandBuffer);
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };

    VkResult res = vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    if (res != VK_SUCCESS)
        std::cerr << "Failed to submit one time command buffer : " << res << std::endl;

    vkQueueWaitIdle(m_graphicsQueue);
    vkFreeCommandBuffers(m_handle, m_commandPoolTransient, 1, &commandBuffer);
}

void Device::addDebugObjectName(VkDebugUtilsObjectNameInfoEXT nameInfo)
{
    if (!vkSetDebugUtilsObjectNameEXT)
        vkSetDebugUtilsObjectNameEXT = PFN_vkSetDebugUtilsObjectNameEXT(
            vkGetInstanceProcAddr(m_cx.lock()->getInstanceHandle(), "vkSetDebugUtilsObjectNameEXT"));
    vkSetDebugUtilsObjectNameEXT(m_handle, &nameInfo);
}

void DeviceBuilder::setPhysicalDevice(VkPhysicalDevice a)
{
    m_product->m_physicalHandle = a;

    m_product->m_multiviewFeature =
        VkPhysicalDeviceMultiviewFeatures{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
                                          .multiview = true,
                                          .multiviewGeometryShader = false,
                                          .multiviewTessellationShader = false};

    m_product->m_bufferDeviceAddressFeature = VkPhysicalDeviceBufferDeviceAddressFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .pNext = &m_product->m_multiviewFeature};

    m_product->m_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &m_product->m_bufferDeviceAddressFeature,
    };

    vkGetPhysicalDeviceFeatures2(a, &m_product->m_features);
    vkGetPhysicalDeviceProperties(a, &m_product->m_props);

    m_product->m_graphicsFamilyIndex = m_product->findQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
    m_product->m_computeFamilyIndex = m_product->findQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
}

std::unique_ptr<Device> DeviceBuilder::build()
{
    assert(m_product->m_physicalHandle);
    assert(m_cx.lock());

    std::set<uint32_t> uniqueQueueFamilies;

    if (m_product->m_graphicsFamilyIndex.has_value())
        uniqueQueueFamilies.insert(m_product->m_graphicsFamilyIndex.value());
    if (m_product->m_presentFamilyIndex.has_value())
        uniqueQueueFamilies.insert(m_product->m_presentFamilyIndex.value());
    if (m_product->m_computeFamilyIndex.has_value())
        uniqueQueueFamilies.insert(m_product->m_computeFamilyIndex.value());

    float queuePriority = 1.f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                   .queueFamilyIndex = queueFamily,
                                                   .queueCount = 1,
                                                   .pQueuePriorities = &queuePriority};
        queueCreateInfos.emplace_back(queueCreateInfo);
    }

    auto contextPtr = m_cx.lock();
    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &m_product->m_features,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = static_cast<uint32_t>(contextPtr->getLayerCount()),
        .ppEnabledLayerNames = contextPtr->getLayers(),
        .enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size()),
        .ppEnabledExtensionNames = m_deviceExtensions.data(),
    };

    // create device
    VkResult res = vkCreateDevice(m_product->m_physicalHandle, &createInfo, nullptr, &m_product->m_handle);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create logical device : " << res << std::endl;
        return nullptr;
    }

    // queue

    vkGetDeviceQueue(m_product->m_handle, m_product->m_graphicsFamilyIndex.value(), 0, &m_product->m_graphicsQueue);
    if (m_product->m_surface)
        vkGetDeviceQueue(m_product->m_handle, m_product->m_presentFamilyIndex.value(), 0, &m_product->m_presentQueue);
    if (m_product->m_computeFamilyIndex.has_value())
        vkGetDeviceQueue(m_product->m_handle, m_product->m_computeFamilyIndex.value(), 0, &m_product->m_computeQueue);

    // command pools

    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_product->m_graphicsFamilyIndex.value(),
    };
    res = vkCreateCommandPool(m_product->m_handle, &commandPoolCreateInfo, nullptr, &m_product->m_commandPool);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create command pool : " << res << std::endl;
        return nullptr;
    }

    VkCommandPoolCreateInfo commandPoolTransientCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = m_product->m_graphicsFamilyIndex.value(),
    };
    res = vkCreateCommandPool(m_product->m_handle, &commandPoolTransientCreateInfo, nullptr,
                              &m_product->m_commandPoolTransient);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create transient command pool : " << res << std::endl;
        return nullptr;
    }

    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorCreateInfo.physicalDevice = m_product->m_physicalHandle;
    allocatorCreateInfo.device = m_product->m_handle;
    allocatorCreateInfo.instance = m_cx.lock()->getInstanceHandle();
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

    vmaCreateAllocator(&allocatorCreateInfo, &m_product->m_allocator);

    return std::move(m_product);
}