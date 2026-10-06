#!/usr/bin/env python3
from pathlib import Path


def read(path):
    return Path(path).read_text(encoding="utf-8")


def write(path, text):
    Path(path).write_text(text, encoding="utf-8")


write("VERSION", "1.3.5\n")

# Debian changelog
path = "debian/changelog"
text = read(path)
entry = """infiltrator-runner-monitor (1.3.5) unstable; urgency=medium

  * Move runner-card presentation and selected-runner detail composition behind
    the toolkit-neutral native-renderer seam used by both GTK and Win32.
  * Share runner state labels/tone, utilisation normalization, job visibility,
    empty-state copy and complete selected-runner detail formatting.
  * Add stable runner-card grid/card component identities and toolkit-free
    renderer tests for state, selection, empty/loading and clamp behaviour.
  * Make the Windows selection surface consume the same detail payload as Linux
    while leaving unavailable Windows repository/job/runtime data explicit.
  * Keep Cards/Table and search/filter control composition for the next small
    migration slice; retain exact Infiltratr Common 1.19.38.

 -- Shannon Smith <infiltratr@yandex.com>  Tue, 06 Oct 2026 14:15:00 +1100

"""
if not text.startswith("infiltrator-runner-monitor (1.3.4)"):
    raise SystemExit("unexpected Debian changelog head")
write(path, entry + text)

# Project changelog
path = "CHANGELOG.md"
text = read(path)
anchor = "## Unreleased\n\nNo unreleased changes.\n\n"
section = """## 1.3.5 - 2026-10-06

- Move runner-card state/presentation and selected-runner detail composition through the shared native-renderer seam.
- Emit state labels/tone, utilisation, job visibility, empty/loading copy and selected-runner detail text once from toolkit-neutral C.
- Make GTK and Win32 translate the same runner-card/detail contract into native controls/drawing.
- Give the runner-card grid and card template stable component IDs and add toolkit-free renderer tests.
- Expand the Windows selection surface to the shared full-detail shape without inventing repository/job/runtime data whose Windows provider is not yet ported.
- Leave Cards/Table and search/filter control composition for the next contained 1.3.x slice; retain Common 1.19.38.

"""
if anchor not in text:
    raise SystemExit("CHANGELOG anchor missing")
write(path, text.replace(anchor, anchor + section, 1))

# Validation record
path = "docs/VALIDATION.md"
text = read(path)
anchor = "## 1.3.4 shared workspace-summary slice\n"
section = """## 1.3.5 shared runner-card/detail slice

Version 1.3.5 moves the Runners card presentation and selected-runner detail composition through the toolkit-neutral renderer seam. `runner_ui_render_runner_cards()` now emits the visible runner sequence, normalized state/tone, selection, utilization, job-visibility policy and empty/loading state once. `runner_ui_prepare_runner_selection()` owns the complete selected-runner text shape used by both platforms.

GTK retains its native FlowBox/card widgets and Win32 retains native GDI drawing. Their adapters now consume the same shared card/detail specification rather than separately deciding state wording, selection detail structure or empty-state copy. Stable `page.runners.cards` and `page.runners.cards.runner` component IDs make the migrated surface explicit in the UI contract.

The toolkit-free renderer test verifies Idle/Running/Offline projection, friendly state labels, selected state, job visibility, utilization clamping, selected-runner primary/secondary detail formatting, filtered-empty/loading messages and invalid/incomplete renderer rejection. Normal Linux native, self-hosted Windows build, hosted Windows runtime and clean Debian package/install/smoke qualification remain required gates.

Windows still does not have the Linux repository/job correlation provider, so those selected-runner fields deliberately remain unavailable instead of being fabricated. Cards/Table switching and search/filter control composition remain the next separate unification slice.

"""
if anchor not in text:
    raise SystemExit("VALIDATION anchor missing")
write(path, text.replace(anchor, section + anchor, 1))

# Remove release scaffolding from the release commit.
Path(".github/workflows/apply-release-1.3.5.yml").unlink(missing_ok=True)
Path("tools/apply_release_1_3_5.py").unlink(missing_ok=True)
