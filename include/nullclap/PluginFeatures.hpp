#pragma once

#include <array>
#include <clap/plugin-features.h>

namespace nullclap::pluginFeatures
{
// Conventional stereo audio effect with no host-routing implication beyond audio.
inline constexpr std::array<const char*, 3> stereoAudioEffect {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_STEREO,
    nullptr,
};

// Explicit opt-in profile for processors which are fundamentally audio effects but
// also consume the host's note/MIDI signal as a first-class realtime input.
//
// Some hosts use descriptor categories when constructing their note-routing graph,
// independently of the note-ports extension. Advertising both categories keeps the
// audio-effect identity while making the note-input role visible at discovery time.
// Do not use this profile merely because a plug-in exposes a note port for an
// incidental purpose: CLAP's NOTE_EFFECT category is host-facing classification.
inline constexpr std::array<const char*, 4> stereoAudioEffectWithNoteInput {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_NOTE_EFFECT,
    CLAP_PLUGIN_FEATURE_STEREO,
    nullptr,
};
} // namespace nullclap::pluginFeatures
