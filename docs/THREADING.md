# Threading and realtime rules

CLAP explicitly assigns callbacks to main, audio, background, or other host threads. `clap-helpers` checks many of these contracts for us, especially in debug builds, but application code still has to remain realtime-safe.

## Audio thread

The following path must be realtime-safe:

- `Plugin::process()`
- `processAudio()`
- `onEvent()` when called from process
- parameter event application
- `emitParameterValue()`

Do not allocate, lock mutexes, access files, perform network I/O, sleep, routinely log, or construct heavyweight objects here.

`ParameterStore` performs no allocation after registration. Its audio-thread value lookups use a prebuilt ID map and atomics. The GUI queue is a fixed 256-event SPSC ring.

## Main/UI thread

Construction, parameter registration, audio-port declaration, remote-page declaration, GUI creation and normal GUI interaction live here.

The GUI-originated parameter methods are designed for a single producer thread. If a future UI toolkit genuinely writes parameters from multiple threads, do not silently convert the queue to a mutex. Add a deliberate multi-producer design and document the cost.

## State callbacks

State save/load may allocate. They are not part of the realtime process path. Extra plug-in state is represented as a byte vector for exactly this reason.

## Atomics

Parameter base and monophonic modulation values are atomics so the GUI can inspect current host-driven values without acquiring an audio-thread lock. They use relaxed memory ordering because individual scalar values do not establish ownership of larger structures.

Do not place dynamically resized structures behind these atomics and assume that makes them realtime-safe.

## Host calls

Only call a CLAP host callback from a thread permitted by that callback's specification. `nullclap::Plugin` currently wraps two common main-thread requests:

- `requestGuiResize()`
- `markStateDirty()`

GUI parameter enqueueing requests a host parameter flush from its main/UI path. Plug-in-specific host extension calls should remain explicit until a reusable pattern emerges.
