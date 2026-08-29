#pragma once

#include <clap/factory/plugin-factory.h>
#include <cstdint>
#include <cstring>

namespace nullclap
{
template <typename PluginType>
class SinglePluginFactory
{
public:
    static const clap_plugin_factory_t* get() noexcept
    {
        static const clap_plugin_factory_t factory {
            &pluginCount,
            &pluginDescriptor,
            &createPlugin,
        };
        return &factory;
    }

private:
    static std::uint32_t CLAP_ABI pluginCount(const clap_plugin_factory_t*) noexcept
    {
        return 1;
    }

    static const clap_plugin_descriptor_t* CLAP_ABI pluginDescriptor(const clap_plugin_factory_t*,
                                                                      std::uint32_t index) noexcept
    {
        return index == 0 ? &PluginType::descriptor() : nullptr;
    }

    static const clap_plugin_t* CLAP_ABI createPlugin(const clap_plugin_factory_t*,
                                                       const clap_host_t* host,
                                                       const char* pluginId) noexcept
    {
        if (host == nullptr || pluginId == nullptr || std::strcmp(pluginId, PluginType::descriptor().id) != 0)
            return nullptr;

        try
        {
            auto* instance = new PluginType(host);
            return instance->clapPlugin();
        }
        catch (...)
        {
            return nullptr;
        }
    }
};
} // namespace nullclap
