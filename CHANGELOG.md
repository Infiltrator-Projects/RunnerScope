# Changelog

## Unreleased

No unreleased changes.

## 1.3.3 - 2026-10-06

- Move page-transition policy into one toolkit-neutral shared plan consumed by both native shells.
- Unify real-change/no-op decisions, Runners search/table visibility, selection clearing and page-triggered Activity/Local Health refresh policy.
- Keep actual GTK notebook switching and Win32 show/hide/repaint operations native to their platforms.
- Add toolkit-free tests covering Runners, Active Jobs, History and Local Health transitions plus invalid/no-op cases.
- Complete the navigation/page-switch policy tranche and retain Common 1.19.38.

## 1.3.2 - 2026-10-06

- Move the product header and general footer chrome into the shared native-renderer translation layer.
- Emit product/family identity and the ordered Settings, Minimize, Maximize/Restore and Close header actions once from toolkit-neutral C.
- Emit Export CSV, About and Refresh footer actions once, including the Refreshing state plus shared status/application/Common version slots.
- Keep GTK and Win32 native: GTK creates native controls and Win32 draws native GDI surfaces from the same shared emission.
- Add toolkit-free tests for header/footer action order, identity, refresh-state labelling and status/version propagation.
- Leave contextual footer actions, page switching and deeper workspace composition for later small 1.3.x slices; retain Common 1.19.38.

## 1.3.1 - 2026-10-06

- Add the first true native-renderer adapter seam to the 1.3 shared UI architecture.
- Emit the left navigation once from toolkit-neutral C, including page order, Local Health separator placement, selected-page state and platform-appropriate labels.
- Make GTK and Win32 translate that same navigation contract into their own native controls/drawing instead of independently owning the product structure.
- Add toolkit-free renderer tests proving the Linux and Windows projections receive the same navigation structure, with only the intentional Local Health platform label difference.
- Keep header/footer, workspace composition, runner cards/tables and the remaining functional-parity work for later small 1.3.x migration slices.
- Retain Common 1.19.38 and the full Linux/Windows/Debian qualification gates.

## 1.3.0 - 2026-10-06

- Introduce the first toolkit-neutral Runner Monitor UI contract shared by the native GTK and Win32 products.
- Give the four pages and major shell surfaces stable component IDs so cross-platform parity can be tested independently of either toolkit.
- Make both native shells consume shared product identity, page/navigation metadata, hero artwork, metric captions and contextual action policy.
- Add a toolkit-free UI-contract CTest and validate the same contract from both native application self-tests.
- Run the full Linux, Windows build/runtime and clean Debian qualification suite on pull requests as well as `main`, so future drift is caught before merge.
- Keep GTK and Win32 native and preserve existing configuration/history data and Common 1.19.38 integration.

## 1.2.39 - 2026-10-06

- Replace the legacy Windows table-only frontend with the native Infiltrator OS shell used by the current product generation.
- Embed the same navigation/hero artwork and Common-verified MB Corpo fonts directly in the Windows executable.
- Add the Windows runner-card fleet view, Cards/Table switching, filtering, selection details, settings, CSV export, metrics and session utilisation.
- Add a clean hosted Windows x64 runtime/self-test qualification alongside the self-hosted x64 release build.

## 1.2.38 - 2026-10-03

- Make bitmap runner cards the default Linux fleet view, with semantic state indicators and bars showing session utilisation.
- Show current jobs on running cards and full runner metadata on selection; retain the sortable table and complete CSV export.
- Preserve card widgets and selection across refreshes, and filter using all runner metadata.
- Remove repeated state counters from the footer and add an installed GTK UI contract test to clean-package CI.
- Retain the detailed bitmap artwork, mandatory MB Corpo fonts and Common 1.19.38 pin.

## 1.2.37 - 2026-10-03

- Replace all four navigation identities, page banners and the sidebar's wireframe pictures with detailed raster hardware artwork.
- Display the bitmap identities at readable sizes and reuse unchanged images during telemetry updates.
- Retain the verified MB Corpo fonts and exact Common 1.19.38 pin.

## 1.2.36 - 2026-10-03

- Make the Common-verified MB Corpo UI, bold and brand faces mandatory for the native Linux package rather than permitting silent system-font substitution.
- Add real product-local raster artwork for navigation, runner/activity/history/health hero surfaces and persistent Infiltrator OS identity.
- Replace long page prose with compact state kickers and move search below the hero so the graphical overview leads the workspace.
- Strengthen gradients, wells, card depth and state hierarchy without reverting the responsiveness improvements from 1.2.27-1.2.35.
- Verify installed MB typography and graphical assets during clean-package CI.
- Keep the exact current Infiltratr Common 1.19.38 / 7070c5812b50821fd7580101cb2289a3184f6b2c pin.

## 1.2.35 - 2026-10-03

- Restore the graphical Infiltrator OS visual language without reverting the 1.2.27-1.2.34 responsiveness work.
- Reintroduce deliberate gradients, depth, stronger shaped surfaces, icon wells and accent hierarchy in the header, workspace, navigation, metrics, selection surface and footer.
- Add a page-identity icon well to the workspace and icon-led footer actions so the application is not dominated by text and flat GTK controls.
- Keep fixed-height tables, detached model rebuilds, contextual provider scanning and all current performance protections intact.
- Keep the exact current Infiltratr Common 1.19.38 / 7070c5812b50821fd7580101cb2289a3184f6b2c pin.

## 1.2.34 - 2026-10-03

- Remove the dead activity-scan result field left after the old scan-summary UI was retired and replace the private theme persistence parser with Common's canonical parser/key API.
- Put Runner, Active Jobs, History and Local Health tables into GTK fixed-height/fixed-column mode and detach their models while rebuilding, avoiding repeated geometry work and per-row redraws during refresh/filter operations.
- Restrict the expensive repository/job sweep to Active Jobs or a Runner view with genuinely busy self-hosted runners; trigger it immediately when the fleet transitions into a busy state and skip it on History/Local Health.
- Ignore clicks on the already-selected navigation page, halve presentation-only duration/utilisation repaint frequency again, and widen the history-write debounce to reduce main-loop churn.
- Match the current Infiltrator OS/System Monitor navigation geometry exactly: 12 px selected controls, 7/9 px padding, 38 px icon wells and a standard separator before Local Linux Health.
- Keep the exact latest Infiltratr Common 1.19.38 / 7070c5812b50821fd7580101cb2289a3184f6b2c pin.

## 1.2.33 - 2026-10-03

- Remove the duplicate hero/counter summary left from the pre-standard Linux shell; the window now follows the current Infiltrator OS header → navigation → workspace → footer hierarchy without repeated fleet state surfaces.
- Remove the associated two-second global summary traversal/allocations and cache effective theme state so duplicate GTK theme notifications do not rebuild and reload the full CSS projection.
- Make manual refresh contextual and stop the expensive activity repository sweep while Local Linux Health is active; opening Active Jobs still refreshes it immediately.
- Enforce the same 3-second runner and 15-second local-health minimums in Settings that configuration loading already enforced, debounce filtering slightly longer, and give first paint more time before the initial activity sweep.
- Purge stale repository-cache/history configuration keys and documentation left from earlier implementations.
- Keep the exact latest Infiltratr Common 1.19.38 / 7070c5812b50821fd7580101cb2289a3184f6b2c pin.

## 1.2.32 - 2026-10-03

- Remove remaining superseded Linux shell CSS and move Settings into the same header control group used by current Infiltrator OS/System Monitor.
- Cut presentation churn in half by updating live duration/utilisation cells every two seconds instead of every second.
- Stop the Activity scan from forcing a redundant extra runner API poll after every repository sweep.
- Make Local Linux Health on-demand: refresh immediately when opened and poll it only while that workspace is visible, eliminating background systemctl and diagnostic-directory scans.
- Raise the minimum runner poll to 3 seconds and Local Health poll to 15 seconds to reduce process and filesystem churn without losing useful live visibility.
- Keep the exact latest Infiltratr Common 1.19.38 / 7070c5812b50821fd7580101cb2289a3184f6b2c pin.

## 1.2.31 - 2026-10-03

- Remove the redundant static page-header layer and legacy Help menubar so the Linux shell follows the current Infiltrator OS brand/header/navigation/workspace hierarchy.
- Move filtering into the active workspace and match System Monitor's responsive 195 px / 64 px navigation rail, 50 px navigation controls and 1100 px compact threshold.
- Reduce one-second GTK churn by updating only visible time-dependent cells, refreshing global utilisation every two seconds and avoiding repeated static Local Health metric scans.
- Defer the expensive activity/local startup scans until after first paint and stop eagerly rendering the hidden History model.
- Run privileged runner restarts off the GTK main thread so authentication and systemd latency no longer freeze the interface.
- Keep the exact latest Infiltratr Common 1.19.38 / 7070c5812b50821fd7580101cb2289a3184f6b2c pin.

## 1.2.30 - 2026-10-03

- Replace the temporary GtkListBox navigation workaround with the same compact toggle-button rail used by the current Infiltrator OS/System Monitor shell.
- Remove the remaining obsolete list-row navigation CSS, subtitle widgets and historical navigation helper code.
- Add direct runner/activity row indexes so one-second live-cell updates no longer perform repeated linear searches through backing arrays.
- Suspend presentation-only one-second repaints while the window is iconified; provider polling continues in the background and the display catches up immediately on restore.
- Keep the exact latest Infiltratr Common 1.19.38 / 7070c5812b50821fd7580101cb2289a3184f6b2c pin.

## 1.2.29 - 2026-10-03

- Remove the superseded first-pass GTK CSS layer so the Linux shell has one authoritative Common-driven style projection instead of stacked historical overrides.
- Match the current Infiltrator OS shell grammar with flat semantic surfaces, restrained selection, Common typography roles and no decorative shell gradients.
- Stop rebuilding visible runner, activity and local-service models when a refresh produces the same static data.
- Transfer worker-owned activity/local rows into the UI state instead of deep-copying every record after each scan.
- Release completed worker references without waiting on the GTK main thread.
- Keep the exact latest Infiltratr Common 1.19.38 / 7070c5812b50821fd7580101cb2289a3184f6b2c pin.


## 1.2.28 - 2026-10-03

- Align the Linux shell with the current Infiltrator OS titlebar, navigation, semantic palette and Common design metrics.
- Reuse one GTK CSS provider instead of stacking a new provider after each theme change.
- Update only the visible live table on one-second ticks and rebuild provider-backed models only when their page is visible.
- Avoid no-op label invalidation for counters, summary and workspace metrics.
- Restore the suite-standard 1280x800 desktop geometry and 214 px navigation rail.
- Remove retired Runner Monitor routing/probe artifacts.
- Advance the pinned Infiltratr Common dependency from 1.19.35 to current 1.19.38.

## 1.2.27 - 2026-10-01

- Stop rebuilding the runner and active-job GTK models every second; live time fields now update in place.
- Debounce search filtering and rebuild only the visible page.
- Batch persistent-history writes after runner state changes.
- Remove repeated local-runner correlation allocations.
- Remove the retired `src/native2/` parallel implementation, its duplicate tests/build targets and stale documentation.
- Keep one supported native implementation per platform.

## 1.2.18 - 2026-09-25

- Align the Linux default window with the 1220x780 publisher desktop geometry.
- Preserve runner discovery, monitoring behaviour, dependencies and Common APIs.

## 1.2.17 - 2026-09-25

- Align native Linux notebook tabs with the suite-wide 30 px desktop control height while retaining the existing compact radius and accent underline.
- Preserve monitoring behaviour, platform architecture, dependencies and Common APIs unchanged.

## 1.2.16 - 2026-09-25

- Use Common's 10 px control radius for native Linux runner summary counters instead of a private compact-radius literal.
- Preserve monitoring behaviour, platform architecture, dependencies and Common APIs unchanged.

## 1.2.15 - 2026-09-25

- Align native Linux runner summary-chip padding with the suite-wide 6 px / 10 px compact spacing rhythm.
- Preserve monitoring behaviour, platform architecture, dependencies and Common APIs unchanged.

## 1.2.14 - 2026-09-25

- Replace the filled selected notebook tab with the suite-wide restrained 2 px accent underline while retaining the existing compact tab geometry.
- Preserve runner monitoring behaviour, platform architecture, dependencies and Common APIs unchanged.

## 1.2.13 - 2026-09-25

- Give native Linux notebook tabs the same 6 px compact corner radius as the surrounding publisher chrome.
- Preserve runner monitoring behaviour, platform architecture, dependencies and Common APIs unchanged.


## 1.2.12 - 2026-09-25

- Align all native Linux buttons with the suite-wide 30 px desktop control height instead of limiting that height to footer actions.
- Preserve runner monitoring behaviour, platform architecture, dependencies and Common APIs unchanged.

## 1.2.11 - 2026-09-25

- Remove the native Linux typography override that accidentally shrank the 28 px product heading back to 18 px.
- Keep the 18 px publisher scale for titlebar chrome while restoring the intended 28 px in-page product heading.
- Preserve monitoring behaviour, platform architecture, dependencies and Common APIs unchanged.

## 1.2.10 - 2026-09-24

- Align the native Linux outer shell with the suite-wide 20 px screen-padding rhythm.
- Preserve monitoring behaviour, platform architecture, dependencies and Common APIs unchanged.

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
