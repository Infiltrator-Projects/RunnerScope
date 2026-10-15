# Runner Monitor

**Project copyright:** © 1993-2026 Shannon Smith

Runner Monitor is a native C desktop monitor for GitHub Actions self-hosted runners. The shipping application is compiled on both supported platforms and links directly against the exact Infiltratr Common revision recorded by the repository gitlink.

There is no Python or Tk/Tkinter runtime in the current native product line.

## Native architecture

- Linux: C11 + GTK 3 desktop shell, built as the ELF executable `/usr/bin/runnerscope`.
- Windows: C11 + native Win32 shell, built as `Runner-Monitor-Windows-v<version>.exe`.
- Shared configuration contract: `src/core/runner_config.c` owns the tested target schema, defaults, validation and serialisation; Windows consumes it now and Linux migration is tracked under issue #14.
- Windows platform adapters: `windows_config_store.c` owns durable config I/O and `windows_process.c` owns bounded child-process capture/timeout semantics.
- Shared dependency: the exact `infiltratr-common` gitlink recorded by this Runner Monitor revision.
- Common owns the product-neutral palette, typography/metrics contract, formatting and other shared primitives.
- Runner Monitor owns runner/provider semantics, local-service behaviour and application policy.
- The current provider credential/API boundary is the authenticated GitHub CLI (`gh`); there is one shipping implementation per platform rather than a parallel experimental product path.
- Python/Tk compatibility sources and the old Common helper bridge are not part of the repository or release product.

The user-facing name is **Runner Monitor**. The stable Linux executable and desktop identity remain `runnerscope`, and the Debian package identity remains `infiltrator-runner-monitor`.

## Appearance

Both native shells consume Common directly. On Linux, the shell follows the current Infiltrator OS structure: branded header with Settings and window controls, responsive navigation rail using the same control/icon geometry and grouping as System Monitor, active workspace header/search/metrics, fixed-layout table surface, contextual selection actions and a compact status/version footer. The retired menubar and duplicate hero/counter summary from earlier iterations are not part of the current shell. Theme selection and persistence use Common's canonical System/Day/Night contract, and Follow system tracks live GTK host-theme changes.

Windows retains the same native bitmap/MB Corpo visual identity for the fleet shell. Its configuration and provider-process mechanics are no longer embedded persistence/process policy in the presentation controller.

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

The native Windows application currently provides the C/Common Win32 monitor shell, organisation configuration and live runner-state monitoring. Each provider refresh uses an immutable organisation snapshot and bounded GitHub CLI execution, so a Settings change cannot alter an in-flight request and a child process cannot retain the capture pipe indefinitely. Active-job/history/local-service parity remains native follow-on work; it will not be implemented by restoring Python.

## Requirements

### Windows

- 64-bit Windows supported by the current GitHub Actions Windows image/toolchain;
- GitHub CLI (`gh`); connect your account inside Runner Monitor.

### Linux

- GTK 3;
- GitHub CLI (`gh`);
- systemd for local runner-service health;
- polkit/`pkexec` for service restart.

Use **Connect GitHub** in Settings or the connection notice. Runner Monitor
shows a one-time code and opens GitHub's browser authorization page. Copy the
code, approve access, and Runner Monitor checks organisation runner access
automatically. No terminal command is needed.

When an existing login lacks the organisation runner permission, Connect GitHub
requests the additional `admin:org` OAuth scope. The account must already have
access to the organisation's runners; authorization cannot grant an organisation
role. Other access-denied responses remain visible for diagnosis.

Failed runner refreshes show a prominent notice, mark displayed rows as last
known data, and withhold live runner counts until a successful refresh.

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

Runner Monitor's core configuration contract covers `organisation`, expected runner count, runner/activity/local-health polling intervals, repository scan limit and theme mode. On Windows, editing the organisation preserves every other parsed field; corrupt persisted configuration is reported instead of being silently replaced by hard-coded defaults.

## Build

```text
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

CMake verifies that a populated Common submodule matches the gitlink recorded by the Runner Monitor revision; there is no second hand-maintained Common commit/version pin in the build definition.

On Windows, configure with the Visual Studio generator and build the Release configuration. Windows CTest additionally exercises the durable configuration store and bounded process adapter.

## Privacy and credentials

Runner Monitor manages browser authorization through GitHub CLI and uses its
existing credential store. It does not copy tokens into application settings.
An externally supplied `GH_TOKEN` or `GITHUB_TOKEN` overrides that store and
cannot be refreshed through browser sign-in; Runner Monitor explains when this
requires updating the managed token or removing the override. Fine-grained
tokens need the organisation's **Self-hosted runners: read** permission.
Provider changes must be made directly in the shipping native implementation
rather than through a second experimental product path.

## Licence

Runner Monitor is free software licensed under the GNU General Public License v3.0 or later. See `LICENSE`.

GitHub and GitHub Actions are trademarks of GitHub, Inc. Runner Monitor is an independent project and is not affiliated with or endorsed by GitHub, Inc.
