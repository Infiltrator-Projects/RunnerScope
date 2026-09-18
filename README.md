# RunnerScope

RunnerScope is a native C/GTK desktop monitor for GitHub Actions self-hosted runners.

Version 1.2.0 removes Python and Tkinter from the installed Linux product. The Linux application is now a native C11 executable linked directly with the exact pinned Infiltratr Common 1.19.2 release. GitHub CLI (`gh`) remains the authentication and API boundary, so RunnerScope does not store a GitHub token.

## Architecture

- Native C11 application and GTK 3 Linux UI
- Infiltratr Common 1.19.2 pinned at `44409af17c89b6ece6b4bcb2c0c133213c695c23`
- Common-owned System / Day / Night theme contract
- Common duration formatting and monotonic timing
- Common durable atomic config/history publication
- GitHub CLI-owned credentials and API transport
- No Python runtime dependency
- No Tkinter runtime dependency

The legacy Python implementation remains in the repository as historical/reference code while the installed Linux application is entirely native.

## Features

- Live organisation runner status: running, idle and offline
- Active workflow/job discovery across recently active repositories
- Self-hosted versus GitHub-hosted activity counters
- Runner state duration, session jobs and utilisation
- Search/filter across monitor tables
- CSV export
- Persistent history
- Local Linux systemd runner service health
- Local runner restart with confirmation via `pkexec`
- System / Day / Night theme modes sourced from Common
- First-run native GTK configuration dialog

## Linux requirements

Runtime dependencies are intentionally small:

- GTK 3 (already present on Linux Mint/Cinnamon)
- GitHub CLI (`gh`)
- systemd for local runner service health
- polkit/`pkexec` only for service restart

Authenticate before starting RunnerScope:

```text
gh auth login
```

## Install

```text
sudo apt install ./runnerscope_1.2.0_amd64.deb
```

Then launch RunnerScope from the application menu or run:

```text
runnerscope
```

## Build

```text
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Privacy

RunnerScope never asks for or stores a GitHub token. Authentication remains in GitHub CLI credential storage. Local configuration contains monitoring preferences and the organisation name only.

## Licence

Copyright © 2026 Shannon Smith.

RunnerScope is GPL-3.0-or-later. GitHub and GitHub Actions are trademarks of GitHub, Inc. RunnerScope is independent and is not affiliated with or endorsed by GitHub, Inc.
