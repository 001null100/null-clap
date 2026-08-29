# Adding a bespoke plug-in

The expected shape of a consuming repository is deliberately small:

```text
MyPlugin/
    CMakeLists.txt
    Source/
        Plugin.hpp
        Plugin.cpp
        Entry.cpp
        Dsp/...
        UI/...
```

## 1. Fetch null-clap

```cmake
include(FetchContent)
FetchContent_Declare(
    null_clap
    GIT_REPOSITORY https://github.com/001null100/null-clap.git
    GIT_TAG <PINNED-COMMIT>
)
FetchContent_MakeAvailable(null_clap)
```

Then create the module:

```cmake
nullclap_add_plugin(MyPlugin
    OUTPUT_NAME MyPlugin
    SOURCES
        Source/Plugin.cpp
        Source/Entry.cpp
)
```

## 2. Define a plug-in class

```cpp
class MyPlugin final : public nullclap::Plugin
{
public:
    static const clap_plugin_descriptor_t& descriptor() noexcept;
    explicit MyPlugin(const clap_host_t* host);

private:
    void processAudio(const clap_process_t&,
                      std::uint32_t start,
                      std::uint32_t end) noexcept override;
    void onEvent(const clap_event_header_t&) noexcept override;
};
```

The constructor registers fixed parameters, audio ports, note ports and remote pages. No host callbacks are used from the constructor; CLAP host access becomes valid during `init()`.

## 3. Give everything stable IDs

```cpp
static constexpr auto mixId = nullclap::stableId("my-plugin.mix");
static constexpr auto inputId = nullclap::stableId("my-plugin.audio.main-in");
static constexpr auto midiInputId = nullclap::stableId("my-plugin.note.midi-in");
```

Keep the strings stable once projects can be saved with the plug-in.

## 4. Declare event inputs when you need them

A plug-in that expects raw MIDI must advertise a note port. For ordinary MIDI 1.0 input:

```cpp
notePorts().addInput(nullclap::NotePortSpec::midi(midiInputId, "MIDI Input"));
```

`NotePortSpec::midi()` advertises `CLAP_NOTE_DIALECT_MIDI` as both the supported and preferred dialect. Use `NotePortSpec::dialects()` only when the application genuinely understands additional CLAP note dialects.

The framework describes the port; the consuming plug-in decides what the incoming events mean.

### Descriptor roles for MIDI-controlled audio effects

Descriptor features are a separate host-facing contract from note ports. A conventional stereo audio effect can use:

```cpp
nullclap::pluginFeatures::stereoAudioEffect.data()
```

If the processor has a normal audio input/output path but its output is also driven by incoming note or MIDI events, use:

```cpp
nullclap::pluginFeatures::stereoMidiControlledAudioEffect.data()
```

That profile advertises `audio-effect`, `instrument`, and `stereo`. CLAP defines `instrument` as a plug-in which processes note events and produces audio, which covers MIDI-triggered gates, glitch processors, vocoders, and similar hybrid audio processors.

Do not use `note-effect` merely because a note port exists. CLAP reserves that role for plug-ins which process or generate note events. If an application also emits notes, it can add `CLAP_PLUGIN_FEATURE_NOTE_EFFECT` explicitly.

## 5. Process spans, not imaginary whole blocks

Implement `processAudio()` under the assumption that a parameter or other input event may have split the host block immediately before `start`. Query `effectiveValue()` inside the span and apply your own smoothing when appropriate.

## 6. Handle non-parameter events explicitly

Override `onEvent()` for note/MIDI/transport/custom events. The framework applies global parameter events first, then forwards every event to this hook at its original sample offset.

For a raw MIDI port, inspect `CLAP_EVENT_MIDI` and cast the header to `clap_event_midi_t`. MIDI interpretation is application logic and intentionally not hidden by null-clap.

Per-note parameter events also arrive here and are not consumed into the monophonic store.

## 7. Export the CLAP entry

`Entry.cpp`:

```cpp
#include "Plugin.hpp"
#include <nullclap/Entry.hpp>

NULLCLAP_DEFINE_ENTRY(MyPlugin);
```

The framework supplies the one-plug-in factory and defensive CLAP 1.2 entry init/deinit counting.

## 8. Add a GUI only if needed

Implement `nullclap::GuiDelegate` in the application repository. JUCE-based plug-ins should keep JUCE there, not add it to null-clap.

## 9. Validate before DAW testing

Every plug-in repository should copy the same principle as null-clap CI: compile, run local tests, then run `clap-validator` before publishing an artifact.

For a Bitwig-only personal plug-in, `.clap` is the primary artifact. Do not add VST3 merely out of habit.
