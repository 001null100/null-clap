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

### Application plug-in

Subclass `nullclap::Plugin`, register parameters/audio ports/note ports/pages in the constructor, and implement `processAudio()`. Raw events are available through `onEvent()`.

`NotePorts` only describes the event endpoints and supported CLAP dialects. It does not decide what a MIDI CC, note, MPE gesture or MIDI 2.0 message means. That interpretation remains application behavior.

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

Non-parameter events, including raw MIDI events received through a note port, are forwarded through `onEvent()` at their original CLAP event time after the preceding audio span has been processed.

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
