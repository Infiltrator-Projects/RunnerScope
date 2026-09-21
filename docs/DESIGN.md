# Design

## First-principles position

Runner Monitor should remain understandable, testable and native even while dependencies are progressively reduced.

The application therefore separates provider facts from application interpretation and keeps Common limited to genuinely shared primitives.

## Goals

- compiled native C binaries on Linux and Windows;
- direct, pinned Common integration;
- accurate runner connectivity/busy/active-job interpretation;
- explicit unavailable/error states rather than plausible guesses;
- durable per-user configuration/history;
- local runner health distinct from provider state;
- platform-native presentation without semantic drift;
- progressive dependency reduction without replacing working native code with an interpreted compatibility shell.

## Provider semantics

GitHub is authoritative for GitHub runner/workflow facts. Runner Monitor owns caching, correlation, timing, presentation and persistence.

The current release line uses GitHub CLI as the authenticated provider adapter. `src/native2/` exists to remove that dependency only when the replacement is at least as correct.

## Common usage

Common owns shared palette, design metrics, typography identity, formatting and other product-neutral helpers. Runner Monitor links it directly into each native binary.

Runner-specific HTTP/provider/service policy must not be moved into Common merely to reduce local code.

## UI rule

Linux and Windows may use different native presentation APIs, but running, idle, offline, queued and active must mean the same thing.

The footer must show the Runner Monitor version followed by the linked Common version, with Common as the final line.

## Local service safety

Restarting a runner is an action, not telemetry. It remains separate from passive monitoring and requires explicit operator intent.

## Persistence

Configuration/history writes must use durable publication appropriate to the platform. Corrupt or unreadable state must be surfaced rather than silently replaced.
