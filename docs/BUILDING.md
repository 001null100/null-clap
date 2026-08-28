# Building and validation

## Requirements

- CMake 3.24+
- a C++20 compiler
- Git
- internet access on first configure so CMake can fetch pinned CLAP dependencies

## Configure and build

### Windows / Visual Studio

```powershell
cmake -S . -B build -DNULLCLAP_BUILD_EXAMPLES=ON -DNULLCLAP_BUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The example plug-in is emitted to:

```text
build/clap/NullClapMinimalEffect.clap
```

### Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DNULLCLAP_BUILD_EXAMPLES=ON -DNULLCLAP_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## CLAP installation locations

Common host search paths include:

- Windows: `%COMMONPROGRAMFILES%\\CLAP` or `%LOCALAPPDATA%\\Programs\\Common\\CLAP`
- Linux: `~/.clap` or `/usr/lib/clap`

For development, Bitwig can also discover plug-ins from paths configured in its plug-in locations.

## Validator

CI pins `clap-validator` 0.4.1. Locally:

```text
clap-validator validate path/to/Plugin.clap --only-failed
```

The validator is not optional ceremony. It catches lifecycle, parameter, state, audio-port and realtime-contract errors that a successful compiler cannot.

## Dependency upgrades

The root CMake pins exact CLAP and clap-helpers commits. To upgrade:

1. change one dependency intentionally;
2. read its changelog;
3. build Windows and Linux;
4. run unit tests;
5. run the full validator;
6. test at least one consuming plug-in in Bitwig before considering the upgrade settled.

Do not casually replace the pins with `main`.
