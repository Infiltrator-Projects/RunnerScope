from pathlib import Path

Path('VERSION').write_text('1.3.3\n')

deb = Path('debian/changelog')
deb_text = deb.read_text()
entry = '''infiltrator-runner-monitor (1.3.3) unstable; urgency=medium

  * Move page-transition policy into the toolkit-neutral shared UI layer.
  * Make GTK and Win32 consume the same page-change/no-op, search/table
    visibility, selection-clear and provider-refresh decisions.
  * Keep GTK notebook switching and Win32 show/hide/repaint mechanics native.
  * Add toolkit-free tests for Runners, Active Jobs, History and Local Health
    transition semantics, including no-op and invalid-page handling.
  * Complete the navigation/page-switch policy unification tranche while
    retaining exact Infiltratr Common 1.19.38.

 -- Shannon Smith <infiltratr@yandex.com>  Tue, 06 Oct 2026 12:50:00 +1100

'''
if not deb_text.startswith('infiltrator-runner-monitor (1.3.2)'):
    raise SystemExit('unexpected debian/changelog head')
deb.write_text(entry + deb_text)

changelog = Path('CHANGELOG.md')
text = changelog.read_text()
marker = '## Unreleased\n\nNo unreleased changes.\n\n'
if text.count(marker) != 1:
    raise SystemExit('unexpected CHANGELOG Unreleased marker')
release = '''## 1.3.3 - 2026-10-06

- Move page-transition policy into one toolkit-neutral shared plan consumed by both native shells.
- Unify real-change/no-op decisions, Runners search/table visibility, selection clearing and page-triggered Activity/Local Health refresh policy.
- Keep actual GTK notebook switching and Win32 show/hide/repaint operations native to their platforms.
- Add toolkit-free tests covering Runners, Active Jobs, History and Local Health transitions plus invalid/no-op cases.
- Complete the navigation/page-switch policy tranche and retain Common 1.19.38.

'''
changelog.write_text(text.replace(marker, marker + release, 1))

validation = Path('docs/VALIDATION.md')
text = validation.read_text()
marker = '## 1.3.2 shared shell-chrome unification slice\n'
if text.count(marker) != 1:
    raise SystemExit('unexpected VALIDATION marker')
section = '''## 1.3.3 shared page-transition policy slice

Version 1.3.3 moves page-transition policy behind the toolkit-neutral UI seam. `runner_ui_plan_page_transition()` is now the single product-level decision point for whether a requested page differs from the current page, whether Runners search/table surfaces should be shown, whether the current selection is cleared, and whether entering Runners/Active Jobs or Local Health requests the corresponding provider refresh.

GTK and Win32 consume that same transition plan but continue to perform platform mechanics natively: GTK changes the notebook page and updates its workspace widgets, while Win32 shows/hides native controls and repaints its workspace. The shared layer therefore owns application semantics without becoming a cross-platform widget toolkit.

The toolkit-free contract test verifies Active Jobs, Runners, Local Health and no-op transitions, including runner-running/table-view state and invalid input rejection. Linux native, self-hosted Windows build, hosted Windows runtime and clean Debian build/install/smoke qualification remain required gates.

This completes the navigation/page-switch policy tranche. It does not claim functional parity for the Windows Active Jobs, History or Local Health backends; those remain later 1.3.x work.

'''
validation.write_text(text.replace(marker, section + marker, 1))

Path('.github/workflows/apply-release-1.3.3.yml').unlink(missing_ok=True)
Path('tools/apply_release_1_3_3.py').unlink(missing_ok=True)
