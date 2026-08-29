#pragma once

#include <nullclap/AudioPorts.hpp>
#include <nullclap/Id.hpp>
#include <nullclap/NotePorts.hpp>
#include <nullclap/Parameter.hpp>
#include <nullclap/Plugin.hpp>

#if defined(_WIN32)
#include "EventSnifferGui.hpp"
#endif

#include <algorithm>
#include <atomic>
#include <clap/events.h>
#include <cstdint>
#include <memory>
#include <utility>

namespace nullclap::midi_role_probe
{
template <typename Role>
class ProbePlugin final : public nullclap::Plugin
{
public:
    static const clap_plugin_descriptor_t& descriptor() noexcept
    {
        static const clap_plugin_descriptor_t value {
            CLAP_VERSION,
            Role::id,
            Role::name,
            "null-clap",
            "https://github.com/001null100/null-clap",
            "",
            "",
            "0.3",
            "Bitwig CLAP MIDI routing probe",
            Role::features,
        };
        return value;
    }

    explicit ProbePlugin(const clap_host_t* host)
        : Plugin(&descriptor(), host)
    {
        // clap-validator 0.4.1's param-conversions test crashes on plug-ins that
        // implement CLAP_EXT_PARAMS while exposing zero parameters. null-clap's
        // Plugin base intentionally implements the extension for every plug-in,
        // so keep one inert hidden parameter in these diagnostic-only probes.
        // It has no effect on MIDI routing or audio processing.
        auto sentinel = nullclap::ParameterSpec::continuous(
            sentinelId,
            "Diagnostic Sentinel (ignore)",
            "Diagnostics",
            0.0,
            1.0,
            0.0,
            CLAP_PARAM_IS_HIDDEN);
        sentinel.persistent = false;
        parameters().add(std::move(sentinel));

        if constexpr (sniffsAllEvents())
        {
            auto count = nullclap::ParameterSpec::continuous(
                eventCountId,
                "Event Count",
                "Diagnostics",
                0.0,
                1000000.0,
                0.0,
                CLAP_PARAM_IS_READONLY);
            count.displayPrecision = 0;
            count.persistent = false;
            parameters().add(std::move(count));

            auto type = nullclap::ParameterSpec::continuous(
                lastEventTypeId,
                "Last Event Type",
                "Diagnostics",
                -1.0,
                65535.0,
                -1.0,
                CLAP_PARAM_IS_READONLY);
            type.displayPrecision = 0;
            type.persistent = false;
            parameters().add(std::move(type));

#if defined(_WIN32)
            // Bitwig does not surface the read-only telemetry parameters in its
            // generic device view, so Probe 7 owns a tiny native Win32 panel.
            // The GUI polls only atomics and never touches the audio thread.
            setGuiDelegate(std::make_unique<EventSnifferGui>(eventCount_, lastEventType_, gain_));
#endif
        }

        auto input = nullclap::AudioPortSpec::stereo(nullclap::stableId("midi-role-probe.audio.in"), "Stereo Input", true);
        auto output = nullclap::AudioPortSpec::stereo(nullclap::stableId("midi-role-probe.audio.out"), "Stereo Output", true);
        input.inPlacePair = output.id;
        output.inPlacePair = input.id;
        audioPorts().addInput(std::move(input));
        audioPorts().addOutput(std::move(output));

        notePorts().addInput(nullclap::NotePortSpec::dialects(
            Role::notePortId,
            "MIDI Input",
            Role::supportedDialects,
            Role::preferredDialect));
        if constexpr (Role::noteOutput)
        {
            notePorts().addOutput(nullclap::NotePortSpec::dialects(
                Role::notePortId,
                "MIDI Output",
                Role::supportedDialects,
                Role::preferredDialect));
        }
    }

private:
    static constexpr clap_id sentinelId = nullclap::stableId("midi-role-probe.diagnostic-sentinel");
    static constexpr clap_id eventCountId = nullclap::stableId("midi-role-probe.event-count");
    static constexpr clap_id lastEventTypeId = nullclap::stableId("midi-role-probe.last-event-type");

    static constexpr bool sniffsAllEvents() noexcept
    {
        if constexpr (requires { Role::sniffAllEvents; })
            return Role::sniffAllEvents;
        return false;
    }

    void processAudio(const clap_process_t& process, std::uint32_t start, std::uint32_t end) noexcept override
    {
        const auto* input = process.audio_inputs_count > 0 ? &process.audio_inputs[0] : nullptr;
        auto* output = process.audio_outputs_count > 0 ? &process.audio_outputs[0] : nullptr;
        if (input == nullptr || output == nullptr)
            return;

        const float gain = gain_.load(std::memory_order_relaxed);
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
        if (header.space_id != CLAP_CORE_EVENT_SPACE_ID)
            return;

        if constexpr (sniffsAllEvents())
        {
            // Ignore host parameter traffic so the telemetry parameters cannot
            // trigger themselves. Any other core event proves that Bitwig got as
            // far as the plug-in's CLAP event queue.
            if (header.type == CLAP_EVENT_PARAM_VALUE
                || header.type == CLAP_EVENT_PARAM_MOD
                || header.type == CLAP_EVENT_PARAM_GESTURE_BEGIN
                || header.type == CLAP_EVENT_PARAM_GESTURE_END)
                return;

            const auto count = eventCount_.fetch_add(1, std::memory_order_relaxed) + 1;
            lastEventType_.store(header.type, std::memory_order_relaxed);
            gain_.store(0.5f, std::memory_order_relaxed);
            emitParameterValue(eventCountId,
                               static_cast<double>(std::min<std::uint32_t>(count, 1000000u)),
                               header.time);
            emitParameterValue(lastEventTypeId, static_cast<double>(header.type), header.time);

            if (header.type == CLAP_EVENT_MIDI && header.size >= sizeof(clap_event_midi_t))
            {
                const auto& midi = reinterpret_cast<const clap_event_midi_t&>(header);
                if ((midi.data[0] & 0xF0u) == 0xB0u && midi.data[1] == 20u)
                    gain_.store(midi.data[2] >= 64u ? 0.25f : 1.0f, std::memory_order_relaxed);
            }
            else if (header.type == CLAP_EVENT_MIDI2 && header.size >= sizeof(clap_event_midi2_t))
            {
                const auto& midi2 = reinterpret_cast<const clap_event_midi2_t&>(header);
                const std::uint32_t word0 = midi2.data[0];
                const std::uint32_t messageType = (word0 >> 28u) & 0x0Fu;
                const std::uint32_t status = (word0 >> 20u) & 0x0Fu;
                const std::uint32_t controller = (word0 >> 8u) & 0xFFu;
                if (messageType == 0x4u && status == 0xBu && controller == 20u)
                    gain_.store(midi2.data[1] >= 0x80000000u ? 0.25f : 1.0f, std::memory_order_relaxed);
            }
            return;
        }

        if (header.type != CLAP_EVENT_MIDI || header.size < sizeof(clap_event_midi_t))
            return;

        const auto& midi = reinterpret_cast<const clap_event_midi_t&>(header);
        if ((midi.data[0] & 0xF0u) == 0xB0u && midi.data[1] == 20u)
            gain_.store(midi.data[2] >= 64u ? 0.25f : 1.0f, std::memory_order_relaxed);
    }

    std::atomic<float> gain_ { 1.0f };
    std::atomic<std::uint32_t> eventCount_ { 0 };
    std::atomic<std::uint16_t> lastEventType_ { 0xFFFFu };
};
} // namespace nullclap::midi_role_probe
