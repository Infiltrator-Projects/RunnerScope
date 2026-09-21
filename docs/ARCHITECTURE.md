# Architecture

## Product architecture

Runner Monitor is a native C application on every supported desktop platform.

- `src/linux_main.c` is the shipping Linux desktop application.
- `src/windows_main.c` is the shipping Windows desktop application.
- both link directly to the exact Common revision pinned by the repository;
- neither shipping target uses Python or Tk/Tkinter;
- the Linux package installs the native ELF executable directly as `/usr/bin/runnerscope`;
- the Windows release publishes a native Win32 EXE.

The previous Python compatibility shell and native helper bridge were removed after the native product path was restored.

## Provider boundary

The current shipping product uses GitHub CLI as its authenticated provider adapter. GitHub remains authoritative for runner/workflow facts; Runner Monitor owns correlation, timing, history, presentation and local-service policy.

The long-term dependency-minimisation work in `src/native2/` is deliberately separate from the shipping architecture. It may replace the current provider/helper boundary only after it reaches behavioural parity. It must never cause the product to fall back to Python again.

## Common boundary

Common is consumed directly, not through a helper process. It provides product-neutral primitives such as theme/design contracts, formatting, timing and safe output helpers.

Runner-specific semantics remain in Runner Monitor.

## Platform presentation

Linux uses GTK 3 for the native desktop shell. Windows uses Win32 controls and system APIs. Toolkit differences must not redefine the meaning of runner states.

Both native shells surface the application version and linked Common version directly.

## State model

Provider state and local service state are separate observations. A runner can remain registered with GitHub while its local service is unhealthy, or local service metadata can exist while provider data is unavailable.

Unavailable provider data is represented as unavailable/error state, not guessed state.

## Persistence

Configuration/history are user data and must remain migration-compatible across native releases. The stable RunnerScope storage identity is retained for upgrade continuity even though the user-facing product name is Runner Monitor.
