#pragma once

#include <iostream>
#include <memory>

#include <vulkan/vulkan.h>

class Context;
class InstanceBuilder;

class Instance
{
    friend InstanceBuilder;

  private:
    VkInstance m_handle;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallback(VkDebugReportFlagsEXT flags,
                                                              VkDebugReportObjectTypeEXT objectType, uint64_t object,
                                                              size_t location, int32_t messageCode,
                                                              const char *pLayerPrefix, const char *pMessage,
                                                              void *pUserData)
    {

        std::cerr << "debug report: " << pMessage << std::endl;

        return VK_FALSE;
    }

    Instance() = default;

  public:
    ~Instance();

    Instance(const Instance &) = delete;
    Instance &operator=(const Instance &) = delete;
    Instance(Instance &&) = delete;
    Instance &operator=(Instance &&) = delete;

  public:
    [[nodiscard]] inline const VkInstance &getHandle() const
    {
        return m_handle;
    }
};

class InstanceBuilder
{
  private:
    std::unique_ptr<Instance> m_product;

    const Context *m_cx;

    void restart()
    {
        m_product = std::unique_ptr<Instance>(new Instance);
    }

    std::string m_appName;
    uint32_t m_appVersion;
    std::string m_engineName;
    uint32_t m_engineVersion;
    uint32_t m_apiVersion;
    bool m_bUseReportCallback = false;
    VkDebugReportFlagsEXT m_reportCallbackFlags;

  public:
    InstanceBuilder()
    {
        restart();
        m_reportCallbackFlags = VK_DEBUG_REPORT_INFORMATION_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT |
                                VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_ERROR_BIT_EXT |
                                VK_DEBUG_REPORT_DEBUG_BIT_EXT;
    }

    void setContext(const Context *cx)
    {
        m_cx = cx;
    }
    void setApplicationName(const std::string &name)
    {
        m_appName = name;
    }
    void setApplicationVersion(uint32_t major, uint32_t minor, uint32_t patch)
    {
        m_appVersion = VK_MAKE_API_VERSION(0, major, minor, patch);
    }
    void setEngineName(const std::string &name)
    {
        m_engineName = name;
    }
    void setEngineVersion(uint32_t major, uint32_t minor, uint32_t patch)
    {
        m_engineVersion = VK_MAKE_API_VERSION(0, major, minor, patch);
    }
    void setApiVersion(uint32_t major, uint32_t minor, uint32_t patch)
    {
        m_apiVersion = VK_MAKE_API_VERSION(0, major, minor, patch);
    }
    void setUseReportCallback(bool bUse)
    {
        m_bUseReportCallback = bUse;
    }
    void setReportCallbackFlags(VkDebugReportFlagsEXT flags)
    {
        m_reportCallbackFlags = flags;
    }

    std::unique_ptr<Instance> build();
};