# Runner Monitor

**Project copyright:** © 1993-2026 Shannon Smith

Runner Monitor is a native C desktop monitor for GitHub Actions self-hosted runners. The shipping application is compiled on both supported platforms and links directly against the exact Infiltratr Common revision recorded by the repository gitlink.

There is no Python or Tk/Tkinter runtime in the current native product line.

## Native architecture

- Linux: C11 + GTK 3 desktop shell, built as the ELF executable `/usr/bin/runnerscope`.
- Windows: C11 + native Win32 shell, built as `Runner-Monitor-Windows-v<version>.exe`.
- Shared configuration contract: `src/core/runner_config.c` owns the tested schema, defaults, validation and serialisation for both native applications.
- Windows platform adapters: `windows_config_store.c` owns private durable config I/O and `windows_http.c` uses Windows WinHTTP directly.
- Shared dependency: the exact `infiltratr-common` gitlink recorded by this Runner Monitor revision.
- Common owns the product-neutral palette, typography/metrics contract, formatting and other shared primitives.
- Runner Monitor owns runner/provider semantics, local-service behaviour and application policy.
- The shared GitHub provider parses and paginates API responses inside Runner Monitor. Linux uses the libcurl library and Windows uses WinHTTP; neither launches a command-line helper.
- Python/Tk compatibility sources and the old Common helper bridge are not part of the repository or release product.

The user-facing name is **Runner Monitor**. The stable Linux executable and desktop identity remain `runnerscope`, and the Debian package identity remains `infiltrator-runner-monitor`.

## Appearance

Both native shells consume Common directly. On Linux, the shell follows the current Infiltrator OS structure: branded header with Settings and window controls, responsive navigation rail using the same control/icon geometry and grouping as System Monitor, active workspace header/search/metrics, fixed-layout table surface, contextual selection actions and a compact status/version footer. The retired menubar and duplicate hero/counter summary from earlier iterations are not part of the current shell. Theme selection and persistence use Common's canonical System/Day/Night contract, and Follow system tracks live GTK host-theme changes.

Windows retains the same native bitmap/MB Corpo visual identity for the fleet shell. Its private configuration storage and HTTPS transport use native platform adapters.

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

The native Windows application currently provides the C/Common Win32 monitor shell, organisation configuration and live runner-state monitoring. Each provider refresh uses an immutable organisation/token snapshot and bounded native HTTPS requests. Results from superseded Settings are discarded. Failed refreshes retain the last observation with an explicit stale warning and unavailable counts. Active-job/history/local-service parity remains native follow-on work; it will not be implemented by restoring Python.

## Requirements

Windows requires 64-bit Windows with WinHTTP. Linux requires GTK 3, libcurl and trusted CA certificates. Local Linux service health uses systemd's native D-Bus interface; an explicit restart uses its interactive authorization support.

Runner Monitor has no command-line helper requirement. Set the organisation and access token in **Settings**, then choose **Save and test**. The token must permit reading the organisation's self-hosted runners; Linux activity discovery also needs access to the relevant repositories and Actions jobs. Permission failures explain which setting needs attention. See GitHub's [organisation runner permissions](https://docs.github.com/en/rest/actions/self-hosted-runners#list-self-hosted-runners-for-an-organization) for token requirements.

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

The shared configuration contract covers `organisation`, `github_token`, expected runner count, runner/activity/local-health polling intervals, repository scan limit and theme mode. Settings preserves unrelated fields. Older configuration without a token opens for setup; corrupt or unreadable configuration is reported. Saved settings survive application restarts and upgrades.

## Build

```text
git submodule update --init --recursive
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

CMake verifies that a populated Common submodule matches the gitlink recorded by the Runner Monitor revision; there is no second hand-maintained Common commit/version pin in the build definition.

On Windows, configure with the Visual Studio generator and build the Release configuration. Windows CTest additionally exercises the private durable configuration store. Linux CTest exercises native service response interpretation. Both platforms test configuration, API parsing, pagination and failure handling without real credentials.

## Privacy and credentials

The access token is saved in the existing per-user JSON configuration file. The Settings field is masked. Linux restricts the file to its owner; Windows restricts its ACL to the current user and SYSTEM. The file contains the token in plain text, so treat it as private. Errors and diagnostic output do not include the token. No GitHub CLI credential store, browser login or separate account connector is used.

cJSON 1.7.19 is bundled under its MIT licence; source identity and hashes are recorded in `third_party/cjson/README.md`. Linux packages carry the licence and Windows embeds it in the executable.

## Licence

Runner Monitor is free software licensed under the GNU General Public License v3.0 or later. See `LICENSE`.

GitHub and GitHub Actions are trademarks of GitHub, Inc. Runner Monitor is an independent project and is not affiliated with or endorsed by GitHub, Inc.
