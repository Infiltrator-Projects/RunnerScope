# Changelog

## Unreleased

No unreleased changes.

## 1.2.10 - 2026-09-24

- Align the native Linux outer shell with the suite-wide 20 px screen-padding rhythm.
- Preserve runner monitoring behaviour, platform architecture, dependencies and Common APIs unchanged.

## 1.2.9 - 2026-09-24

- Align the native Linux product title with the 18 px publisher titlebar scale.
- Preserve monitoring behaviour, platform architecture, dependencies and Common APIs unchanged.


## 1.2.8 - 2026-09-24

- Align native Linux generic button corners with the suite-wide 6 px compact control radius.
- Preserve runner monitoring, provider behaviour, data models, dependencies and Common APIs unchanged.

## 1.2.7 - 2026-09-24

- Tighten native Linux notebook tabs to the 5 px / 10 px publisher tab rhythm.
- Preserve monitoring behaviour, data models, dependencies and Common APIs unchanged.

## 1.2.6 - 2026-09-24

- Align native Linux footer actions with the 30 px publisher desktop control height.
- Preserve monitoring behaviour, layout structure, dependencies and Common APIs unchanged.

## 1.2.5 - 2026-09-24

- Bring the native Linux product heading onto the 28 px publisher desktop title scale.
- Preserve monitoring behaviour, provider access, layout structure, dependencies and Common APIs.


## 1.2.4 - 2026-09-24

- Replace the native Linux shell's private font choice with the already-linked Common typography contract.
- Use Common's UI family for normal controls and its brand family for the product title without changing layout, monitoring behaviour or dependencies.

## 1.2.3 - 2026-09-24

- Align runner summary counter corners with the suite-wide 6 px compact radius.
- Keep runner/provider behaviour, dependencies and native platform architecture unchanged.

## 1.2.2 - 2026-09-23

- Restore a conventional Linux application menu bar with File, Edit, View and Help menus.
- Add View → Theme with Follow system, Day and Night, backed by the same persisted Common theme mode already used by Settings.
- Make Follow system react to live GTK host-theme changes instead of requiring a restart or settings round-trip.
- Add Help → About Runner Monitor with application version, Common version, project identity, author, website and licence details.
- Replace the crowded one-line footer with a dedicated action row plus a separate status/version row, keeping selection-specific actions grouped apart from general actions.
- Restyle footer actions with Common palette roles so enabled and disabled buttons remain legible in Night mode.
- Use the dedicated Runner Monitor application icon for the native Linux window and About dialog.

## 1.2.1 - 2026-09-23

- Advance the exact Infiltratr Common dependency from 1.19.10 to released Common 1.19.24.
- Inherit the intervening shared design, formatting, POSIX and graphics hardening while preserving Runner Monitor's native C/GTK and Win32 product architecture.
- Keep the dependency exact through the committed gitlink and qualify both native platforms plus the clean Debian install path before publication.

## 1.2.0 - 2026-09-21

- Restore native C as the shipping Runner Monitor architecture.
- Restore the substantial native Linux C/GTK monitor and link it directly to pinned Common 1.19.10.
- Add a native Win32 C/Common executable and publish a real Windows EXE.
- Remove Python/Tk product entry points, launchers, tests and the obsolete out-of-process Common helper bridge.
- Install the native Linux executable directly as `/usr/bin/runnerscope`; remove Python/Tk package dependencies.
- Publish only qualified native Debian and Windows EXE artifacts.
- Show the linked Common version directly in native application footers.
- Keep the stricter `src/native2/` first-party work as an incremental dependency-minimisation track rather than allowing it to displace the native product.
- Add native Linux/Windows CI and package guards that reject a return to Python/Tk shipping artifacts.


## 1.1.13 - 2026-09-21

- Show the exact qualified Common version in standalone Windows/Linux Python builds even when the native Common bridge is absent.
- Preserve native bridge and source-tree Common version discovery when available.
- Add regression coverage that forces the no-helper path and forbids the vague "Common compatibility" footer.

## 1.1.12 - 2026-09-21

- Put the Runner Monitor version above the Common version in the sidebar footer.
- Keep the Common version as the final bottom line and protect that layout with a regression contract.

## 1.1.11 - 2026-09-21

- Preserve repository-cache and history-retention settings instead of resetting hidden values when Settings is saved.
- Paginate organisation runners, repositories, workflow runs and jobs; unresolved busy runners now search the full organisation repository set.
- Replace full Tk table reconstruction with stable in-place row updates, add semantic counter filtering, meaningful duration/history sorting and display-faithful CSV export.
- Make GitHub access testing asynchronous, reduce system-theme polling, clear hidden-tab actions and re-check live provider busy state immediately before local runner restart.
- Debounce durable history writes, report configuration/history persistence failures explicitly, and migrate the earlier Windows state path without discarding history.
- Batch Linux systemd service inspection and make diagnostic-log sampling tolerant of files changing during refresh.
- Harden the first-party native runner model, length-bounded HTTP parsing and private configuration reads, with deterministic regression coverage.
- Run normal validation on hosted Linux and Windows, including a clean Debian build/install/smoke/purge path, and derive build/release metadata from VERSION and the pinned Common submodule.

## 1.1.10 - 2026-09-20

- Reproduce MBLINK's low-alpha semantic surfaces in Tk by preblending Common graphite with the exact accent/state roles.
- Lock the product header to the same 44px-class height used by Infiltrator Software.
- Make selected navigation use the MBLINK cyan-tinted surface and border treatment rather than a flat selection block.
- Tint summary cards subtly by semantic state while keeping the graphite base dominant.
- Clarify state meaning: running/self-hosted activity cyan, healthy idle green, GitHub-hosted activity blue, queued amber, offline/fault red.
- Repair the UI regression contract to validate the blended navigation surface.

## 1.1.9 - 2026-09-20

- Retune Runner Monitor to the full Common/MBLINK Night hierarchy instead of using mostly generic dark surfaces.
- Match Infiltrator Software's 44px-class titlebar composition: centred product title/subtitle with About, live Theme and Refresh controls.
- Use the canonical cyan accent for selected navigation, focus and primary actions; selected-summary cyan for highlighted state; green/success borders for healthy states; amber/warning borders for queued and restart actions; red for faults/offline.
- Move context, detail and status strips onto the MBLINK connection/connection-border roles for clearer graphite layering.
- Preserve native window-manager chrome on the Python/Tk compatibility UI rather than replacing it with fragile custom move/resize code.
- Add regression coverage for titlebar controls and the MBLINK semantic role mappings.

## 1.1.8 - 2026-09-20

- Replace the generic monitor-screen artwork with a dedicated workflow/pipeline glyph so Runner Monitor is immediately distinguishable from System Monitor.
- Keep the stable `runnerscope` desktop/icon identity while updating both SVG and PNG package artwork.
- Ensure the installed menu icon, app-install icon and repository-derived Software artwork all originate from the same Runner Monitor asset.

## 1.1.7 - 2026-09-20

- Replace the dense utility-style window with the shared Infiltrator operations-console shell used across the desktop family.
- Add a left navigation rail for Runners, Active jobs, History and Local service while preserving the existing notebook-backed behaviour internally.
- Present runner and activity totals as semantic graphite cards with Common success/info/warning/fault roles instead of terminal-like counter chips.
- Move refresh/appearance actions into the product header, selection actions into a dedicated details bar, and export/status into a compact footer.
- Increase table spacing and hierarchy while keeping technical data in structured tables rather than turning the whole application into monospace.
- Advance the native/Common bridge from Common 1.19.6 to released Common 1.19.10 at `33e69c0a462b56d388881d89c4eb49f72fa0b0fe`.
- Expose and consume the full 1.19.10 semantic appearance roles instead of stopping at the older base palette.
- Standardise Runner Monitor artwork on the non-automotive Infiltrator icon family with canonical `#00ADEF` linework.

## 1.1.6

- Unified Follow system, Day and Night with the Common appearance contract.
- Day uses the white palette; Night uses the MB graphite/black palette with #00ADEF.
- Repaired dynamic theme refresh and tree recolouring so live OS theme changes cannot recurse or crash the UI.

## 1.1.2

- Renamed the application from RunnerScope to Runner Monitor.
- Kept the existing `runnerscope` package, executable, configuration paths and desktop WM class for upgrade compatibility.
- Kept all runner monitoring, job resolution, history and local-service functions unchanged.


## 1.0.3

- Restores the full rich runner monitor UI/data model from the proven Windows baseline.
- Restores State for, session job count, busy percentage, resolving busy-runner state, detailed session summaries, sortable columns and clickable counters.
- Keeps the fast 2-second runner-state poll and immediately requests job resolution when a runner becomes busy.
- Uses the graphite/silver theme consistently in setup, settings and the main monitor.
- Keeps one shared Windows/Linux code path while adding Linux local service health and restart support.
- Keeps organisation, runner names and credentials out of the published source.

## 1.0.0 - 2026-09-04

Initial public release of RunnerScope.

- Shared Windows/Linux monitoring core
- First-run local configuration
- GitHub CLI authentication without storing tokens
- Self-hosted runner state and active workflow/job monitoring
- Local Windows service and Linux systemd health/restart support
- CSV export and session history
