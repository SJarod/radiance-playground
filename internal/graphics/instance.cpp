#include <cassert>
#include <iostream>

#include "context.hpp"

#include "instance.hpp"

Instance::~Instance()
{
    vkDestroyInstance(m_handle, nullptr);
}

std::unique_ptr<Instance> InstanceBuilder::build()
{
    assert(m_cx);

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = m_appName.c_str(),
        .applicationVersion = m_appVersion,
        .pEngineName = m_engineName.c_str(),
        .engineVersion = m_engineVersion,
        .apiVersion = m_apiVersion,
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(m_cx->getLayerCount()),
        .ppEnabledLayerNames = m_cx->getLayers(),
        .enabledExtensionCount = static_cast<uint32_t>(m_cx->getInstanceExtensionCount()),
        .ppEnabledExtensionNames = m_cx->getInstanceExtensions(),
    };
    if (m_bUseReportCallback)
    {
        VkDebugReportCallbackCreateInfoEXT debugReportCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
            .flags = m_reportCallbackFlags,
            .pfnCallback = &Instance::debugReportCallback,
        };

        createInfo.pNext = &debugReportCreateInfo;
    }

    uint32_t count;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> props(count);
    vkEnumerateInstanceLayerProperties(&count, props.data());
    std::cout << "Available instance extensions" << std::endl;
    for (int i = 0; i < count; ++i)
    {
        std::cout << "\t" << props[i].layerName << std::endl;
    }

    VkResult res = vkCreateInstance(&createInfo, nullptr, &m_product->m_handle);
    if (res != VK_SUCCESS)
    {
        std::cerr << "Failed to create instance" << res << std::endl;
        return nullptr;
    }

    return std::move(m_product);
}
