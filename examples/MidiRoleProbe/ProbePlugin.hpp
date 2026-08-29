#pragma once

#include <nullclap/AudioPorts.hpp>
#include <nullclap/Id.hpp>
#include <nullclap/NotePorts.hpp>
#include <nullclap/Plugin.hpp>

#include <algorithm>
#include <atomic>
#include <clap/events.h>
#include <cstdint>
#include <utility>

namespace nullclap::midi_role_probe
{
template <const char* PluginId, const char* PluginName, const char* const* Features>
class ProbePlugin final : public nullclap::Plugin
{
public:
    static const clap_plugin_descriptor_t& descriptor() noexcept
    {
        static const clap_plugin_descriptor_t value {
            CLAP_VERSION,
            PluginId,
            PluginName,
            "null-clap",
            "https://github.com/001null100/null-clap",
            "",
            "",
            "0.1",
            "Bitwig CLAP MIDI role-routing probe",
            Features,
        };
        return value;
    }

    explicit ProbePlugin(const clap_host_t* host)
        : Plugin(&descriptor(), host)
    {
        auto input = nullclap::AudioPortSpec::stereo(nullclap::stableId("midi-role-probe.audio.in"), "Stereo Input", true);
        auto output = nullclap::AudioPortSpec::stereo(nullclap::stableId("midi-role-probe.audio.out"), "Stereo Output", true);
        input.inPlacePair = output.id;
        output.inPlacePair = input.id;
        audioPorts().addInput(std::move(input));
        audioPorts().addOutput(std::move(output));
        notePorts().addInput(nullclap::NotePortSpec::midi(nullclap::stableId("midi-role-probe.midi.in"), "MIDI Input"));
    }

private:
    void processAudio(const clap_process_t& process, std::uint32_t start, std::uint32_t end) noexcept override
    {
        const auto* input = process.audio_inputs_count > 0 ? &process.audio_inputs[0] : nullptr;
        auto* output = process.audio_outputs_count > 0 ? &process.audio_outputs[0] : nullptr;
        if (input == nullptr || output == nullptr)
            return;

        const float gain = duck_.load(std::memory_order_relaxed) ? 0.25f : 1.0f;
        const auto channels = std::min(input->channel_count, output->channel_count);
        for (std::uint32_t ch = 0; ch < channels; ++ch)
        {
            if (input->data32 != nullptr && output->data32 != nullptr
                && input->data32[ch] != nullptr && output->data32[ch] != nullptr)
            {
                for (std::uint32_t frame = start; frame < end; ++frame)
                    output->data32[ch][frame] = input->data32[ch][frame] * gain;
            }
            else if (input->data64 != nullptr && output->data64 != nullptr
                     && input->data64[ch] != nullptr && output->data64[ch] != nullptr)
            {
                for (std::uint32_t frame = start; frame < end; ++frame)
                    output->data64[ch][frame] = input->data64[ch][frame] * static_cast<double>(gain);
            }
        }
    }

    void onEvent(const clap_event_header_t& header) noexcept override
    {
        if (header.space_id != CLAP_CORE_EVENT_SPACE_ID || header.type != CLAP_EVENT_MIDI
            || header.size < sizeof(clap_event_midi_t))
            return;
        const auto& midi = reinterpret_cast<const clap_event_midi_t&>(header);
        if ((midi.data[0] & 0xF0u) == 0xB0u && midi.data[1] == 20u)
            duck_.store(midi.data[2] >= 64u, std::memory_order_relaxed);
    }

    std::atomic<bool> duck_ { false };
};
} // namespace nullclap::midi_role_probe
