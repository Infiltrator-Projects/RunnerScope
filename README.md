# Runner Monitor

**Project copyright:** © 1993-2026 Shannon Smith

Runner Monitor is a native C desktop monitor for GitHub Actions self-hosted runners. The shipping application is compiled on both supported platforms and links directly against the repository-pinned Infiltratr Common library.

There is no Python or Tk/Tkinter runtime in the 1.2 native product line.

## Native architecture

- Linux: C11 + GTK 3 desktop shell, built as the ELF executable `/usr/bin/runnerscope`.
- Windows: C11 + native Win32 shell, built as `Runner-Monitor-Windows-v<version>.exe`.
- Shared dependency: the exact pinned Infiltratr Common source tree.
- Common owns the product-neutral palette, typography/metrics contract, formatting and other shared primitives.
- Runner Monitor owns runner/provider semantics, local-service behaviour and application policy.
- The current provider credential/API boundary remains the authenticated GitHub CLI (`gh`) while the dependency-minimisation core in `src/native2/` is qualified to replace it.
- Python/Tk compatibility sources and the old Common helper bridge are not part of the repository or release product.

The user-facing name is **Runner Monitor**. The stable Linux executable and desktop identity remain `runnerscope`, and the Debian package identity remains `infiltrator-runner-monitor`.

## Appearance

Both native shells consume Common directly. The footer reports the application version followed by the linked Common version, with Common as the final line.

System, Day and Night semantics come from Common. Runner-specific status colours and table meanings remain local to Runner Monitor.

## Features

The restored native Linux application includes:

- live organisation runner status;
- active workflow/job discovery;
- self-hosted versus GitHub-hosted activity counters;
- runtime, state duration, session jobs and utilisation;
- filtering and CSV export;
- persistent history;
- local Linux runner-service health and diagnostic discovery;
- explicit local service restart;
- native configuration UI;
- Common-owned appearance and durable publication.

The native Windows application currently provides the C/Common Win32 monitor shell, organisation configuration and live runner-state monitoring. Active-job/history/local-service parity remains native follow-on work; it will not be implemented by restoring Python.

## Requirements

### Windows

- 64-bit Windows supported by the current GitHub Actions Windows image/toolchain;
- GitHub CLI (`gh`) authenticated with access to the target organisation.

### Linux

- GTK 3;
- GitHub CLI (`gh`);
- systemd for local runner-service health;
- polkit/`pkexec` for service restart.

Authenticate GitHub CLI before starting Runner Monitor:

```text
gh auth login
```

## Install and run

Windows: use the published `Runner-Monitor-Windows-v<version>.exe`.

Linux:

```text
sudo apt install ./runnerscope_<version>_amd64.deb
runnerscope
```

## Configuration

The native applications retain the stable RunnerScope configuration identity so existing settings can migrate without changing the user-facing product name.

- Windows: `%APPDATA%\RunnerScope\config.json`
- Linux: `$XDG_CONFIG_HOME/runnerscope/config.json`, or `~/.config/runnerscope/config.json`

`GITHUB_RUNNER_ORG` can override the configured organisation.

## Build

```text
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

On Windows, configure with the Visual Studio generator and build the Release configuration.

## Privacy and credentials

Runner Monitor does not store a GitHub token. The current shipping provider adapter uses the authenticated GitHub CLI credential store. The native dependency-minimisation core is being developed separately so that this adapter can eventually be replaced without changing the product back to an interpreted runtime.

## Licence

Runner Monitor is free software licensed under the GNU General Public License v3.0 or later. See `LICENSE`.

GitHub and GitHub Actions are trademarks of GitHub, Inc. Runner Monitor is an independent project and is not affiliated with or endorsed by GitHub, Inc.
