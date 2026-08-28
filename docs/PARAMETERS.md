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

Use `value(id)` when you need the base value and `effectiveValue(id)` in DSP when modulation should affect the sound.

## Registration

Parameters are registered during plug-in construction, before host initialization:

```cpp
constexpr auto mixId = nullclap::stableId("my-plugin.mix");

auto mix = nullclap::ParameterSpec::continuous(
    mixId, "Mix", "Main", 0.0, 1.0, 0.5);
mix.unit = "%";
parameters().add(std::move(mix));
```

Choice parameters use native integer values and CLAP's stepped/enum flags:

```cpp
parameters().add(nullclap::ParameterSpec::choice(
    nullclap::stableId("my-plugin.mode"),
    "Mode", "Main", { "Clean", "Wild", "Broken" }, 0));
```

## Stable IDs

`stableId()` is FNV-1a over a namespaced string. Treat the input string as persisted schema. Renaming UI text is harmless; changing `"my-plugin.mix"` changes the ID and therefore breaks automation/state identity.

For long-lived plug-ins, define IDs in one central header rather than scattering string literals.

## Sample accuracy

Host parameter events are consumed at their CLAP `header.time`. `Plugin::process()` divides the audio block into spans so `processAudio()` never straddles a parameter event with stale state.

No smoother is imposed by the framework. A plug-in decides whether a parameter should step, ramp, slew or drive a physical model.

## GUI gestures

A GUI should use:

```cpp
beginParameterGesture(id);
setParameterFromGui(id, value);
endParameterGesture(id);
```

The local base value changes immediately. A bounded lock-free queue then emits CLAP gesture/value events through `process()` or `params.flush()` when the host provides an output event list. `paramsRequestFlush()` is requested after enqueueing.

This keeps host automation semantics intact without touching the audio thread with GUI locks.

## Read-only/internal parameters

Set `CLAP_PARAM_IS_READONLY` for host-visible telemetry. Internal code can update it with `ParameterStore::setInternalValue()` or `Plugin::emitParameterValue()` from the process callback. Telemetry should normally set `persistent = false`.

`emitParameterValue()` defaults to `CLAP_EVENT_DONT_RECORD` so meters/state outputs do not become accidental automation.

## Polyphonic modulation

v0.1 handles global parameter value/modulation events. If a parameter event targets a specific note/port/channel/key, `ParameterStore` intentionally does not fold it into the monophonic state. The raw event still reaches `Plugin::onEvent()`.

When a real plug-in needs per-note modulation, add a voice-aware facility based on concrete requirements rather than extending `ParameterStore` with an imaginary voice model.
