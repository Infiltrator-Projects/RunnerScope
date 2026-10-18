# Architecture

## Product architecture

RunnerScope is a native C application on every supported desktop platform.

- `src/linux_main.c` is the shipping Linux desktop application.
- `src/windows_main.c` is the shipping Windows desktop application.
- `src/core/runner_config.c` is the toolkit-neutral configuration schema, validation and serialisation authority.
- `src/core/json.c` owns strict bounded JSON decoding and the one process-wide cJSON parsing lock for monitoring and sign-in.
- `src/platform/linux_storage.c` owns Linux configuration/history paths, bounded reads, validation and private atomic publication, without a GTK dependency.
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

RunnerScope calls GitHub directly using the access token saved through Settings. GitHub remains authoritative for runner/workflow facts; RunnerScope owns correlation, timing, history, presentation and local-service policy. There are no child-process provider or service adapters.

The shared provider validates JSON and projects runners, repositories, runs and jobs into the existing controller format. Pagination only accepts HTTPS links to `api.github.com`, has page/byte bounds and discards the complete fetch if any page fails. Native transports verify certificates, refuse redirects, bound response size and timeout, honor the controller deadline and cancellation, and omit credentials from diagnostics. cJSON is bundled at an exact recorded source revision.

Workers consume immutable organisation/token snapshots. Superseded results cannot update a view after Settings changes. Provider errors retain last-known observations as stale and make current counts unavailable rather than describing old data as idle.

## Configuration boundary

`RunnerConfig` is the shared configuration authority for both platforms. It owns defaults, validation, duplicate-key rejection, JSON interpretation and deterministic serialisation. Platform storage owns file paths and durable publication. Settings edits a complete candidate configuration so changing the organisation or token preserves other preferences.

GitHub device authorization supplies the session internally; Settings displays only its one-time browser code. The session is stored in the existing per-user JSON file. Linux uses owner-only file permissions; Windows applies a current-user/SYSTEM ACL before writing temporary files. Both publish the validated document atomically. Legacy documents without a session remain compatible and require browser sign-in through Settings. Corrupt and unreadable configuration is surfaced.

## Common boundary

Common is consumed directly, not through a helper process. It provides product-neutral primitives such as theme/design contracts, formatting, timing and safe output helpers.

The `infiltratr-common` gitlink is the single exact dependency authority. CMake may verify that the populated submodule equals that gitlink, but it must not maintain a second expected Common version or commit literal. Runner-specific semantics remain in RunnerScope.

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

Configuration/history are user data and must remain migration-compatible across native releases. The stable RunnerScope storage identity is retained for upgrade continuity even though the user-facing product name is RunnerScope.

Windows configuration publication writes a complete validated document to a temporary file, flushes it, and replaces the prior file atomically. Read, parse and publication errors are surfaced rather than silently replaced.

Linux storage retains the stable paths and uses Common durable-write primitives.
Its platform module consumes the same configuration parser as Windows. Settings
reads are limited to 64 KiB, history to 4 MiB and 300 retained entries. Nonregular
files and symlinks are rejected without blocking. Unreadable/corrupt history is
reported and never overwritten by a new empty session; new events remain in
memory. Publication failures remain visible and can be retried by later writes.

Local service observation has independent fresh/stale/error state. GitHub state
is correlated only through a complete canonical organisation/runner unit name,
never a substring match. Custom or escaped unit identities that cannot be proved
remain uncorrelated.

## Build and release ownership

`main` is the only retained and working branch. `VERSION` owns the RunnerScope version. The Common gitlink owns the Common revision. The single CI workflow owns qualification and release publication.

Linux packages and Windows executables are published only when the exact artifact uploaded for release has passed the platform's runtime/contract qualification. Release publication is not selected by commit-message convention and does not rebuild the product in a second workflow.

The shared authentication controller owns existing-session qualification, output validation and organisation discovery. The official GitHub CLI is supplied at a pinned release identity as a sign-in broker only. Native platform adapters own hidden process creation, private temporary configuration, bounded output, deadlines and cancellation. Neither platform searches PATH or requests developer registration. The native UI copies a valid challenge code and opens the browser automatically, then saves/connects on completion. Generation checks discard results after cancellation, organisation changes or window closure. The monitoring provider still performs its own HTTPS requests and JSON interpretation. See [GitHub sign-in](OAUTH.md).

Release CI also verifies the live beta and alpha APT indexes and downloads the published package to compare it with the qualified GitHub release digest. A GitHub release alone does not complete publication.

## Build seams

`CMakeLists.txt` defines the shared libraries and dependency identity.
`cmake/NativeDesktop.cmake` owns platform targets and install layout;
`cmake/RuntimeAssets.cmake` consumes Common font identity and the pinned broker;
`cmake/Tests.cmake` owns test registration and respects `BUILD_TESTING`.
`RUNNERSCOPE_BUILD_DESKTOP=OFF` removes desktop/platform dependencies and asset
downloads from core-contract builds. Archive downloads are checksum-verified in
partial files before entering the reusable cache.

Release jobs serialize publication on each ref. `support/release/state.sh`
distinguishes a published release from an interrupted draft at the same commit,
so reruns resume publication while refusing to reuse a released version for a
different revision. The native installer installs the declared Git build
dependency, rejects incompatible PGO profiles and respects Debian `nocheck`.
