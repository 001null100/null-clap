# State

CLAP hosts rely on the plug-in state extension to restore parameter values. null-clap therefore treats state as part of the framework rather than leaving every plug-in to invent a serializer.

## Format v1

All integers are written little-endian. Doubles are serialized by their IEEE-754 64-bit representation.

```text
4 bytes  magic "NCLP"
u32      format version (= 1)
u32      parameter count
u32      extra-state byte count

repeat parameter count:
    u32  clap parameter id
    u64  double value bits

N bytes  plug-in-specific extra state
```

The extra-state payload is capped at 16 MiB as a defensive bound.

## Parameter migration

State stores `(id, value)` pairs, not parameter indexes. Unknown IDs are skipped on load. Missing parameters keep their current/default value. This lets parameters be reordered freely and permits additive schema evolution.

Changing a stable parameter ID is still a breaking state migration.

## Host synchronization after load

Restoring state changes the framework's persistent parameter values directly. A CLAP host may cache parameter values, so a successful load must also tell the host to query those values again.

`nullclap::Plugin::stateLoad()` therefore calls `paramsRescan(CLAP_PARAM_RESCAN_VALUES)` after both the framework parameter state and the plug-in-specific extra state have loaded successfully. Do not remove this notification or defer it into the audio callback. It is part of the state restoration contract and is covered by `clap-validator`'s state reproducibility tests.

## Extra application state

Override:

```cpp
std::vector<std::byte> saveExtraState() const;
bool loadExtraState(std::span<const std::byte> bytes);
```

Use this only for state that is not naturally a CLAP parameter: editor topology, serialized tables, user data, etc. Keep ordinary knobs in `ParameterStore` so automation, remote controls and state share one source of truth.

`loadExtraState()` is transactional from the framework's point of view: if it returns `false`, the overall CLAP state load reports failure and the host rescan is not issued. Plug-ins should validate their extra payload before committing application-specific changes when practical.

## Version changes

If the framework state layout changes, increment `state::formatVersion` and add an explicit backward reader. Do not reinterpret old bytes under a new meaning.
