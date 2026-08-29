#include <nullclap/Id.hpp>
#include <nullclap/NotePorts.hpp>
#include <nullclap/Plugin.hpp>
#include <nullclap/PluginFeatures.hpp>

#include <cassert>
#include <clap/events.h>
#include <clap/host.h>
#include <clap/plugin.h>
#include <cstdint>
#include <cstring>

namespace
{
const void* CLAP_ABI hostGetExtension(const clap_host_t*, const char*) { return nullptr; }
void CLAP_ABI hostRequestRestart(const clap_host_t*) {}
void CLAP_ABI hostRequestProcess(const clap_host_t*) {}
void CLAP_ABI hostRequestCallback(const clap_host_t*) {}

clap_host_t makeHost()
{
    return {
        CLAP_VERSION,
        nullptr,
        "null-clap tests",
        "null-clap",
        "https://github.com/001null100/null-clap",
        "0.1",
        &hostGetExtension,
        &hostRequestRestart,
        &hostRequestProcess,
        &hostRequestCallback,
    };
}

struct OneEventList
{
    clap_input_events_t interface {};
    clap_event_midi_t event {};

    OneEventList()
    {
        interface.ctx = this;
        interface.size = [](const clap_input_events_t*) -> std::uint32_t { return 1; };
        interface.get = [](const clap_input_events_t* list, std::uint32_t index) -> const clap_event_header_t* {
            if (index != 0)
                return nullptr;
            const auto* self = static_cast<const OneEventList*>(list->ctx);
            return &self->event.header;
        };
    }
};

class MidiProbePlugin final : public nullclap::Plugin
{
public:
    static const clap_plugin_descriptor_t& descriptor() noexcept
    {
        static const clap_plugin_descriptor_t value {
            CLAP_VERSION,
            "dev.nullclap.tests-midi-probe",
            "null-clap MIDI probe",
            "null-clap",
            "https://github.com/001null100/null-clap",
            "",
            "",
            "0.1",
            "Framework raw MIDI delivery probe",
            nullclap::pluginFeatures::stereoAudioEffectWithNoteInput.data(),
        };
        return value;
    }

    explicit MidiProbePlugin(const clap_host_t* host)
        : Plugin(&descriptor(), host)
    {
        notePorts().addInput(nullclap::NotePortSpec::midi(
            nullclap::stableId("tests-midi-probe.input"), "MIDI Input"));
    }

    int midiEvents = 0;
    std::uint16_t portIndex = 0;
    std::uint8_t status = 0;
    std::uint8_t data1 = 0;
    std::uint8_t data2 = 0;

private:
    void processAudio(const clap_process_t&, std::uint32_t, std::uint32_t) noexcept override {}

    void onEvent(const clap_event_header_t& header) noexcept override
    {
        if (header.space_id != CLAP_CORE_EVENT_SPACE_ID || header.type != CLAP_EVENT_MIDI)
            return;
        const auto& midi = reinterpret_cast<const clap_event_midi_t&>(header);
        ++midiEvents;
        portIndex = midi.port_index;
        status = midi.data[0];
        data1 = midi.data[1];
        data2 = midi.data[2];
    }
};
} // namespace

int main()
{
    // The hybrid profile deliberately retains audio-effect identity while exposing
    // the host-facing note-effect category used by hosts when constructing note routes.
    assert(std::strcmp(nullclap::pluginFeatures::stereoAudioEffectWithNoteInput[0],
                       CLAP_PLUGIN_FEATURE_AUDIO_EFFECT) == 0);
    assert(std::strcmp(nullclap::pluginFeatures::stereoAudioEffectWithNoteInput[1],
                       CLAP_PLUGIN_FEATURE_NOTE_EFFECT) == 0);

    auto host = makeHost();
    auto* instance = new MidiProbePlugin(&host);
    const auto* plugin = instance->clapPlugin();

    assert(plugin->init(plugin));
    assert(plugin->activate(plugin, 48000.0, 1, 64));
    assert(plugin->start_processing(plugin));

    OneEventList events;
    events.event.header.size = sizeof(events.event);
    events.event.header.time = 3;
    events.event.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    events.event.header.type = CLAP_EVENT_MIDI;
    events.event.header.flags = CLAP_EVENT_IS_LIVE;
    events.event.port_index = 0;
    events.event.data[0] = 0xBF; // channel 16 CC
    events.event.data[1] = 20;
    events.event.data[2] = 127;

    clap_process_t process {};
    process.frames_count = 16;
    process.in_events = &events.interface;

    assert(plugin->process(plugin, &process) == CLAP_PROCESS_CONTINUE);
    assert(instance->midiEvents == 1);
    assert(instance->portIndex == 0);
    assert(instance->status == 0xBF);
    assert(instance->data1 == 20);
    assert(instance->data2 == 127);

    plugin->stop_processing(plugin);
    plugin->deactivate(plugin);
    plugin->destroy(plugin);
    return 0;
}
