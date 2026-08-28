#include <nullclap/Id.hpp>
#include <nullclap/NotePorts.hpp>
#include <nullclap/ParameterStore.hpp>

#include <cassert>
#include <cmath>
#include <cstring>

int main()
{
    using namespace nullclap;

    static_assert(stableId("gain") == stableId("gain"));
    static_assert(stableId("gain") != stableId("mix"));

    ParameterStore store;
    auto gain = ParameterSpec::continuous(stableId("test.gain"), "Gain", "Main", -60.0, 12.0, 0.0);
    gain.unit = "dB";
    assert(store.add(std::move(gain)));
    assert(!store.add(ParameterSpec::continuous(stableId("test.gain"), "Duplicate", "Main", 0.0, 1.0, 0.5)));

    const auto modeId = stableId("test.mode");
    assert(store.add(ParameterSpec::choice(modeId, "Mode", "Main", { "A", "B", "C" }, 1)));

    assert(store.count() == 2);
    assert(store.setBaseValue(stableId("test.gain"), 6.0));
    assert(store.setModulation(stableId("test.gain"), 3.5));
    assert(std::abs(store.effectiveValue(stableId("test.gain")) - 9.5) < 1.0e-9);

    assert(store.setBaseValue(modeId, 1.8));
    assert(store.value(modeId) == 2.0);

    char text[64] {};
    assert(store.valueToText(modeId, 2.0, text, sizeof(text)));
    assert(std::strcmp(text, "C") == 0);

    double parsed = 0.0;
    assert(store.textToValue(modeId, "B", parsed));
    assert(parsed == 1.0);

    clap_event_param_value_t valueEvent {};
    valueEvent.header.size = sizeof(valueEvent);
    valueEvent.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    valueEvent.header.type = CLAP_EVENT_PARAM_VALUE;
    valueEvent.param_id = stableId("test.gain");
    valueEvent.note_id = -1;
    valueEvent.port_index = -1;
    valueEvent.channel = -1;
    valueEvent.key = -1;
    valueEvent.value = -12.0;
    assert(store.applyInputEvent(valueEvent.header));
    assert(store.value(stableId("test.gain")) == -12.0);

    valueEvent.note_id = 42;
    valueEvent.value = -30.0;
    assert(!store.applyInputEvent(valueEvent.header));
    assert(store.value(stableId("test.gain")) == -12.0);

    NotePorts ports;
    const auto midiPortId = stableId("test.note.midi-in");
    ports.addInput(NotePortSpec::midi(midiPortId, "MIDI Input"));
    assert(ports.count(true) == 1);
    assert(ports.count(false) == 0);

    clap_note_port_info_t noteInfo {};
    assert(ports.info(0, true, noteInfo));
    assert(noteInfo.id == midiPortId);
    assert(noteInfo.supported_dialects == CLAP_NOTE_DIALECT_MIDI);
    assert(noteInfo.preferred_dialect == CLAP_NOTE_DIALECT_MIDI);
    assert(std::strcmp(noteInfo.name, "MIDI Input") == 0);
    assert(!ports.info(1, true, noteInfo));

    return 0;
}
