# Changelog

## 1.3.18 — 2026-10-17

- Stop Activity scans from paging through completed workflow history that the UI never consumes.
- Query GitHub separately for in-progress and queued workflow runs, preserving existing bounded pagination and error handling.
- Add regression coverage proving unfiltered activity requests are narrowed to active run states.

## 1.3.17 — 2026-10-17

- Use RunnerScope consistently in the build project, Windows class identities, exports and CI artifacts.

## 1.3.16 - 2026-10-17

- Use canonical alpha product and package identities without migration aliases.

## 1.3.14

- Replace developer registration with automatically supplied official GitHub sign-in.
- Reuse an existing authorised session when it has runner access.
- Open the browser and copy the one-time code automatically.
- Discover organisation choices, select a sole administered organisation, and save/connect after sign-in.
- Remove the app-registration form, Client ID controls and separate Save requirement from sign-in.
- Verify the bundled sign-in helper and exercise cancellation, private temporary storage and process failures.


## 1.3.13

- Replace manual token entry with native GitHub browser sign-in on Windows and Linux.
- Preserve existing settings and sessions; cancelled sign-in cannot overwrite the saved account.
- Test authorization codes, server polling intervals, slow-down, expiry and safe error handling.
- Add complete GitHub setup inside Settings, including exact registration fields, an Open Registration link and a saved public Client ID.
- Open setup automatically when no project Client ID is supplied; configured builds go directly to browser sign-in.

## Unreleased

No unreleased changes.

## 1.3.12 - 2026-10-16

- Move the governing engineering philosophy into `docs/PHILOSOPHY.md` and update maintained documentation references.

## 1.3.11 - 2026-10-16

- Parse JSON number syntax independently of the desktop locale, preserving legacy decimal settings on comma-decimal desktops.
- Reject C-only NaN, infinity, hexadecimal and malformed numeric tokens, including in unknown forward-compatible fields.
- Exercise the real parser under available non-C locales and preserve caller state on malformed input.
- Verify complete staged release bytes before and after publication.

## 1.3.10 - 2026-10-16

- Restore settings compatibility with saved System/Day/Night theme names and historical runner/local-health refresh intervals, preserving organisation and expected runner count during upgrades.
- Accept historical whole-second timer numbers written with a decimal suffix without changing their values.
- Explain where to create the GitHub access token and which read permissions it needs directly in Settings.
- Add regression coverage using the legacy settings format that failed to load in 1.3.9.

## 1.3.9 - 2026-10-15

- Save organisation and a masked access token through Settings, preserving existing preferences across restarts and upgrades.
- Replace GitHub CLI requests with internal HTTPS: libcurl on Linux and WinHTTP on Windows, sharing bounded JSON parsing, pagination and permission errors.
- Use the same tested configuration contract on Linux and Windows and restrict token files to their owner.
- Keep last-known observations visibly stale after failures and discard results from superseded settings.
- Replace Linux systemctl/pkexec subprocesses with native systemd D-Bus observation and explicit restart; open diagnostics in an internal viewer.
- Add configuration, API/pagination/error, private storage and native service regression coverage. Keep all work and publication on main.

## 1.3.8 - 2026-10-15

- Introduce a toolkit-neutral RunnerScope configuration contract with one authority for defaults, validation, parsing and serialisation.
- Fix the Windows Settings corruption path: changing only the organisation now preserves expected-runner, poll, scan, repository-limit and theme values.
- Move Windows durable configuration I/O out of the Win32 controller; writes are flushed and atomically replaced, and malformed persisted state fails closed instead of silently becoming defaults.
- Move Windows child-process capture out of the Win32 controller and replace the ineffective post-read timeout with nonblocking pipe draining plus a real wall-clock timeout/termination contract.
- Snapshot the organisation per refresh worker so concurrent Settings changes cannot alter an in-flight GitHub request.
- Add deterministic configuration round-trip/duplicate/malformed-input tests plus Windows config-store and process-timeout CTests.
- Update architecture, roadmap and validation documentation to distinguish the completed Windows extraction from the remaining Linux controller hardening in issue #14.

## 1.3.7 - 2026-10-15

- Make `SHA256SUMS.txt` portable by recording release asset basenames instead of workflow-local `release/` paths.
- Verify the checksum manifest against the exact qualified Debian and Windows artifacts before publishing the release.

## 1.3.6 - 2026-10-15

- Advance the exact repository gitlink to released Libraries 1.19.42 and remove the duplicated Common version/commit literals from CMake and README.
- Derive Common checkout validation from the gitlink itself so dependency identity has one authority.
- Collapse Linux qualification into the clean Debian build/install path and run the full CTest suite from `dh_auto_test`, including the shared UI contracts.
- Collapse Windows qualification into one hosted x64 build so the executable that passes runtime tests is the executable staged for release.
- Replace the separate commit-message-gated release workflow with same-run publication from qualified artifacts and enforce VERSION/tag/main consistency.
- Pin artifact upload/download actions to immutable commits and remove redundant font-cache maintainer scripts in favour of standard fontconfig triggers.
- Make invalid shared UI page/platform inputs fail closed instead of silently projecting a plausible default and add a dedicated failure-semantics CTest.
- Record the remaining provider/configuration/persistence/process ownership debt separately in issue #14 rather than introducing another implementation path.

## 1.3.5 - 2026-10-06

- Move runner-card state/presentation and selected-runner detail composition through the shared native-renderer seam.
- Emit state labels/tone, utilisation, job visibility, empty/loading copy and selected-runner detail text once from toolkit-neutral C.
- Make GTK and Win32 translate the same runner-card/detail contract into native controls/drawing.
- Give the runner-card grid and card template stable component IDs and add toolkit-free renderer tests.
- Expand the Windows selection surface to the shared full-detail shape without inventing repository/job/runtime data whose Windows provider is not yet ported.
- Leave Cards/Table and search/filter control composition for the next contained 1.3.x slice; retain Common 1.19.38.

## 1.3.4 - 2026-10-06

- Move the workspace hero and four-metric summary behind the shared native-renderer seam.
- Emit page identity, platform title, subtitle, navigation/hero artwork roles and ordered metric captions/values once from toolkit-neutral C.
- Make GTK and Win32 translate that same summary into native widgets/drawing while retaining platform-native metric providers.
- Show unavailable values on Windows workspaces whose providers are not yet ported instead of reusing unrelated runner counts.
- Add toolkit-free tests for workspace identity, artwork roles, metric order/values, platform naming and invalid-state rejection.
- Retain Common 1.19.38 and the full Linux/Windows/Debian qualification gates.

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

- Introduce the first toolkit-neutral RunnerScope UI contract shared by the native GTK and Win32 products.
- Give the four pages and major shell surfaces stable component IDs so cross-platform parity can be tested independently of either toolkit.
- Make both native shells consume shared product identity, page/navigation metadata, hero artwork, metric captions and contextual action policy.
- Add a toolkit-free UI-contract CTest and validate the same contract from both native application self-tests.
- Run the full Linux, Windows build/runtime and clean Debian qualification suite on pull requests as well as `main`, so future drift is caught before merge.
- Keep GTK and Win32 native and preserve existing configuration/history data and Common 1.19.38 integration.

Earlier release detail remains permanently available in Git history and the corresponding release tags.
