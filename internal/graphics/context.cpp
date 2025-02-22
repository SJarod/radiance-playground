#include "device.hpp"
#include <iostream>
#include <set>

#include "context.hpp"

std::vector<VkPhysicalDevice> Context::getAvailablePhysicalDevices() const
{
    uint32_t count;
    vkEnumeratePhysicalDevices(m_instance->getHandle(), &count, nullptr);
    std::vector<VkPhysicalDevice> physicalDevices(count);
    vkEnumeratePhysicalDevices(m_instance->getHandle(), &count, physicalDevices.data());
    return physicalDevices;
}

std::unique_ptr<Context> ContextBuilder::build()
{
    InstanceBuilder ib;
    ib.setContext(m_product.get());
    ib.setUseReportCallback(false);
    ib.setApplicationName("RadiancePlayground");
    ib.setApplicationVersion(0, 0, 0);
    ib.setEngineName("RadiancePlaygroundEngine");
    ib.setEngineVersion(0, 0, 0);
    ib.setApiVersion(1, 3, 296);
    m_product->m_instance = ib.build();
    return std::move(m_product);
}