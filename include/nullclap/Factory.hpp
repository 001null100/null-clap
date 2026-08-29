#pragma once

#include <array>
#include <clap/factory/plugin-factory.h>
#include <clap/plugin-features.h>
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
    static const clap_plugin_descriptor_t& descriptorForHost() noexcept
    {
#if defined(NULLCLAP_ROUTE_NOTE_INPUT_AS_NOTE_EFFECT)
        // Descriptor features are discovery-time host-routing metadata. Preserve
        // every application feature and add NOTE_EFFECT exactly once for targets
        // which explicitly opted into hybrid audio + note-input routing.
        static const auto features = [] {
            std::array<const char*, 64> result {};
            std::size_t count = 0;
            bool hasNoteEffect = false;
            const auto* const* source = PluginType::descriptor().features;
            if (source != nullptr)
            {
                while (*source != nullptr && count + 2 < result.size())
                {
                    hasNoteEffect |= std::strcmp(*source, CLAP_PLUGIN_FEATURE_NOTE_EFFECT) == 0;
                    result[count++] = *source++;
                }
            }
            if (!hasNoteEffect && count + 1 < result.size())
                result[count++] = CLAP_PLUGIN_FEATURE_NOTE_EFFECT;
            result[count] = nullptr;
            return result;
        }();

        static const auto descriptor = [] {
            auto result = PluginType::descriptor();
            result.features = features.data();
            return result;
        }();
        return descriptor;
#else
        return PluginType::descriptor();
#endif
    }

    static std::uint32_t CLAP_ABI pluginCount(const clap_plugin_factory_t*) noexcept
    {
        return 1;
    }

    static const clap_plugin_descriptor_t* CLAP_ABI pluginDescriptor(const clap_plugin_factory_t*,
                                                                      std::uint32_t index) noexcept
    {
        return index == 0 ? &descriptorForHost() : nullptr;
    }

    static const clap_plugin_t* CLAP_ABI createPlugin(const clap_plugin_factory_t*,
                                                       const clap_host_t* host,
                                                       const char* pluginId) noexcept
    {
        const auto& hostDescriptor = descriptorForHost();
        if (host == nullptr || pluginId == nullptr || std::strcmp(pluginId, hostDescriptor.id) != 0)
            return nullptr;

        try
        {
            auto* instance = new PluginType(host);
            auto* plugin = const_cast<clap_plugin_t*>(instance->clapPlugin());
            // clap_plugin_t::desc must match the descriptor returned by the factory.
            // The opt-in host profile is implemented here so consuming plug-ins do
            // not need to duplicate descriptor arrays or diverge instance/factory metadata.
            plugin->desc = &hostDescriptor;
            return plugin;
        }
        catch (...)
        {
            return nullptr;
        }
    }
};
} // namespace nullclap
