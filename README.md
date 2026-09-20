# Runner Monitor

**Project copyright:** © 1993-2026 Shannon Smith

Runner Monitor is a cross-platform desktop monitor for GitHub Actions self-hosted runners. It shows runner connectivity and busy state, resolves active workflow jobs, records session history, exports CSV data, and can inspect the local runner service on Windows and Linux.

Runner Monitor 1.1 continues the native Infiltrator migration without discarding the proven monitor UI. The Windows and Linux launchers still share the same `runnerscope.py` application, while packaged Linux builds add a native C bridge linked to an exact Infiltratr Common release. Common 1.19.10 owns the complete current semantic palette, typography/metrics contract and durable atomic publication used for configuration/history; GitHub credentials remain owned by the authenticated `gh` CLI.

### Compatibility naming

The user-facing product remains **Runner Monitor**. The Debian/APT package identity is `infiltrator-runner-monitor` so it cannot collide with a future distribution package, while the existing `runnerscope` executable, desktop identity and per-user configuration paths remain stable. Existing `runnerscope` package installations migrate through the central Infiltrator repository transition package; the release asset may retain its `runnerscope_...deb` filename without changing the package identity stored inside the Debian archive.

## Engineering ethos

What does a runner monitor built from first principles need to own so that a change in a helper tool does not redefine what the application means? Runner Monitor treats GitHub's runner and workflow state as input, while session history, interpretation, presentation and local-runner behaviour remain project-owned.

The current application is partway through a native migration, so Python, the authenticated GitHub CLI and platform services are still practical adapters. They are not intended to become semantic sources of truth. Critical behaviour is moved into first-party native code when doing so makes the contract clearer or more dependable, while proven parts are not rewritten merely for fashion.

The objective is a monitor whose state transitions and history can be explained and tested independently of incidental command output. New dependencies are justified by stronger reliability or maintainability, not by novelty alone.

## Appearance

Runner Monitor supports **Follow system**, **Day** and **Night**. Day uses the white Infiltrator palette; Night uses the MB graphite/black palette with the canonical `#00ADEF` blue accent. Follow system detects the host light/dark preference and selects exactly Day or Night, updating while the application is running rather than inheriting a third toolkit palette.

The desktop shell follows the same operations-console hierarchy as the rest of the Infiltrator family: a compact product header, left navigation rail, semantic status cards, layered graphite work surfaces, dedicated selection actions and a low-noise status footer. Normal interface copy uses the Common UI/brand typography roles; technical values stay in structured tables rather than forcing the whole application into a terminal aesthetic.

## Features


- Live organisation runner status: running, idle and offline
- Active workflow/job discovery, including current step where GitHub exposes it
- Parallel active-job scanning with repository caching for much faster startup and refresh
- Mercedes graphite/silver UI with MB Corpo fonts when installed, plus safe platform fallbacks
- Self-hosted versus GitHub-hosted activity counters
- Runtime, queue time and per-runner session history
- Search/filter across monitoring tables
- CSV export
- Local runner health and `_diag` discovery
- Local runner service restart with confirmation before interrupting an active job
- First-run configuration dialog
- No GitHub token stored by Runner Monitor
- Shared graphite/silver interface on Windows and Linux\n- Native C/Common bridge on packaged Linux builds\n- Common-owned semantic theme roles and durable atomic config/history writes

## Requirements

### Windows

- Python 3.10 or newer with Tkinter
- GitHub CLI (`gh`)

### Linux

- Python 3.10 or newer
- Tkinter (`python3-tk` on Debian/Ubuntu/Linux Mint)
- GitHub CLI (`gh`)
- `systemd` for local runner service health/restart features
- `pkexec` or `sudo` if you want to restart a local runner from the GUI

Authenticate GitHub CLI before starting Runner Monitor:

```text
gh auth login
```

Organisation runner access may require the authenticated account/token to have the appropriate organisation permissions.

## Running

Windows:

```text
python runnerscope_windows.py
```

Linux:

```text
python3 runnerscope_linux.py
```

You can also run the shared core directly:

```text
python runnerscope.py
```

## First-run configuration

On first launch Runner Monitor asks for the GitHub organisation and monitoring preferences, then writes a local configuration file. It does not put the user's organisation, runner names, machine paths, or authentication credentials into the source tree.

Default config locations:

- Windows: `%APPDATA%\RunnerScope\config.json`
- Linux: `$XDG_CONFIG_HOME/runnerscope/config.json`, or `~/.config/runnerscope/config.json`

A safe `config.example.json` is included only as a reference. `config.json` and `state.json` are explicitly ignored by Git.

The configuration can be changed later with the **Settings** button. Restart Runner Monitor after changing polling settings.

Environment variables can override local config values when needed:

- `GITHUB_RUNNER_ORG`
- `GITHUB_RUNNER_REFRESH`
- `GITHUB_RUNNER_ACTIVITY_REFRESH`
- `GITHUB_RUNNER_REPO_LIMIT`
- `GITHUB_RUNNER_HISTORY`
- `GITHUB_RUNNER_EXPECTED`
- `GITHUB_RUNNER_LOCAL_HEALTH_REFRESH`

## Self-test

```text
python runnerscope.py --self-test
```

## Privacy and credentials

Runner Monitor shells out to the installed GitHub CLI. Authentication remains in GitHub CLI's own credential storage. Runner Monitor does not ask for, store, or publish a GitHub token.

The local config contains monitoring preferences and the organisation name only. It is stored outside the repository by default.

## Licence

Copyright © 1993-2026 Shannon Smith.

Runner Monitor is free software licensed under the GNU General Public License v3.0 or later. See `LICENSE`.

GitHub and GitHub Actions are trademarks of GitHub, Inc. Runner Monitor is an independent project and is not affiliated with or endorsed by GitHub, Inc.
