# Architecture

## Purpose

`null-clap` is the thin host-facing layer shared by bespoke plug-ins. Its job is to translate CLAP lifecycle, events and extensions into small C++ facilities without becoming a DSP framework.

The intended dependency direction is:

```text
Bitwig / CLAP host
        |
        v
   null-clap
        |
        +---- parameter/event/state/audio+note-port plumbing
        +---- GUI hosting boundary
        +---- remote-control pages
        +---- reusable host-facing descriptor feature profiles
        |
        v
 consuming plug-in
        |
        +---- DSP / physics / sequencing / analysis
        +---- MIDI/note interpretation
        +---- application UI
        +---- plug-in-specific Bitwig integration
```

The consuming plug-in owns behavior. null-clap owns protocol.

## Layers

### Official CLAP API

Fetched from `free-audio/clap` and pinned in `CMakeLists.txt`. This is the ABI contract with the host.

### clap-helpers

Provides checked C++ glue for CLAP callbacks and host extension access. Debug builds use maximal checking and terminate on contract violations; release builds use minimal checking and ignore helper-level misbehaviour reports.

### null-clap core

`nullclap::Plugin` composes the reusable facilities:

- `ParameterStore`
- `AudioPorts`
- `NotePorts`
- `RemoteControls`
- state serialization
- toolkit-neutral `GuiDelegate`
- a fixed-capacity GUI-to-host parameter event queue
- sample-offset segmentation of `process()`

`PluginFeatures.hpp` additionally provides explicit descriptor feature profiles. These are discovery/routing metadata, not runtime behavior. They are opt-in because a plug-in having a note port does not by itself make that plug-in a CLAP note effect.

### Application plug-in

Subclass `nullclap::Plugin`, register parameters/audio ports/note ports/pages in the constructor, and implement `processAudio()`. Raw events are available through `onEvent()`.

`NotePorts` only describes the event endpoints and supported CLAP dialects. It does not decide what a MIDI CC, note, MPE gesture or MIDI 2.0 message means. That interpretation remains application behavior.

## Host-facing feature profiles

A conventional stereo audio processor can use `nullclap::pluginFeatures::stereoAudioEffect`.

For a processor that remains fundamentally an audio effect but also consumes the host's note/MIDI signal as first-class realtime input, `nullclap::pluginFeatures::stereoAudioEffectWithNoteInput` advertises both `audio-effect` and `note-effect`, plus `stereo`.

This hybrid profile is intentionally explicit. Some hosts use descriptor categories when constructing their device/note routing topology independently of the `CLAP_EXT_NOTE_PORTS` declaration. A consuming plug-in should choose the hybrid profile only when it genuinely needs host note/MIDI routing while operating as an audio processor. null-clap does not infer descriptor categories from note ports.

## Process timeline

CLAP events carry a sample offset inside each audio block. null-clap never flattens those offsets into one block-level parameter snapshot.

For a block with events at samples 117 and 305:

```text
0 ---------------- 117 ---------------- 305 ---------------- N
| processAudio()   | apply event        | apply event        |
| params state A   | processAudio()     | processAudio()     |
|                  | params state B     | params state C     |
```

`processAudio(process, start, end)` therefore sees parameter state that is valid for exactly that span.

Non-parameter events, including raw MIDI events received through a note port, are forwarded through `onEvent()` at their original CLAP event time after the preceding audio span has been processed. `tests/MidiInputTests.cpp` exercises that contract with a raw channel-16 CC event through a real `clap_process_t` event list.

Event handlers that need block-level information such as the initial transport structure may call the protected `currentProcess()` accessor. It returns the host's raw `clap_process_t` only while the current `process()` call is active. null-clap deliberately does not wrap that transport/process context in a parallel abstraction.

This is a central framework invariant. Do not move parameter event handling to a timer, GUI callback, or once-per-block polling path.

## Extension policy

Stable standard CLAP extensions are preferred. The initial framework implements:

- params
- state
- audio-ports
- note-ports
- remote-controls
- gui when a delegate is present

More extensions should be added when a real plug-in needs them. Audio-port activation/configuration and richer state contexts are obvious future candidates, but are intentionally not speculative v0.1 surface area.

## What is deliberately absent

- voice allocation
- polyphonic modulation state
- MIDI interpretation
- transport abstractions
- preset database/discovery
- DSP utilities
- JUCE dependency
- Bitwig controller-extension transport

The raw CLAP event hook and process-context accessor make these possible without forcing premature generic designs.
