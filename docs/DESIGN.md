# Design

## First-principles position

Runner Monitor asks what must be owned for a runner-monitoring application to remain dependable even if a helper command, UI toolkit or provider response changes.

The project therefore separates provider facts from application interpretation and is progressively moving critical behaviour into first-party native code.

## Goals

- accurate runner connectivity/busy/active-job interpretation;
- explicit unavailable/error state rather than plausible guesses;
- durable per-user configuration and session history;
- local runner health that is distinct from cloud/provider state;
- no application-owned GitHub token in the compatibility product;
- a native target with minimal external runtime dependencies;
- cross-platform behaviour defined by shared product contracts rather than toolkit quirks.

## Migration philosophy

Rewriting is not automatically improvement. The Python/Tk implementation is retained while it is the proven product. Native code replaces a capability only when it is at least as correct and maintainable and has regression evidence.

The intended end state is first-party native code because it gives the project stronger ownership of protocol/state semantics and reduces dependence on command-output/toolkit/runtime changes.

## Provider semantics

GitHub is the authority for GitHub runner/workflow facts. Runner Monitor is the authority for how those facts are cached, correlated, timed, displayed and stored.

A missing API field is not inferred from neighbouring fields unless the inference is explicitly defined and tested.

## Local service safety

Restarting a local runner is an action, not telemetry. It must remain separate from passive monitoring and should require explicit operator intent, particularly when a job may be active.

## Persistence

History/configuration writes use durable publication. Corrupt or unreadable state should fail explicitly or fall back through a documented recovery path; silent truncation is not acceptable.

## Dependency rule

Common may provide genuinely generic primitives. Runner Monitor-specific HTTP/provider/TLS/service behaviour must not be pushed into Common merely to reduce local code.

## UI rule

Presentation should consume the same runner/job/session model on each platform. Toolkit or native-shell differences must not redefine the meaning of running, idle, offline, queued or active.
