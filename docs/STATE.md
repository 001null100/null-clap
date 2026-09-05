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

The extra-state payload is capped at 16 MiB. The parameter count is capped at 1,048,576 records to bound allocation and parsing work. These are defensive limits, not new fields: existing valid v1 states remain compatible.

## Parameter migration

State stores `(id, value)` pairs, not parameter indexes. Unknown IDs are skipped on load. Missing parameters keep their current/default value. Repeated IDs use the last value. This permits additive schema evolution and parameter reordering; changing a stable parameter ID is still a breaking migration.

Writable parameters always participate in host state. Read-only telemetry can opt out with `persistent = false`.

## Parsing and failure behavior

`state::load()` validates and stages the entire parameter list and extra payload before applying anything. Truncated data, unsupported versions, excessive counts, non-finite values, stream errors, or allocation failure return `false` without changing framework parameters or the caller's extra-state vector.

Streams may return fewer bytes than requested; the reader/writer loops until the operation completes. Zero progress, negative results, and byte counts larger than the requested transfer fail. Stream callbacks must obey the CLAP C ABI and must not throw exceptions or write outside the supplied buffer.

The public `state::save()` and `state::load()` functions catch their own allocation failures inside their `noexcept` boundaries. A failed save can leave a partial stream; the host must discard a save for which `false` was returned.

## Extra application state

Override:

```cpp
std::vector<std::byte> saveExtraState() const;
bool loadExtraState(std::span<const std::byte> bytes);
```

Use this for state that is not naturally a CLAP parameter: editor topology, serialized tables, user data, etc. Keep ordinary knobs in `ParameterStore` so automation, remote controls and state share one source of truth.

`Plugin::stateLoad()` preserves the existing hook contract: newly restored parameter values are visible inside `loadExtraState()`. If that hook returns `false` or throws, the framework restores the previous persistent parameter values and reports failure. This is a failure rollback, not a cross-thread atomic snapshot.

The framework cannot roll back opaque application objects. Validate and stage the extra payload before committing application-specific changes; a rejecting or throwing hook must not leave its own objects partially changed. It must not register/remove parameters during loading.

## Host synchronization after load

After both framework state and application state load successfully, `Plugin::stateLoad()` calls `paramsRescan(CLAP_PARAM_RESCAN_VALUES)` so the host refreshes cached parameter values. Failed loads do not issue that rescan. Keep this notification on the state/main-thread path, not in DSP.

## Version changes

If the framework state layout changes, increment `state::formatVersion` and add an explicit backward reader. Do not reinterpret old bytes under a new meaning.
