# Validation

## Shipping evidence

CI validates the product as native software on both supported platforms.

Linux:
- configures and builds the C application against the pinned Common gitlink;
- runs all CTest contracts and the native application self-test;
- verifies the application is an ELF binary;
- rejects Python/Tk runtime linkage;
- builds a clean Debian package;
- rejects Python/Tk package dependencies;
- installs, smoke-tests and purges the exact package.

Windows:
- configures and builds the native Win32 C application against the pinned Common gitlink;
- the self-hosted ARM64 pool cross-builds the release x64 executable and verifies its PE machine type;
- a clean hosted Windows x64 runner independently builds and executes CTest plus `Runner-Monitor.exe --self-test`;
- stages and uploads the native EXE.

Release publication consumes only qualified native artifacts.

## 1.3.x shared UI contract qualification

The 1.3.x migration adds a toolkit-neutral application UI tree as the source of truth. Its tests must run without GTK or Win32 and fail if required product structure disappears or changes accidentally.

The shared-contract test must cover at least:

- stable page and navigation IDs;
- required component hierarchy;
- shared actions and visibility/enabled rules;
- fleet cards, details, Cards/Table and search/filter surfaces;
- Active Jobs, History and Local Health surfaces as they migrate;
- renderer capability coverage for every shared component type;
- explicit marking of any genuine platform-only extension.

Native renderer qualification remains separate. Linux must prove the GTK renderer can realise the contract; Windows must prove the Win32 renderer can realise the same contract. A shared-contract pass alone is not sufficient if a native renderer cannot instantiate the required surface.

Once a migration slice is complete, CI must reject new application-level direct GTK/Win32 layout composition outside the renderer/platform boundary for that slice. This prevents the old duplicated-layout architecture from quietly returning.

## 1.3.5 shared runner-card/detail slice

Version 1.3.5 moves the Runners card presentation and selected-runner detail composition through the toolkit-neutral renderer seam. `runner_ui_render_runner_cards()` now emits the visible runner sequence, normalized state/tone, selection, utilization, job-visibility policy and empty/loading state once. `runner_ui_prepare_runner_selection()` owns the complete selected-runner text shape used by both platforms.

GTK retains its native FlowBox/card widgets and Win32 retains native GDI drawing. Their adapters now consume the same shared card/detail specification rather than separately deciding state wording, selection detail structure or empty-state copy. Stable `page.runners.cards` and `page.runners.cards.runner` component IDs make the migrated surface explicit in the UI contract.

The toolkit-free renderer test verifies Idle/Running/Offline projection, friendly state labels, selected state, job visibility, utilization clamping, selected-runner primary/secondary detail formatting, filtered-empty/loading messages and invalid/incomplete renderer rejection. Normal Linux native, self-hosted Windows build, hosted Windows runtime and clean Debian package/install/smoke qualification remain required gates.

Windows still does not have the Linux repository/job correlation provider, so those selected-runner fields deliberately remain unavailable instead of being fabricated. Cards/Table switching and search/filter control composition remain the next separate unification slice.

## 1.3.4 shared workspace-summary slice

Version 1.3.4 moves the workspace hero and four-metric summary through the toolkit-neutral renderer seam. `runner_ui_render_workspace_summary()` now emits the selected page identity, platform-specific title, subtitle, navigation/hero artwork roles and the ordered four metric caption/value pairs once. GTK and Win32 translate that same summary into native widgets or native GDI drawing.

The platforms still calculate their runtime values natively because the underlying providers are not yet unified. Linux retains its existing Runners, Active Jobs, History and Local Health calculations. Windows provides its available Runners counts and deliberately reports unavailable values for workspaces whose Windows providers have not yet been ported, rather than showing misleading runner counts under unrelated captions.

The toolkit-free renderer test verifies page identity, platform-specific Local Health naming, subtitle/artwork propagation, metric order, captions and values, plus invalid/incomplete renderer rejection. The normal Linux native, self-hosted Windows build, hosted Windows runtime and clean Debian package/install/smoke qualification remain required gates.

This completes the hero/fleet-summary structural tranche. Runner cards/details, Cards/Table/search composition and the missing Windows workspace backends remain separate follow-on work.

## 1.3.3 shared page-transition policy slice

Version 1.3.3 moves page-transition policy behind the toolkit-neutral UI seam. `runner_ui_plan_page_transition()` is now the single product-level decision point for whether a requested page differs from the current page, whether Runners search/table surfaces should be shown, whether the current selection is cleared, and whether entering Runners/Active Jobs or Local Health requests the corresponding provider refresh.

GTK and Win32 consume that same transition plan but continue to perform platform mechanics natively: GTK changes the notebook page and updates its workspace widgets, while Win32 shows/hides native controls and repaints its workspace. The shared layer therefore owns application semantics without becoming a cross-platform widget toolkit.

The toolkit-free contract test verifies Active Jobs, Runners, Local Health and no-op transitions, including runner-running/table-view state and invalid input rejection. Linux native, self-hosted Windows build, hosted Windows runtime and clean Debian build/install/smoke qualification remain required gates.

This completes the navigation/page-switch policy tranche. It does not claim functional parity for the Windows Active Jobs, History or Local Health backends; those remain later 1.3.x work.

## 1.3.2 shared shell-chrome unification slice

Version 1.3.2 extends the native-renderer seam from navigation into the persistent shell chrome. `runner_ui_render_header()` now owns product/family identity and the ordered Settings, Minimize, Maximize/Restore and Close action semantics. `runner_ui_render_footer()` owns the general Export CSV, About and Refresh actions, the Refreshing label transition, and the status/application/Common-version slots. GTK and Win32 translate those same emissions into native widgets or native GDI drawing.

The toolkit-free renderer test now records header and footer emission as well as navigation. It verifies header action order, product identity, footer action order, the Refresh now → Refreshing… state change, and status/version propagation. Linux native, self-hosted Windows build, hosted Windows runtime and clean Debian build/install/smoke qualification remain required gates.

This slice deliberately does not absorb the page-specific contextual footer actions (`Open selected job`, `Open diagnostic`, `Restart selected runner`) or page-switch event handling. Those still depend on platform/backend parity work and remain separate follow-on slices rather than being hidden behind placeholder controls.

## 1.3.1 first native-renderer unification slice

Version 1.3.1 moves the navigation rail from shared metadata into the first real translation path. `runner_ui_render_navigation()` now emits navigation semantics once: page order, separator placement, platform label, and selected-page state. GTK and Win32 each provide a thin adapter that translates those semantics into native widgets or native drawing. The shared layer does not create GTK widgets, HWNDs, or a general-purpose compatibility toolkit.

The toolkit-free renderer test records the emitted navigation and verifies that both platform projections receive all four pages in the same order, exactly one separator before Local Health, and exactly one selected item. It also verifies that the only intended label difference remains `Local Linux health` versus `Local Windows health`. Both native applications continue to run their own runtime/self-tests in addition to this contract test.

This completes the renderer-interface seam and navigation emission only. Page-switch event handling and the header/footer, workspace hero/metrics, cards/tables/details, Active Jobs, History, Local Health and settings/actions composition remain native code until their own small migration slices are qualified.

## 1.3.0 first shared-contract tranche

Version 1.3.0 establishes the first executable cross-platform seam. The toolkit-neutral C contract owns the stable four-page identities, major shell/component IDs, shared product identity, navigation/page metadata, hero assets, metric captions, contextual action policy and the Runners view-switch rule. Both GTK and Win32 consume that same contract instead of carrying independent copies of those product decisions.

The contract is built as a platform-neutral library and has its own CTest. Both native application self-tests validate it as well. Pull requests now run the complete Linux native, self-hosted Windows x64 build, hosted Windows x64 runtime and clean Debian package/install qualification suite before merge, so subsequent migration slices are checked on both platforms before reaching `main`.

This is the first slice, not a claim that the whole UI has already moved behind renderers. Runner cards/details, deeper layout composition and the remaining Active Jobs, History, Local Health and contextual-action parity continue under the 1.3.x migration plan.

## 1.2.39 parity baseline

Version 1.2.39 is the pre-migration baseline: Windows was brought back into the current Infiltrator OS visual family, shares the product bitmap/typography assets, and is now build- and runtime-qualified on Windows. It is not yet proof that every Linux workspace/backend feature is present on Windows; that remaining functional parity is part of the 1.3.x migration work.

## 1.2.38 fleet presentation qualification

Clean-package CI runs the installed binary's `--ui-self-test` under Xvfb with
isolated configuration and state paths. The test makes no provider or service
calls. It verifies the default card view, idle/running/offline treatment,
utilisation fractions, current-job visibility, full selection details,
metadata filtering, empty results, card reuse and selection retention across
refreshes, removal of a selected runner, and the retained table model.
Visual review covers fifteen idle runners, a mixed fleet, selection details,
filtering, the table switch, day/night themes and a narrower window.

## 1.2.37 bitmap qualification

The clean-package job loads all nine installed PNGs through GdkPixbuf, rather
than checking only that two files exist. Visual inspection verifies solid,
shaded hardware artwork at the navigation, banner and sidebar display sizes.
Native Linux self-tests and the clean Debian install remain release gates;
the Common 1.19.38 pin and MB Corpo font verification remain enforced.

## 1.2.30 forensic qualification

The 1.2.30 Linux pass verifies that navigation uses the current Infiltrator OS/System Monitor toggle-button rail rather than the retired GtkListBox workaround, that the superseded list-row CSS/helpers are absent, and that one-second live runner/activity updates use direct row indexes instead of repeated linear searches through backing arrays. Presentation-only tick work is suppressed while the window is iconified, while provider polling continues normally. The exact latest Common 1.19.38 gitlink remains enforced.

## 1.2.29 forensic qualification

The 1.2.29 Linux pass additionally verifies that the shipping tree has one Common-driven GTK style projection, no temporary forensic workflow or transformation script, the latest pinned Common revision, and no superseded tab-label naming. Provider refresh application was qualified after removing redundant visible-model rebuilds and transfer-time deep copies.

## Parity rule

A platform feature is not considered complete merely because the native binary compiles. Behavioural parity must be demonstrated for runner state, job correlation, history, service controls and persistence. During 1.3.x, shared product structure must additionally be represented once in the UI contract and proven renderable by both native backends before claiming cross-platform UI parity.
