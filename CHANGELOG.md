# Changelog

## Unreleased

No unreleased changes.

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
