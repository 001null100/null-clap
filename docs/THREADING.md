# Threading and realtime rules

CLAP assigns callbacks to main, audio, background, or other host threads. `clap-helpers` checks many contracts, especially in Debug builds, but application code must also remain realtime-safe.

## Audio thread

The realtime path includes `Plugin::process()`, `processAudio()`, `onEvent()` when called from process, parameter application, and `emitParameterValue()`.

Do not allocate, lock mutexes, access files, perform network I/O, sleep, routinely log, or construct heavyweight objects here.

`ParameterStore` uses a prebuilt ID map and atomics with no allocation after registration. GUI parameter delivery uses a fixed 256-slot SPSC ring (255 usable events), not a mutex-backed queue. A drain snapshots its work budget and removes an event only after the host accepts it. A producer refilling the ring cannot make one drain unbounded.

## Main/UI thread

Construction, parameter registration, port/page declarations, GUI creation, and normal GUI interaction live here. Registration must be complete before audio processing; never grow the parameter/port/page containers concurrently with callbacks.

GUI parameter methods have one producer. If a future toolkit writes from multiple threads, add an explicit multi-producer design rather than assuming these calls support it. They return `false` on producer overflow, and callers must retain/retry rejected requests, especially gesture ends. A rejected value edit does not change its local parameter value.

The ring has one consumer at a time: `process()` or `params.flush()`, following CLAP's nonconcurrent scheduling contract. The consumer may run on the audio thread while active or the main thread while inactive.

## State callbacks

State save/load may allocate and are not realtime operations. The reader stages a bounded state before committing it. The extra-state hook sees restored parameter values; if it rejects or throws, framework parameters are rolled back. Application objects need their own validation-before-commit discipline.

Rollback on failure does not make a complete preset a cross-thread atomic snapshot. Follow the host's state callback scheduling and coordinate application-owned shared data appropriately. Never add audio-thread locks to make state loading appear synchronous.

## Atomics

Parameter base and monophonic modulation amounts are atomics so the GUI can inspect individual host-driven values without an audio-thread lock. Their relaxed memory ordering does not establish ownership or consistency of larger structures.

The SPSC ring publishes producer writes with release/acquire ordering and does not reuse a slot until the consumer has accepted its event. The retry flag coalesces host callbacks; it is not a general-purpose task queue.

## Host calls

Only call a host function from threads its CLAP declaration permits. Main-thread helpers include `requestGuiResize()` and `markStateDirty()`.

GUI enqueueing requests `paramsRequestFlush()` from its non-audio producer path. If an output event is rejected during processing, the consumer uses the thread-safe `requestCallback()` and sets a retry flag. `Plugin::onMainThread()` then requests the flush before invoking the application's `onMainThreadCallback()`. It never requests a parameter flush directly from the audio thread.

When the host does not expose its params extension, the producer/main-thread retry requests processing instead. A host that supplies no usable output list retains ownership of scheduling future delivery; the queue remains intact.

Plug-in-specific host extension calls should stay explicit until a concrete reusable pattern emerges.
