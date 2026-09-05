#include <nullclap/Id.hpp>
#include <nullclap/NotePorts.hpp>
#include <nullclap/ParameterStore.hpp>

#include "TestCheck.hpp"
#include <cmath>
#include <cstring>
#include <limits>

int main()
{
    using namespace nullclap;

    static_assert(stableId("gain") == stableId("gain"));
    static_assert(stableId("gain") != stableId("mix"));

    ParameterStore store;
    auto gain = ParameterSpec::continuous(stableId("test.gain"), "Gain", "Main", -60.0, 12.0, 0.0);
    gain.unit = "dB";
    CHECK(store.add(std::move(gain)));
    CHECK(!store.add(ParameterSpec::continuous(stableId("test.gain"), "Duplicate", "Main", 0.0, 1.0, 0.5)));

    const auto modeId = stableId("test.mode");
    CHECK(store.add(ParameterSpec::choice(modeId, "Mode", "Main", { "A", "B", "C" }, 1)));

    CHECK(store.count() == 2);
    CHECK(store.setBaseValue(stableId("test.gain"), 6.0));
    CHECK(store.setModulation(stableId("test.gain"), 3.5));
    CHECK(std::abs(store.effectiveValue(stableId("test.gain")) - 9.5) < 1.0e-9);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (const double invalid : { nan, inf, -inf })
    {
        CHECK(!store.add(ParameterSpec::continuous(100, "Bad minimum", "", invalid, 1.0, 0.0)));
        CHECK(!store.add(ParameterSpec::continuous(100, "Bad maximum", "", 0.0, invalid, 0.0)));
        CHECK(!store.add(ParameterSpec::continuous(100, "Bad default", "", 0.0, 1.0, invalid)));
        CHECK(!store.setModulation(stableId("test.gain"), invalid));
        CHECK(store.modulation(stableId("test.gain")) == 3.5);
        CHECK(std::isfinite(store.effectiveValue(stableId("test.gain"))));
    }
    CHECK(store.count() == 2);

    CHECK(store.setBaseValue(modeId, 1.8));
    CHECK(store.value(modeId) == 2.0);

    char text[64] {};
    CHECK(store.valueToText(modeId, 2.0, text, sizeof(text)));
    CHECK(std::strcmp(text, "C") == 0);

    double parsed = 0.0;
    CHECK(store.textToValue(modeId, "B", parsed));
    CHECK(parsed == 1.0);

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
    CHECK(store.applyInputEvent(valueEvent.header));
    CHECK(store.value(stableId("test.gain")) == -12.0);

    valueEvent.note_id = 42;
    valueEvent.value = -30.0;
    CHECK(!store.applyInputEvent(valueEvent.header));
    CHECK(store.value(stableId("test.gain")) == -12.0);

    clap_event_param_mod_t modEvent {};
    modEvent.header.size = sizeof(modEvent);
    modEvent.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    modEvent.header.type = CLAP_EVENT_PARAM_MOD;
    modEvent.param_id = stableId("test.gain");
    modEvent.note_id = -1;
    modEvent.port_index = -1;
    modEvent.channel = -1;
    modEvent.key = -1;
    modEvent.amount = nan;
    CHECK(!store.applyInputEvent(modEvent.header));
    CHECK(store.modulation(stableId("test.gain")) == 3.5);

    // Writable host parameters always participate in state; read-only telemetry
    // can opt out. Preserve this existing state/validator contract.
    const auto transientWritableId = stableId("test.transient-writable");
    auto transientWritable = ParameterSpec::continuous(
        transientWritableId, "Transient Writable", "Main", 0.0, 1.0, 0.0);
    transientWritable.persistent = false;
    CHECK(store.add(std::move(transientWritable)));
    CHECK(store.setBaseValue(transientWritableId, 0.75));

    const auto transientReadOnlyId = stableId("test.transient-readonly");
    auto transientReadOnly = ParameterSpec::continuous(
        transientReadOnlyId, "Transient Readonly", "Main", 0.0, 1.0, 0.0, CLAP_PARAM_IS_READONLY);
    transientReadOnly.persistent = false;
    CHECK(store.add(std::move(transientReadOnly)));
    CHECK(store.setInternalValue(transientReadOnlyId, 0.8));

    bool savedWritable = false;
    bool savedReadOnly = false;
    for (const auto& saved : store.persistentValues())
    {
        savedWritable |= saved.id == transientWritableId;
        savedReadOnly |= saved.id == transientReadOnlyId;
    }
    CHECK(savedWritable);
    CHECK(!savedReadOnly);
    CHECK(store.restorePersistentValue(transientWritableId, 0.25));
    CHECK(std::abs(store.value(transientWritableId) - 0.25) < 1.0e-9);
    CHECK(!store.restorePersistentValue(transientReadOnlyId, 0.25));

    NotePorts ports;
    const auto midiPortId = stableId("test.note.midi-in");
    ports.addInput(NotePortSpec::midi(midiPortId, "MIDI Input"));
    CHECK(ports.count(true) == 1);
    CHECK(ports.count(false) == 0);

    clap_note_port_info_t noteInfo {};
    CHECK(ports.info(0, true, noteInfo));
    CHECK(noteInfo.id == midiPortId);
    CHECK(noteInfo.supported_dialects == CLAP_NOTE_DIALECT_MIDI);
    CHECK(noteInfo.preferred_dialect == CLAP_NOTE_DIALECT_MIDI);
    CHECK(std::strcmp(noteInfo.name, "MIDI Input") == 0);
    CHECK(!ports.info(1, true, noteInfo));

    auto huge = ParameterSpec::continuous(101, "Huge range", "", -1.0e300, 1.0e300, 0.0);
    huge.valueLabels = { "Low" };
    CHECK(store.add(std::move(huge)));
    CHECK(store.valueToText(101, 1.0e300, text, sizeof(text)));
    CHECK(text[sizeof(text) - 1] == '\0');
    return 0;
}
