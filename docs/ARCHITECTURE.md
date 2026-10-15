# Architecture

## Product architecture

Runner Monitor is a native C application on every supported desktop platform.

- `src/linux_main.c` is the shipping Linux desktop application.
- `src/windows_main.c` is the shipping Windows desktop application.
- `src/core/runner_config.c` is the toolkit-neutral configuration schema, validation and serialisation authority.
- `src/platform/windows_config_store.c` owns Windows configuration storage and atomic publication.
- `src/core/github.c` owns GitHub JSON interpretation, projections, pagination and failure semantics.
- `src/platform/linux_http.c` and `windows_http.c` own native HTTPS transport.
- `src/platform/linux_services.c` owns systemd D-Bus observation and explicit restart.
- both applications link directly to the exact Common revision recorded by the repository gitlink;
- neither shipping target uses Python or Tk/Tkinter;
- the Linux package installs the native ELF executable directly as `/usr/bin/runnerscope`;
- the Windows release publishes a native Win32 EXE.

The previous Python compatibility shell and native helper bridge were removed after the native product path was restored. A second experimental product/core is not an acceptable refactoring mechanism.

## Provider boundary

Runner Monitor calls GitHub directly using the access token saved through Settings. GitHub remains authoritative for runner/workflow facts; Runner Monitor owns correlation, timing, history, presentation and local-service policy. There are no child-process provider or service adapters.

The shared provider validates JSON and projects runners, repositories, runs and jobs into the existing controller format. Pagination only accepts HTTPS links to `api.github.com`, has page/byte bounds and discards the complete fetch if any page fails. Native transports verify certificates, refuse redirects, bound response size and timeout, and omit credentials from diagnostics. cJSON is bundled at an exact recorded source revision.

Workers consume immutable organisation/token snapshots. Superseded results cannot update a view after Settings changes. Provider errors retain last-known observations as stale and make current counts unavailable rather than describing old data as idle.

## Configuration boundary

`RunnerConfig` is the shared configuration authority for both platforms. It owns defaults, validation, duplicate-key rejection, JSON interpretation and deterministic serialisation. Platform storage owns file paths and durable publication. Settings edits a complete candidate configuration so changing the organisation or token preserves other preferences.

The token is masked in Settings and stored in the existing per-user JSON file. Linux uses owner-only file permissions; Windows applies a current-user/SYSTEM ACL before writing temporary files. Both publish the validated document atomically. Legacy documents without a token remain compatible and require setup through Settings. Corrupt and unreadable configuration is surfaced.

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

Linux configuration storage and history retain the stable paths and use Common durable-write primitives. Linux configuration now consumes the same core parser as Windows. Further controller decomposition can proceed without changing those contracts.

## Build and release ownership

`main` is the only retained and working branch. `VERSION` owns the Runner Monitor version. The Common gitlink owns the Common revision. The single CI workflow owns qualification and release publication.

Linux packages and Windows executables are published only when the exact artifact uploaded for release has passed the platform's runtime/contract qualification. Release publication is not selected by commit-message convention and does not rebuild the product in a second workflow.
