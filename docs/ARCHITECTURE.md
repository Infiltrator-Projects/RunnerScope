# Architecture

## Purpose

Runner Monitor is a cross-platform desktop monitor for GitHub Actions self-hosted runners. It observes organisation runner connectivity/busy state, resolves active workflow jobs, records session history, exports data and can inspect/restart a local runner service where the platform permits it.

The project is in a deliberate transition from a Python/Tk application toward a first-party native implementation. Architecture therefore distinguishes the current compatibility product from the native target rather than pretending the migration is already complete.

## Current compatibility application

`runnerscope.py` owns the shared current application behaviour. `runnerscope_linux.py` and `runnerscope_windows.py` are platform launch/integration surfaces.

The compatibility application currently relies on:

- Python/Tkinter for the UI/runtime;
- the authenticated GitHub CLI for provider authentication/data access;
- platform service-management mechanisms for local-runner inspection/restart.

Those dependencies are current implementation facts, not the intended long-term semantic source of truth.

## Native bridge

`native/runnerscope_native.c` is the packaged native bridge linked against exact pinned Common. It provides project-owned native functionality already used alongside the compatibility application, including Common-backed theme/persistence contracts where applicable.

## First-party native core

`src/native2/` is the independent native rewrite:

- `model.c/.h` — runner/job/session state model;
- `http.c/.h` — first-party HTTP/provider transport layer;
- `secure_config.c/.h` — configuration/credential boundary;
- `linux_local.c/.h` — Linux local runner/service integration;
- `main.c` — native composition and self-test entry point.

The target is intentionally restricted to first-party C plus Common and normal libc/POSIX/Linux interfaces.

## Dependency boundary

The native target explicitly does **not** use GTK/GLib, Qt, Tk/Tkinter, Python, GitHub CLI, libcurl, OpenSSL-family libraries, GnuTLS, NSS or a second shared Infiltrator library.

This is not a claim that those technologies are generally wrong. It is a project decision to own this monitor's provider protocol, HTTPS/TLS path, configuration and local-runner integration directly so that helper-tool output cannot redefine product behaviour.

## Common boundary

Common may supply product-neutral parsing, formatting, timing, design and durable I/O primitives. Runner-specific GitHub semantics, local-runner state, provider protocol and application policy stay here.

## State model

Provider state and local service state are independent observations. A runner can be registered with GitHub while its local service is unhealthy, or local service metadata can exist while provider state is unavailable.

The UI/model therefore preserves:

- provider connectivity/status;
- busy/idle state;
- active workflow/job identity when resolvable;
- local service state;
- history/session records;
- error/unavailable states.

Unavailable provider data is not converted into a guessed offline/busy state.

## Persistence

Configuration and history are durable application data. Persistence must be atomic at the file-publication boundary and migration-compatible across the Python/native transition.

## Migration rule

The native rewrite must earn replacement capability by capability. It should not become the default merely because it compiles. Behaviour, saved data and failure semantics must remain compatible or have an explicit migration.
