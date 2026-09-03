# RunnerScope

RunnerScope is a cross-platform desktop monitor for GitHub Actions self-hosted runners. It shows runner connectivity and busy state, resolves active workflow jobs, records session history, exports CSV data, and can inspect the local runner service on Windows and Linux.

The Windows and Linux launchers use the same `runnerscope.py` core. Platform-specific code is limited to local service discovery/restart and small operating-system integration details, so the two versions do not drift apart.

## Features

- Live organisation runner status: running, idle and offline
- Active workflow/job discovery, including current step where GitHub exposes it
- Self-hosted versus GitHub-hosted activity counters
- Runtime, queue time and per-runner session history
- Search/filter across monitoring tables
- CSV export
- Local runner health and `_diag` discovery
- Local runner service restart with confirmation before interrupting an active job
- First-run configuration dialog
- No GitHub token stored by RunnerScope
- Shared graphite/silver interface on Windows and Linux

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

Authenticate GitHub CLI before starting RunnerScope:

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

On first launch RunnerScope asks for the GitHub organisation and monitoring preferences, then writes a local configuration file. It does not put the user's organisation, runner names, machine paths, or authentication credentials into the source tree.

Default config locations:

- Windows: `%APPDATA%\RunnerScope\config.json`
- Linux: `$XDG_CONFIG_HOME/runnerscope/config.json`, or `~/.config/runnerscope/config.json`

A safe `config.example.json` is included only as a reference. `config.json` and `state.json` are explicitly ignored by Git.

The configuration can be changed later with the **Settings** button. Restart RunnerScope after changing polling settings.

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

RunnerScope shells out to the installed GitHub CLI. Authentication remains in GitHub CLI's own credential storage. RunnerScope does not ask for, store, or publish a GitHub token.

The local config contains monitoring preferences and the organisation name only. It is stored outside the repository by default.

## Licence

Copyright © 2026 Shannon Smith.

RunnerScope is free software licensed under the GNU General Public License v3.0 or later. See `LICENSE`.

GitHub and GitHub Actions are trademarks of GitHub, Inc. RunnerScope is an independent project and is not affiliated with or endorsed by GitHub, Inc.
