# Changelog

## Unreleased

No unreleased changes.

## 1.3.9 - 2026-10-15

- Add native Connect GitHub dialogs with streamed sign-in codes, Copy/Open/Cancel controls, and automatic runner-access verification on Linux and Windows.
- Recover missing GitHub login or organisation runner scope inside the app while preserving existing account scopes and Git settings.
- Show failed runner refreshes prominently, mark prior data as stale, and withhold live counts until runner access succeeds.
- Cancel and reap authorization processes on dialog/application closure, bound output drain and authorization time, and suppress debug output during sign-in.
- Add offline authorization and child-process regression tests covering streamed codes, account selection, environment credentials, failure, cancellation and cleanup.

## 1.3.8 - 2026-10-15

- Introduce a toolkit-neutral Runner Monitor configuration contract with one authority for defaults, validation, parsing and serialisation.
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

- Advance the exact repository gitlink to released Infiltratr Common 1.19.42 and remove the duplicated Common version/commit literals from CMake and README.
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

- Introduce the first toolkit-neutral Runner Monitor UI contract shared by the native GTK and Win32 products.
- Give the four pages and major shell surfaces stable component IDs so cross-platform parity can be tested independently of either toolkit.
- Make both native shells consume shared product identity, page/navigation metadata, hero artwork, metric captions and contextual action policy.
- Add a toolkit-free UI-contract CTest and validate the same contract from both native application self-tests.
- Run the full Linux, Windows build/runtime and clean Debian qualification suite on pull requests as well as `main`, so future drift is caught before merge.
- Keep GTK and Win32 native and preserve existing configuration/history data and Common 1.19.38 integration.

Earlier release detail remains permanently available in Git history and the corresponding release tags.
