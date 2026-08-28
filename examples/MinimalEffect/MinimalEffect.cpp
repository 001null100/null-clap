#include "MinimalEffect.hpp"

#include <clap/plugin-features.h>
#include <algorithm>
#include <cmath>

const clap_plugin_descriptor_t& MinimalEffect::descriptor() noexcept
{
    static const char* const features[] {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_UTILITY,
        nullptr,
    };

    static const clap_plugin_descriptor_t descriptor {
        CLAP_VERSION,
        "dev.nullexo.nullclap.minimal-effect",
        "NullClap Minimal Effect",
        "Null Exo",
        "https://github.com/001null100/null-clap",
        "",
        "",
        "0.1.0",
        "Minimal stereo gain effect used to validate null-clap.",
        features,
    };
    return descriptor;
}

MinimalEffect::MinimalEffect(const clap_host_t* host)
    : Plugin(&descriptor(), host)
{
    auto gain = nullclap::ParameterSpec::continuous(gainId, "Gain", "Main", -60.0, 12.0, 0.0);
    gain.unit = "dB";
    gain.displayPrecision = 2;
    parameters().add(std::move(gain));

    auto input = nullclap::AudioPortSpec::stereo(0, "Stereo Input", true);
    auto output = nullclap::AudioPortSpec::stereo(0, "Stereo Output", true);
    input.inPlacePair = output.id;
    output.inPlacePair = input.id;
    audioPorts().addInput(std::move(input));
    audioPorts().addOutput(std::move(output));

    nullclap::RemoteControlPage page;
    page.id = nullclap::stableId("minimal.remote.main");
    page.section = "Main";
    page.name = "Performance";
    page.parameters[0] = gainId;
    remoteControls().add(std::move(page));
}

void MinimalEffect::processAudio(const clap_process_t& process,
                                 std::uint32_t startFrame,
                                 std::uint32_t endFrame) noexcept
{
    if (process.audio_inputs_count == 0 || process.audio_outputs_count == 0 || startFrame >= endFrame)
        return;

    const auto& input = process.audio_inputs[0];
    auto& output = process.audio_outputs[0];
    const auto channels = std::min(input.channel_count, output.channel_count);
    const double linearGain = std::pow(10.0, parameters().effectiveValue(gainId) / 20.0);

    if (input.data32 != nullptr && output.data32 != nullptr)
    {
        const float gain = static_cast<float>(linearGain);
        for (std::uint32_t channel = 0; channel < channels; ++channel)
        {
            const auto* source = input.data32[channel];
            auto* destination = output.data32[channel];
            if (source == nullptr || destination == nullptr)
                continue;
            for (std::uint32_t frame = startFrame; frame < endFrame; ++frame)
                destination[frame] = source[frame] * gain;
        }
        return;
    }

    if (input.data64 != nullptr && output.data64 != nullptr)
    {
        for (std::uint32_t channel = 0; channel < channels; ++channel)
        {
            const auto* source = input.data64[channel];
            auto* destination = output.data64[channel];
            if (source == nullptr || destination == nullptr)
                continue;
            for (std::uint32_t frame = startFrame; frame < endFrame; ++frame)
                destination[frame] = source[frame] * linearGain;
        }
    }
}
