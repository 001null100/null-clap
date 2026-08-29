#pragma once

#include <array>
#include <clap/plugin-features.h>

namespace nullclap::pluginFeatures
{
// Conventional stereo audio effect.
inline constexpr std::array<const char*, 3> stereoAudioEffect {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_STEREO,
    nullptr,
};

// Stereo audio processor whose output is also driven by incoming note/MIDI
// events. CLAP defines `instrument` as a plug-in that processes note events and
// then produces audio; pairing it with `audio-effect` describes processors such
// as MIDI-triggered gates, glitch effects, vocoders, and other hybrid effects
// which retain a normal audio input/output path.
//
// This is intentionally *not* a note-effect profile: note-effect is for plug-ins
// that process or generate note events. Consumers should only add NOTE_EFFECT if
// they actually expose a note output / sequencing role as well.
inline constexpr std::array<const char*, 4> stereoMidiControlledAudioEffect {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_INSTRUMENT,
    CLAP_PLUGIN_FEATURE_STEREO,
    nullptr,
};
} // namespace nullclap::pluginFeatures
