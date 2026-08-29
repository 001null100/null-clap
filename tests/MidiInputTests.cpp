#include <nullclap/Factory.hpp>
#include <nullclap/Id.hpp>
#include <nullclap/NotePorts.hpp>
#include <nullclap/Plugin.hpp>
#include <nullclap/PluginFeatures.hpp>

#include <cassert>
#include <clap/events.h>
#include <clap/ext/note-ports.h>
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

bool descriptorHasFeature(const clap_plugin_descriptor_t& descriptor, const char* feature)
{
    if (descriptor.features == nullptr)
        return false;
    for (const auto* const* current = descriptor.features; *current != nullptr; ++current)
        if (std::strcmp(*current, feature) == 0)
            return true;
    return false;
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
    static inline MidiProbePlugin* lastInstance = nullptr;

    static const clap_plugin_descriptor_t& descriptor() noexcept
    {
        // Deliberately start as a plain audio effect. The target-level null-clap
        // routing opt-in must augment this descriptor at the factory boundary.
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
            nullclap::pluginFeatures::stereoAudioEffect.data(),
        };
        return value;
    }

    explicit MidiProbePlugin(const clap_host_t* host)
        : Plugin(&descriptor(), host)
    {
        lastInstance = this;
        notePorts().addInput(nullclap::NotePortSpec::controllerInput(
            nullclap::stableId("tests-midi-probe.input"), "MIDI Input"));
    }

    ~MidiProbePlugin() override
    {
        if (lastInstance == this)
            lastInstance = nullptr;
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
    auto host = makeHost();
    const auto* factory = nullclap::SinglePluginFactory<MidiProbePlugin>::get();
    assert(factory->get_plugin_count(factory) == 1);

    const auto* descriptor = factory->get_plugin_descriptor(factory, 0);
    assert(descriptor != nullptr);
    assert(descriptorHasFeature(*descriptor, CLAP_PLUGIN_FEATURE_AUDIO_EFFECT));
    assert(descriptorHasFeature(*descriptor, CLAP_PLUGIN_FEATURE_STEREO));
    assert(descriptorHasFeature(*descriptor, CLAP_PLUGIN_FEATURE_NOTE_EFFECT));

    const auto* plugin = factory->create_plugin(factory, &host, descriptor->id);
    assert(plugin != nullptr);
    assert(plugin->desc == descriptor);
    auto* instance = MidiProbePlugin::lastInstance;
    assert(instance != nullptr);

    assert(plugin->init(plugin));

    const auto* notePorts = static_cast<const clap_plugin_note_ports_t*>(
        plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS));
    assert(notePorts != nullptr);
    assert(notePorts->count(plugin, true) == 1);
    clap_note_port_info_t inputInfo {};
    assert(notePorts->get(plugin, 0, true, &inputInfo));
    assert((inputInfo.supported_dialects & CLAP_NOTE_DIALECT_MIDI) != 0);
    assert((inputInfo.supported_dialects & CLAP_NOTE_DIALECT_MIDI_MPE) != 0);
    assert((inputInfo.supported_dialects & CLAP_NOTE_DIALECT_CLAP) != 0);
    assert(inputInfo.preferred_dialect == CLAP_NOTE_DIALECT_MIDI);

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
    assert(MidiProbePlugin::lastInstance == nullptr);
    return 0;
}
