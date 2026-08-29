#pragma once

#include <clap/plugin-features.h>

namespace nullclap::midi_role_probe
{
inline constexpr char audioEffectId[] = "dev.nullclap.midi-role-probe.audio-effect";
inline constexpr char audioEffectName[] = "NullClap MIDI Probe - Audio Effect";
inline constexpr char audioNoteEffectId[] = "dev.nullclap.midi-role-probe.audio-note-effect";
inline constexpr char audioNoteEffectName[] = "NullClap MIDI Probe - Audio+Note Effect";
inline constexpr char audioInstrumentId[] = "dev.nullclap.midi-role-probe.audio-instrument";
inline constexpr char audioInstrumentName[] = "NullClap MIDI Probe - Audio+Instrument";
inline constexpr char allRolesId[] = "dev.nullclap.midi-role-probe.all-roles";
inline constexpr char allRolesName[] = "NullClap MIDI Probe - All Roles";
inline constexpr char instrumentId[] = "dev.nullclap.midi-role-probe.instrument";
inline constexpr char instrumentName[] = "NullClap MIDI Probe - Instrument";

inline constexpr const char* audioEffectFeatures[] {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_STEREO,
    nullptr,
};
inline constexpr const char* audioNoteEffectFeatures[] {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_NOTE_EFFECT,
    CLAP_PLUGIN_FEATURE_STEREO,
    nullptr,
};
inline constexpr const char* audioInstrumentFeatures[] {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_INSTRUMENT,
    CLAP_PLUGIN_FEATURE_STEREO,
    nullptr,
};
inline constexpr const char* allRolesFeatures[] {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_INSTRUMENT,
    CLAP_PLUGIN_FEATURE_NOTE_EFFECT,
    CLAP_PLUGIN_FEATURE_STEREO,
    nullptr,
};
inline constexpr const char* instrumentFeatures[] {
    CLAP_PLUGIN_FEATURE_INSTRUMENT,
    CLAP_PLUGIN_FEATURE_STEREO,
    nullptr,
};
} // namespace nullclap::midi_role_probe
