#include <iostream>
#include <set>
#include <vector>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "context.hpp"
#include "surface.hpp"

#include "device.hpp"

#define VK_INSTANCE_PROC_ADDR(func) func = PFN_##func(vkGetInstanceProcAddr(m_cx.lock()->getInstanceHandle(), #func))

Device::~Device()
{
    std::cout << "Destroying device : " << getDeviceName() << std::endl;

    // ensure all the resources have been freed
    if (m_bufferCount != 0)
    {
        std::cout << "\tUndestroyed buffers :" << std::endl;
        for (auto &&it = m_bufferNames.begin(); it != m_bufferNames.end(); ++it)
        {
            std::cout << "\t\t" << *it << std::endl;
        }
    }
    if (m_imageCount != 0)
    {
        std::cout << "\tUndestroyed images :" << std::endl;
        for (auto &&it = m_imageNames.begin(); it != m_imageNames.end(); ++it)
        {
            std::cout << "\t\t" << *it << std::endl;
        }
    }
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

const VkInstance Device::getContextInstance() const
{
    return m_cx.lock()->getInstanceHandle();
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

VkCommandBuffer Device::cmdBeginOneTimeSubmit(std::string cmdName) const
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
    addDebugObjectName(VkDebugUtilsObjectNameInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
        .objectType = VK_OBJECT_TYPE_COMMAND_BUFFER,
        .objectHandle = (uint64_t)commandBuffer,
        .pObjectName = std::string(cmdName + " transient command buffer").c_str(),
    });
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

    res = vkQueueWaitIdle(m_graphicsQueue);
    if (res != VK_SUCCESS)
        std::cerr << "Wait for transient command buffer failed : " << res << std::endl;

    assert(res == VK_SUCCESS);
    if (res != VK_SUCCESS)
        abort();

    vkFreeCommandBuffers(m_handle, m_commandPoolTransient, 1, &commandBuffer);
}

void Device::addDebugObjectName(VkDebugUtilsObjectNameInfoEXT nameInfo) const
{
    vkSetDebugUtilsObjectNameEXT(m_handle, &nameInfo);
}

void DeviceBuilder::setPhysicalDevice(VkPhysicalDevice a)
{
    m_product->m_physicalHandle = a;

    m_product->m_rqFeatures = VkPhysicalDeviceRayQueryFeaturesKHR{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
    };
    m_product->m_rtvalidationFeatures = VkPhysicalDeviceRayTracingValidationFeaturesNV{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_VALIDATION_FEATURES_NV,
        .pNext = &m_product->m_rqFeatures,
    };
    m_product->m_asFeatures = VkPhysicalDeviceAccelerationStructureFeaturesKHR{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .pNext = &m_product->m_rtvalidationFeatures,
        .accelerationStructure = true,
    };
    m_product->m_multiviewFeature = VkPhysicalDeviceMultiviewFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MULTIVIEW_FEATURES,
        .pNext = &m_product->m_asFeatures,
        .multiview = true,
        .multiviewGeometryShader = false,
        .multiviewTessellationShader = false,
    };

    m_product->m_bufferDeviceAddressFeature = VkPhysicalDeviceBufferDeviceAddressFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
        .pNext = &m_product->m_multiviewFeature,
    };

    m_product->m_features13 = VkPhysicalDeviceVulkan13Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &m_product->m_bufferDeviceAddressFeature,
    };
    // enabling synchronization2 feature
    m_product->m_features13.synchronization2 = VK_TRUE;

    m_product->m_features = VkPhysicalDeviceFeatures2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &m_product->m_features13,
    };

    vkGetPhysicalDeviceFeatures2(a, &m_product->m_features);

#ifndef NDEBUG
    if (m_product->m_rtvalidationFeatures.rayTracingValidation)
        addDeviceExtensionIfAvailable(VK_NV_RAY_TRACING_VALIDATION_EXTENSION_NAME);
#endif

    vkGetPhysicalDeviceProperties(a, &m_product->m_props);
    vkGetPhysicalDeviceProperties2(a, &m_product->m_props2);

    {
        uint32_t count;
        vkEnumerateDeviceLayerProperties(a, &count, nullptr);
        std::vector<VkLayerProperties> props(count);
        vkEnumerateDeviceLayerProperties(a, &count, props.data());
        std::cout << "Available layers for " << m_product->getDeviceName() << std::endl;
        for (int i = 0; i < count; ++i)
        {
            std::cout << "\t" << props[i].layerName << std::endl;
        }
    }
    {
        uint32_t count;
        vkEnumerateDeviceExtensionProperties(a, nullptr, &count, nullptr);
        std::vector<VkExtensionProperties> props(count);
        vkEnumerateDeviceExtensionProperties(a, nullptr, &count, props.data());
        std::cout << "Available extensions for " << m_product->getDeviceName() << std::endl;
        for (int i = 0; i < count; ++i)
        {
            std::cout << "\t" << props[i].extensionName << std::endl;
        }
    }

    m_product->m_graphicsFamilyIndex = m_product->findQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
    m_product->m_computeFamilyIndex = m_product->findQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
}

#define VK_INSTANCE_PROC_ADDR_BUILDER(func)                                                                            \
    m_product->func = PFN_##func(vkGetInstanceProcAddr(m_cx.lock()->getInstanceHandle(), #func))

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
    allocatorCreateInfo.flags =
        VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorCreateInfo.physicalDevice = m_product->m_physicalHandle;
    allocatorCreateInfo.device = m_product->m_handle;
    allocatorCreateInfo.instance = m_cx.lock()->getInstanceHandle();
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

    vmaCreateAllocator(&allocatorCreateInfo, &m_product->m_allocator);

    VK_INSTANCE_PROC_ADDR_BUILDER(vkSetDebugUtilsObjectNameEXT);

    VK_INSTANCE_PROC_ADDR_BUILDER(vkCreateAccelerationStructureKHR);
    VK_INSTANCE_PROC_ADDR_BUILDER(vkGetAccelerationStructureBuildSizesKHR);
    VK_INSTANCE_PROC_ADDR_BUILDER(vkGetAccelerationStructureDeviceAddressKHR);
    VK_INSTANCE_PROC_ADDR_BUILDER(vkCmdBuildAccelerationStructuresKHR);

    return std::move(m_product);
}