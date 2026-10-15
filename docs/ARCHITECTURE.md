# Architecture

## Product architecture

Runner Monitor is a native C application on every supported desktop platform.

- `src/linux_main.c` is the shipping Linux desktop application.
- `src/windows_main.c` is the shipping Windows desktop application.
- both link directly to the exact Common revision recorded by the repository gitlink;
- neither shipping target uses Python or Tk/Tkinter;
- the Linux package installs the native ELF executable directly as `/usr/bin/runnerscope`;
- the Windows release publishes a native Win32 EXE.

The previous Python compatibility shell and native helper bridge were removed after the native product path was restored. A second experimental product/core is not an acceptable refactoring mechanism.

## Provider boundary

The current shipping product uses GitHub CLI as its authenticated provider adapter. GitHub remains authoritative for runner/workflow facts; Runner Monitor owns correlation, timing, history, presentation and local-service policy.

There is no parallel provider implementation in the repository. Provider work is made directly against the shipping native applications so behaviour cannot drift between a product path and an experimental replacement.

The current platform controllers still contain too much provider/process/configuration/persistence policy inline. Issue #14 defines the migration of those concerns into explicit owned boundaries. That work must replace existing policy incrementally; it must not create another long-lived implementation.

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

Durable write helpers already come from Common where applicable. Issue #14 covers the remaining duplicated parsing and read/error semantics so corrupt or unreadable state is surfaced deliberately rather than being silently replaced by defaults.

## Build and release ownership

`VERSION` owns the Runner Monitor version. The Common gitlink owns the Common revision. The single CI workflow owns qualification and release publication.

Linux packages and Windows executables are published only when the exact artifact uploaded for release has passed the platform's runtime/contract qualification. Release publication is not selected by commit-message convention and does not rebuild the product in a second workflow.
