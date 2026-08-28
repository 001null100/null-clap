# Agent instructions

This repository is shared infrastructure for bespoke CLAP plug-ins. Treat changes here as API changes that may affect every consuming plug-in.

## Core rules

1. **CLAP semantics stay visible.** Do not build abstractions that erase the difference between base parameter values, modulation amounts, automation gestures, process events, flush events, or host callbacks.
2. **No application logic.** Physics, glitch algorithms, synth voices, sequencers, MIDI interpretation, visual themes, and Bitwig mappings belong in consuming repositories.
3. **No realtime hazards.** `Plugin::process()`, `processAudio()`, and event handlers must not allocate, lock mutexes, touch files, log routinely, perform network I/O, or call host functions that are not explicitly audio-thread safe.
4. **Stable IDs are contracts.** Never casually change an existing plug-in parameter ID, audio-port ID, note-port ID, or remote-page ID. State, routing and automation depend on them.
5. **Prefer standard stable CLAP extensions.** Draft extensions require a concrete need, explicit documentation, and tests.
6. **Do not add a generic DSP framework.** null-clap is host plumbing, not an audio engine.
7. **Keep dependencies pinned.** Dependency upgrades are standalone changes and must pass the full CI/validator matrix.

## Before changing public headers

- Read `docs/ARCHITECTURE.md` and the relevant subsystem document.
- Check whether the need is truly shared by more than one plug-in or is an unavoidable CLAP concern.
- Preserve source compatibility where it is cheap. If not, document the break.
- Add or update a minimal example/test that demonstrates the behavior.

## Parameter changes

- Use native/plain units, not normalized 0..1 values, unless the actual parameter is inherently normalized.
- `ParameterStore::value()` is the base value.
- `ParameterStore::modulation()` is the current monophonic modulation amount.
- `effectiveValue()` is clamped `base + modulation`.
- Polyphonic events are currently intentionally *not* consumed by the framework. Surface them through `Plugin::onEvent()` until a real voice-aware plug-in provides requirements.
- Read-only telemetry parameters should set `persistent = false` unless there is a clear reason to restore them.

## Note and MIDI ports

- Port declaration belongs in the framework; MIDI/note meaning belongs in the consuming plug-in.
- Prefer `NotePortSpec::midi()` when the application consumes ordinary `CLAP_EVENT_MIDI` messages.
- Do not advertise dialects the application cannot actually interpret.
- Keep note-port configuration fixed while active, as required by CLAP.

## Threading

Follow `docs/THREADING.md`. In particular, do not replace the fixed GUI parameter ring buffer with a mutex-backed queue. Do not make state save/load realtime operations.

## Validation

A change is not finished until:

- Windows build succeeds.
- Linux build succeeds.
- unit tests pass.
- `NullClapMinimalEffect.clap` passes `clap-validator`.

If CI finds a CLAP contract violation, fix the contract rather than suppressing the validator test unless the validator is demonstrably wrong.

## Documentation

When adding a reusable facility, update the appropriate document and `docs/ADDING_A_PLUGIN.md` if the normal plug-in recipe changes. Comments should explain CLAP/threading reasons, not narrate obvious C++.
