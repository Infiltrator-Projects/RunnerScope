# RunnerScope

**Project copyright:** © 1993-2026 Shannon Smith

RunnerScope is a native C desktop monitor for GitHub Actions self-hosted runners. The shipping application is compiled on both supported platforms and links directly against the exact Libraries revision recorded by the repository gitlink.

There is no Python or Tk/Tkinter runtime in the current native product line.

## Native architecture

- Linux: C11 + GTK 3 desktop shell, built as the ELF executable `/usr/bin/runnerscope`.
- Windows: C11 + native Win32 shell, built as `RunnerScope-Windows-v<version>.exe`.
- Shared configuration contract: `src/core/runner_config.c` owns the tested schema, defaults, validation and serialisation for both native applications.
- Windows platform adapters: `windows_config_store.c` owns private durable config I/O and `windows_http.c` uses Windows WinHTTP directly.
- Shared dependency: the exact `infiltratr-common` gitlink recorded by this RunnerScope revision.
- Common owns the product-neutral palette, typography/metrics contract, formatting and other shared primitives.
- RunnerScope owns runner/provider semantics, local-service behaviour and application policy.
- The shared GitHub provider parses and paginates API responses inside RunnerScope. Linux uses the libcurl library and Windows uses WinHTTP; neither launches a command-line helper.
- Python/Tk compatibility sources and the old Common helper bridge are not part of the repository or release product.

The user-facing name is **RunnerScope**. The stable Linux executable and desktop identity remain `runnerscope`, and the Debian package identity remains `runnerscope`.

## Appearance

Both native shells consume Common directly. On Linux, the shell follows the current Infiltrator OS structure: branded header with Settings and window controls, responsive navigation rail using the same control/icon geometry and grouping as System Monitor, active workspace header/search/metrics, fixed-layout table surface, contextual selection actions and a compact status/version footer. The retired menubar and duplicate hero/counter summary from earlier iterations are not part of the current shell. Theme selection and persistence use Common's canonical System/Day/Night contract, and Follow system tracks live GTK host-theme changes.

Windows retains the same native bitmap/MB Corpo visual identity for the fleet shell. Its private configuration storage and HTTPS transport use native platform adapters.

System, Day and Night semantics come from Common. Runner-specific status colours and table meanings remain local to RunnerScope.

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

The native Windows application currently provides the C/Common Win32 monitor shell, organisation configuration and live runner-state monitoring. Each provider refresh uses an immutable organisation/token snapshot and bounded native HTTPS requests. Results from superseded Settings are discarded. Failed refreshes retain the last observation with an explicit stale warning and unavailable counts. Active-job/history/local-service parity remains native follow-on work; it will not be implemented by restoring Python.

## Requirements

Windows requires 64-bit Windows with WinHTTP. Linux requires GTK 3, libcurl and trusted CA certificates. Local Linux service health uses systemd's native D-Bus interface; an explicit restart uses its interactive authorization support.

Choose **Sign in with GitHub**. RunnerScope reuses an existing session when it has runner access; otherwise it opens your browser and copies the one-time code. Paste it into GitHub and approve access. The application remembers the account, discovers organisation choices and saves/connects automatically. A single administered organisation is selected for you. No app registration, Client ID, personal token, terminal command or separate helper installation is required. GitHub’s official sign-in tool is supplied with the release; monitoring itself remains inside the native application.

## Install and run

Windows: use the published `RunnerScope-Windows-v<version>.exe`.

Linux:

```text
sudo apt install ./runnerscope_<version>_amd64.deb
runnerscope
```

## Configuration

The native applications retain the stable RunnerScope configuration identity so existing settings can migrate without changing the user-facing product name.

- Windows: `%APPDATA%\RunnerScope\config.json`
- Linux: `$XDG_CONFIG_HOME/runnerscope/config.json`, or `~/.config/runnerscope/config.json`

The shared configuration contract covers `organisation`, the saved GitHub session (`github_token`), expected runner count, runner/activity/local-health polling intervals, repository scan limit and theme mode. Settings preserves unrelated fields. Older configuration without a session opens for browser sign-in; corrupt or unreadable configuration is reported. Saved settings survive application restarts and upgrades. Legacy `System`/`Day`/`Night` names are accepted through Common's case-insensitive theme parser. Historical whole-second timer values, including decimal spellings such as `2.0`, retain their values; fresh-install defaults stay unchanged.

## Build

```text
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

CMake verifies that a populated Common submodule matches the gitlink recorded by the RunnerScope revision; there is no second hand-maintained Common commit/version pin in the build definition.

On Windows, configure with the Visual Studio generator and build the Release configuration. Windows CTest additionally exercises the private durable configuration store. Linux CTest exercises native service response interpretation. Both platforms test configuration, API parsing, pagination and failure handling without real credentials.

## Privacy and credentials

The session returned by GitHub is saved in the existing per-user JSON configuration file. Linux restricts the file to its owner; Windows restricts its ACL to the current user and SYSTEM. Credentials never appear in URLs, diagnostics or the Settings controls. Cancelling sign-in preserves any previous saved account. Expired or revoked sessions ask for browser sign-in again. The supplied official GitHub sign-in broker handles browser authorization; see [GitHub sign-in](docs/OAUTH.md).

cJSON 1.7.19 is bundled under its MIT licence; source identity and hashes are recorded in `third_party/cjson/README.md`. Linux packages carry the licence and Windows embeds it in the executable.

## Licence

RunnerScope is free software licensed under the GNU General Public License v3.0 or later. See `LICENSE`.

GitHub and GitHub Actions are trademarks of GitHub, Inc. RunnerScope is an independent project and is not affiliated with or endorsed by GitHub, Inc.
