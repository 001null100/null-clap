# null-clap

A small C++20 framework for building bespoke, CLAP-first audio plugins.

`null-clap` exists to keep host/plugin plumbing out of individual projects. It wraps the official CLAP API and `clap-helpers`, then provides the reusable pieces that keep recurring in personal Bitwig plugins: plugin lifecycle, parameters, sample-accurate event dispatch, state, audio-port descriptions, remote-control pages, GUI delegation, and validator-backed builds.

The framework deliberately does **not** contain DSP, physics, synthesis, sequencing, or application-specific UI. A plugin should own its musical behavior; `null-clap` owns the boring contract with the host.

## Design goals

- **CLAP-native.** CLAP is the source architecture, not a compatibility target.
- **Bitwig-first without Bitwig lock-in.** Use standard CLAP facilities wherever possible. Companion Bitwig extensions remain plugin-specific when cross-device integration is genuinely required.
- **Sample-accurate parameter handling.** Automation and modulation events are applied at their CLAP sample offsets before processing the corresponding audio span.
- **Realtime-safe hot path.** No allocation, locks, filesystem access, or host calls from the audio-processing path unless CLAP explicitly permits them.
- **Small abstractions.** The framework should remove boilerplate without hiding CLAP semantics.
- **Stable IDs and reproducible state.** Parameter identity is explicit and state files are versioned.
- **Validation is part of the build.** The example plugin is exercised by `clap-validator` in CI.

## Current scope

The initial framework provides:

- `nullclap::Plugin`, a reusable CLAP plug-in base built on `clap-helpers`.
- `ParameterStore` with stable IDs, native ranges, automation values, monophonic modulation and GUI-originated gestures.
- Sample-offset event segmentation through the plug-in process callback.
- Versioned binary parameter state save/load.
- Declarative audio-port lists.
- Declarative CLAP remote-control pages.
- A toolkit-agnostic GUI delegate interface, suitable for a JUCE-component adapter in a consuming plug-in.
- `SinglePluginFactory` and a compact entry-point helper.
- A minimal stereo gain effect proving the full lifecycle.
- Windows/Linux CI plus `clap-validator` validation.

Polyphonic parameter modulation is intentionally **not** abstracted yet. The core event is surfaced to plug-ins so the first project that genuinely needs per-note state can drive the design instead of us inventing a voice framework in advance.

## Quick start

```cmake
include(FetchContent)

FetchContent_Declare(
    null_clap
    GIT_REPOSITORY https://github.com/001null100/null-clap.git
    GIT_TAG main
)
FetchContent_MakeAvailable(null_clap)

target_link_libraries(MyPlugin PRIVATE nullclap::nullclap)
```

For real plug-ins, pin `GIT_TAG` to a commit rather than following `main`.

See [`docs/ADDING_A_PLUGIN.md`](docs/ADDING_A_PLUGIN.md) for the intended project structure and [`examples/MinimalEffect`](examples/MinimalEffect) for a complete implementation.

## Building this repository

Requirements:

- CMake 3.24+
- C++20 compiler
- Git

```bash
cmake -S . -B build -DNULLCLAP_BUILD_EXAMPLES=ON -DNULLCLAP_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The example output is a native `.clap` module. Platform-specific install locations and validator instructions are in [`docs/BUILDING.md`](docs/BUILDING.md).

## Dependency policy

Dependencies are pinned in the root `CMakeLists.txt` rather than floating silently:

- CLAP 1.2.10
- `clap-helpers` commit `c35dd4906bd8efbb900cb2b89e680fed463cc8b1`

The CLAP 1.x ABI is stable, but source APIs and helper behavior still evolve. Updating either dependency should be an explicit change followed by the complete test and validator pass.

## Documentation

- [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) - layers, ownership and extension philosophy
- [`docs/PARAMETERS.md`](docs/PARAMETERS.md) - automation, modulation, gestures and IDs
- [`docs/THREADING.md`](docs/THREADING.md) - realtime and host-thread contracts
- [`docs/STATE.md`](docs/STATE.md) - state format and migration rules
- [`docs/GUI.md`](docs/GUI.md) - toolkit-independent GUI boundary
- [`docs/BUILDING.md`](docs/BUILDING.md) - local builds, installation and validation
- [`docs/ADDING_A_PLUGIN.md`](docs/ADDING_A_PLUGIN.md) - recipe for a new bespoke plug-in
- [`AGENTS.md`](AGENTS.md) - explicit rules for coding agents working in this repository

## What belongs here?

Promote code into `null-clap` when at least one real plug-in needs a reusable CLAP concern. Keep application behavior in the application repository.

Good framework candidates: parameter transport, state I/O, GUI hosting, port declarations, host capability wrappers, reusable CLAP extension glue.

Bad framework candidates: Motion Engine physics, GlitchDeck buffers, a specific visual theme, a synth voice model, Bitwig mappings unique to one plug-in.

The framework is intentionally a thin bridge, not a new audio-programming universe.

## License

MIT. See [`LICENSE`](LICENSE).
