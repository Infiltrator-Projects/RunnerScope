#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def write(path, text):
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_once(text, old, new, label):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    return text.replace(old, new, 1)


write("VERSION", "1.3.1\n")

deb = read("debian/changelog")
deb_entry = """infiltrator-runner-monitor (1.3.1) unstable; urgency=medium

  * Add the first native-renderer translation seam on top of the shared UI
    contract introduced in 1.3.0.
  * Emit the complete navigation structure once from toolkit-neutral C instead
    of independently composing page order and separators in GTK and Win32.
  * Translate that shared navigation through thin native GTK and Win32 adapters,
    preserving native controls, drawing, platform labels and interaction.
  * Add toolkit-free renderer tests for page order, separator placement,
    selected-page state and the intentional Local Linux/Windows label variant.
  * Qualify the renderer slice through Linux native, Windows native, hosted
    Windows runtime and clean Debian build/install/smoke tests.
  * Retain exact Infiltratr Common 1.19.38 and all existing data compatibility.

 -- Shannon Smith <infiltratr@yandex.com>  Tue, 06 Oct 2026 11:30:00 +1100

"""
if not deb.startswith("infiltrator-runner-monitor (1.3.0)"):
    raise RuntimeError("unexpected Debian changelog head")
write("debian/changelog", deb_entry + deb)

changelog = read("CHANGELOG.md")
old_head = "# Changelog\n\n## Unreleased\n\nNo unreleased changes.\n\n"
new_head = old_head + """## 1.3.1 - 2026-10-06

- Add the first true native-renderer adapter seam to the 1.3 shared UI architecture.
- Emit the left navigation once from toolkit-neutral C, including page order, Local Health separator placement, selected-page state and platform-appropriate labels.
- Make GTK and Win32 translate that same navigation contract into their own native controls/drawing instead of independently owning the product structure.
- Add toolkit-free renderer tests proving the Linux and Windows projections receive the same navigation structure, with only the intentional Local Health platform label difference.
- Keep header/footer, workspace composition, runner cards/tables and the remaining functional-parity work for later small 1.3.x migration slices.
- Retain Common 1.19.38 and the full Linux/Windows/Debian qualification gates.

"""
changelog = replace_once(changelog, old_head, new_head, "CHANGELOG head")
write("CHANGELOG.md", changelog)

validation = read("docs/VALIDATION.md")
marker = "## 1.3.0 first shared-contract tranche\n"
section = """## 1.3.1 first native-renderer unification slice

Version 1.3.1 moves the navigation rail from shared metadata into the first real translation path. `runner_ui_render_navigation()` now emits navigation semantics once: page order, separator placement, platform label, and selected-page state. GTK and Win32 each provide a thin adapter that translates those semantics into native widgets or native drawing. The shared layer does not create GTK widgets, HWNDs, or a general-purpose compatibility toolkit.

The toolkit-free renderer test records the emitted navigation and verifies that both platform projections receive all four pages in the same order, exactly one separator before Local Health, and exactly one selected item. It also verifies that the only intended label difference remains `Local Linux health` versus `Local Windows health`. Both native applications continue to run their own runtime/self-tests in addition to this contract test.

This completes the renderer-interface seam and navigation emission only. Page-switch event handling and the header/footer, workspace hero/metrics, cards/tables/details, Active Jobs, History, Local Health and settings/actions composition remain native code until their own small migration slices are qualified.

"""
validation = replace_once(validation, marker, section + marker, "VALIDATION 1.3.0 marker")
write("docs/VALIDATION.md", validation)

for transient in (
    ROOT / "tools/apply_release_1_3_1.py",
    ROOT / ".github/workflows/apply-release-1.3.1.yml",
):
    if transient.exists():
        transient.unlink()
