#pragma once

#include <clap/plugin-features.h>
#include <clap/ext/note-ports.h>
#include <cstdint>

namespace nullclap::midi_role_probe
{
struct AudioEffectCurrentRole
{
    inline static constexpr char id[] = "dev.nullclap.midi-probe.audio-current";
    inline static constexpr char name[] = "NullClap MIDI Probe 1 - Current";
    inline static constexpr const char* features[] {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_STEREO,
        nullptr,
    };
    static constexpr clap_id notePortId = 0x4E434D49u;
    static constexpr std::uint32_t supportedDialects = CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_CLAP;
    static constexpr std::uint32_t preferredDialect = CLAP_NOTE_DIALECT_MIDI;
    static constexpr bool noteOutput = false;
};

struct AudioEffectClapPreferredRole
{
    inline static constexpr char id[] = "dev.nullclap.midi-probe.audio-clap-preferred";
    inline static constexpr char name[] = "NullClap MIDI Probe 2 - CLAP Preferred";
    inline static constexpr const char* features[] {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_STEREO,
        nullptr,
    };
    static constexpr clap_id notePortId = 0x4E434D49u;
    static constexpr std::uint32_t supportedDialects = CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_CLAP;
    static constexpr std::uint32_t preferredDialect = CLAP_NOTE_DIALECT_CLAP;
    static constexpr bool noteOutput = false;
};

struct AudioEffectNihPortRole
{
    inline static constexpr char id[] = "dev.nullclap.midi-probe.audio-nih-port";
    inline static constexpr char name[] = "NullClap MIDI Probe 3 - NIH Port";
    inline static constexpr const char* features[] {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_STEREO,
        nullptr,
    };
    static constexpr clap_id notePortId = 0;
    static constexpr std::uint32_t supportedDialects = CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_CLAP;
    static constexpr std::uint32_t preferredDialect = CLAP_NOTE_DIALECT_CLAP;
    static constexpr bool noteOutput = false;
};

struct AudioInstrumentNihRole
{
    inline static constexpr char id[] = "dev.nullclap.midi-probe.audio-instrument-nih";
    inline static constexpr char name[] = "NullClap MIDI Probe 4 - Audio+Instrument";
    inline static constexpr const char* features[] {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_INSTRUMENT,
        CLAP_PLUGIN_FEATURE_STEREO,
        nullptr,
    };
    static constexpr clap_id notePortId = 0;
    static constexpr std::uint32_t supportedDialects = CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_CLAP;
    static constexpr std::uint32_t preferredDialect = CLAP_NOTE_DIALECT_CLAP;
    static constexpr bool noteOutput = false;
};

struct AudioNoteEffectNihRole
{
    inline static constexpr char id[] = "dev.nullclap.midi-probe.audio-note-effect-nih";
    inline static constexpr char name[] = "NullClap MIDI Probe 5 - Audio+Note Effect";
    inline static constexpr const char* features[] {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_NOTE_EFFECT,
        CLAP_PLUGIN_FEATURE_STEREO,
        nullptr,
    };
    static constexpr clap_id notePortId = 0;
    static constexpr std::uint32_t supportedDialects = CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_CLAP;
    static constexpr std::uint32_t preferredDialect = CLAP_NOTE_DIALECT_CLAP;
    static constexpr bool noteOutput = true;
};

struct AllRolesNihRole
{
    inline static constexpr char id[] = "dev.nullclap.midi-probe.all-roles-nih";
    inline static constexpr char name[] = "NullClap MIDI Probe 6 - All Roles";
    inline static constexpr const char* features[] {
        CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
        CLAP_PLUGIN_FEATURE_INSTRUMENT,
        CLAP_PLUGIN_FEATURE_NOTE_EFFECT,
        CLAP_PLUGIN_FEATURE_STEREO,
        nullptr,
    };
    static constexpr clap_id notePortId = 0;
    static constexpr std::uint32_t supportedDialects = CLAP_NOTE_DIALECT_MIDI | CLAP_NOTE_DIALECT_CLAP;
    static constexpr std::uint32_t preferredDialect = CLAP_NOTE_DIALECT_CLAP;
    static constexpr bool noteOutput = true;
};
} // namespace nullclap::midi_role_probe
