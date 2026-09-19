# Changelog

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
