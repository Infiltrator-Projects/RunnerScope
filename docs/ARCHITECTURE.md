# Architecture

## Product architecture

Runner Monitor is a native C application on every supported desktop platform.

- `src/linux_main.c` is the shipping Linux desktop application.
- `src/windows_main.c` is the shipping Windows desktop application.
- `src/core/runner_config.c` is the toolkit-neutral configuration schema, validation and serialisation authority introduced in 1.3.8.
- `src/platform/windows_config_store.c` owns Windows configuration storage and atomic publication.
- `src/platform/windows_process.c` owns bounded Windows child-process execution/capture.
- both applications link directly to the exact Common revision recorded by the repository gitlink;
- neither shipping target uses Python or Tk/Tkinter;
- the Linux package installs the native ELF executable directly as `/usr/bin/runnerscope`;
- the Windows release publishes a native Win32 EXE.

The previous Python compatibility shell and native helper bridge were removed after the native product path was restored. A second experimental product/core is not an acceptable refactoring mechanism.

## Provider boundary

The current shipping product uses GitHub CLI as its authenticated provider adapter. GitHub remains authoritative for runner/workflow facts; Runner Monitor owns correlation, timing, history, presentation and local-service policy.

Windows provider execution is now an explicit platform boundary. `windows_process.c` captures output without blocking indefinitely on the child pipe, applies a real wall-clock timeout, terminates an overrun process and reports timeout separately from ordinary non-zero exit status. The Win32 controller passes an immutable organisation snapshot to each refresh worker, so Settings changes cannot race the request being executed.

Linux still performs provider discovery from the shipping controller. Issue #14 remains the authority for extracting the corresponding Linux provider snapshot/process boundary without creating a parallel application path.

## Configuration boundary

`RunnerConfig` is the extracted application configuration contract. The core module owns defaults, field validation, duplicate-key rejection, JSON interpretation and deterministic serialisation for migrated consumers. Windows consumes it now; Linux still has an inline compatibility implementation that issue #14 must remove before there is one repository-wide authority. Platform storage code owns only where bytes live and how they are durably published.

The Windows Settings surface edits the organisation while retaining the complete parsed `RunnerConfig`; saving no longer reconstructs unrelated fields from hard-coded defaults. Corrupt Windows configuration is surfaced and is not silently converted into a plausible valid document.

Linux configuration storage is still implemented inline in `linux_main.c`. Migrating that controller to the same core contract is the next configuration slice under issue #14. Until then, the core contract is independently tested on every platform so the schema authority exists before the Linux inline parser is deleted.

## Common boundary

Common is consumed directly, not through a helper process. It provides product-neutral primitives such as theme/design contracts, formatting, timing and safe output helpers.

The `infiltratr-common` gitlink is the single exact dependency authority. CMake may verify that the populated submodule equals that gitlink, but it must not maintain a second expected Common version or commit literal. Runner-specific semantics remain in Runner Monitor.

## Platform presentation

Linux uses GTK 3 for the native desktop shell. Windows uses Win32 controls and system APIs. Toolkit differences must not redefine the meaning of runner states.

The 1.3.x architecture is removing duplicated application structure through one toolkit-neutral declarative UI contract and two native renderers. The shared contract owns page hierarchy, component identity, application-level actions/state bindings and migrated presentation semantics. GTK and Win32 own native widget/control creation, drawing, event-loop integration and other platform mechanics.

The governing rule is: **application structure lives once; platform mechanics live twice.**

This is intentionally a small product abstraction, not a replacement widget toolkit. Platform-specific extensions are permitted only when explicit, isolated and tested. The migration plan is documented in [UI_CONTRACT_MIGRATION.md](UI_CONTRACT_MIGRATION.md) and tracked in issue #2.

Invalid shared-contract inputs fail closed. An invalid page or platform is not silently converted into a plausible Runners/Linux view.

Both native shells surface the application version and linked Common version directly.

## State model

Provider state and local service state are separate observations. A runner can remain registered with GitHub while its local service is unhealthy, or local service metadata can exist while provider data is unavailable.

Unavailable, unknown, invalid and failed observations remain distinct. Provider or persisted data is never guessed merely to keep the interface populated.

## Persistence

Configuration/history are user data and must remain migration-compatible across native releases. The stable RunnerScope storage identity is retained for upgrade continuity even though the user-facing product name is Runner Monitor.

Windows configuration publication writes a complete validated document to a temporary file, flushes it, and replaces the prior file atomically. Read, parse and publication errors are surfaced rather than silently replaced.

Linux history/configuration persistence still uses the shipping GTK controller plus Common durable-write primitives. Issue #14 covers the remaining Linux read/error semantics and controller extraction.

## Build and release ownership

`VERSION` owns the Runner Monitor version. The Common gitlink owns the Common revision. The single CI workflow owns qualification and release publication.

Linux packages and Windows executables are published only when the exact artifact uploaded for release has passed the platform's runtime/contract qualification. Release publication is not selected by commit-message convention and does not rebuild the product in a second workflow.
