# Parameters

## Plain/native values

null-clap stores CLAP parameter values in their real units. A gain parameter can be `-60..12 dB`; a radius can be `0.05..1.0`; a mode can be integer choices. Do not normalize everything to 0..1 merely because a GUI slider likes normalized coordinates.

`ParameterStore` maintains two pieces of runtime state per parameter:

```text
base value        automation / user / state value
modulation amount CLAP modulation offset
----------------------------------------------
effective value   clamp(base + modulation)
```

Use `value(id)` for the base value and `effectiveValue(id)` in DSP when modulation should affect sound. Non-finite modulation amounts are rejected without replacing the previous amount. Runtime non-finite base values retain the existing fallback-to-default behavior.

## Registration

Register parameters during construction, before host initialization:

```cpp
constexpr auto mixId = nullclap::stableId("my-plugin.mix");
auto mix = nullclap::ParameterSpec::continuous(
    mixId, "Mix", "Main", 0.0, 1.0, 0.5);
mix.unit = "%";
parameters().add(std::move(mix));
```

`add()` returns `false` for an invalid/duplicate ID, empty name, reversed range, or a non-finite minimum, maximum, or default. Finite defaults are clamped and quantized to the declared range. Check registration failures rather than silently assuming a parameter exists. Registration may allocate; it is never an audio-thread operation.

Choice parameters use native integer values and CLAP's stepped/enum flags:

```cpp
parameters().add(nullclap::ParameterSpec::choice(
    nullclap::stableId("my-plugin.mode"),
    "Mode", "Main", { "Clean", "Wild", "Broken" }, 0));
```

## Stable IDs

`stableId()` is FNV-1a over a namespaced string. Treat the input string as persisted schema. Renaming UI text is harmless; changing `"my-plugin.mix"` changes the ID and breaks automation/state identity. For long-lived plug-ins, define IDs in one central header.

## Sample accuracy

Host parameter events are consumed at their CLAP `header.time`. `Plugin::process()` divides the audio block into spans so `processAudio()` never straddles a parameter event with stale state. Valid timestamps are in `[0, frames_count)`; an event at `frames_count` is outside the block.

No smoother is imposed by the framework. A plug-in decides whether a parameter should step, ramp, slew or drive a physical model. Structurally truncated known core events are not passed to application event handlers; valid unknown/custom event types remain available.

## GUI gestures and backpressure

A GUI uses `beginParameterGesture(id)`, `setParameterFromGui(id, value)`, and `endParameterGesture(id)` from one main/UI producer thread. Each returns whether the request was accepted. Accepted value edits update the local base value immediately and queue a CLAP output event.

The queue has 256 storage slots, 255 usable. If it is full, the request returns `false`; a rejected value edit does not change the local parameter. The UI must retain/retry rejected requests, particularly gesture-end requests. Retrying must happen from the producer thread, not a blocking loop on the audio thread. Read-only parameters reject all three GUI methods.

When the host's output list rejects an event, the framework keeps it and subsequent events in FIFO order. A main-thread callback requests another parameter flush; the framework never calls the non-audio `request_flush` API from DSP. If the host has no params extension, GUI edits request processing instead. A missing output list retains queued events until a usable list is supplied.

Each drain processes only the queue length observed at entry. A concurrently active producer cannot extend an audio callback into an unbounded drain loop. Enqueueing and delivery remain allocation-free and mutex-free. The framework does not guarantee immediate delivery when the host continually rejects output.

## Read-only/internal parameters

Set `CLAP_PARAM_IS_READONLY` for host-visible telemetry. Internal code can update it with `ParameterStore::setInternalValue()` or `Plugin::emitParameterValue()` from the process callback. Telemetry should normally set `persistent = false`.

`emitParameterValue()` defaults to `CLAP_EVENT_DONT_RECORD`. Its sample offset must be strictly less than the current block's frame count. Out-of-range calls return `false` without changing the local telemetry value. A valid call can still return `false` if the host rejects output; unlike queued GUI edits, generated telemetry is not automatically retried, and its local value can already have changed.

## Polyphonic modulation

The framework handles global parameter value/modulation events. Events targeting a specific note/port/channel/key are intentionally not folded into monophonic state; valid raw events still reach `Plugin::onEvent()`.

Add voice-aware facilities only when a real consuming plug-in provides requirements rather than extending `ParameterStore` with an imaginary voice model.
