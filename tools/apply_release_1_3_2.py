#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    return (ROOT / path).read_text(encoding="utf-8")


def write(path, text):
    (ROOT / path).write_text(text, encoding="utf-8")


def replace_once(text, old, new, label):
    if text.count(old) != 1:
        raise SystemExit(f"expected one {label} anchor, found {text.count(old)}")
    return text.replace(old, new, 1)


write("VERSION", "1.3.2\n")

changelog = read("debian/changelog")
entry = """infiltrator-runner-monitor (1.3.2) unstable; urgency=medium

  * Move the product header and general footer chrome behind the shared native-
    renderer seam introduced for navigation in 1.3.1.
  * Emit product/family identity plus Settings, Minimize, Maximize/Restore and
    Close header actions once from toolkit-neutral C.
  * Emit Export CSV, About and Refresh footer actions once, including the live
    Refreshing label and shared status/version slots.
  * Translate the same chrome structure through native GTK widgets and Win32
    GDI drawing without introducing a cross-platform widget toolkit.
  * Add toolkit-free tests for chrome action ordering, identity, refresh state
    and footer status/version propagation.
  * Keep contextual footer actions, page switching and workspace composition
    for later qualified 1.3.x slices; retain exact Common 1.19.38.

 -- Shannon Smith <infiltratr@yandex.com>  Tue, 06 Oct 2026 12:12:00 +1100

"""
if not changelog.startswith("infiltrator-runner-monitor (1.3.1)"):
    raise SystemExit("unexpected Debian changelog head")
write("debian/changelog", entry + changelog)

project = read("CHANGELOG.md")
anchor = "## Unreleased\n\nNo unreleased changes.\n\n## 1.3.1 - 2026-10-06\n"
replacement = """## Unreleased

No unreleased changes.

## 1.3.2 - 2026-10-06

- Move the product header and general footer chrome into the shared native-renderer translation layer.
- Emit product/family identity and the ordered Settings, Minimize, Maximize/Restore and Close header actions once from toolkit-neutral C.
- Emit Export CSV, About and Refresh footer actions once, including the Refreshing state plus shared status/application/Common version slots.
- Keep GTK and Win32 native: GTK creates native controls and Win32 draws native GDI surfaces from the same shared emission.
- Add toolkit-free tests for header/footer action order, identity, refresh-state labelling and status/version propagation.
- Leave contextual footer actions, page switching and deeper workspace composition for later small 1.3.x slices; retain Common 1.19.38.

## 1.3.1 - 2026-10-06
"""
project = replace_once(project, anchor, replacement, "project changelog")
write("CHANGELOG.md", project)

validation = read("docs/VALIDATION.md")
anchor = "## 1.3.1 first native-renderer unification slice\n"
section = """## 1.3.2 shared shell-chrome unification slice

Version 1.3.2 extends the native-renderer seam from navigation into the persistent shell chrome. `runner_ui_render_header()` now owns product/family identity and the ordered Settings, Minimize, Maximize/Restore and Close action semantics. `runner_ui_render_footer()` owns the general Export CSV, About and Refresh actions, the Refreshing label transition, and the status/application/Common-version slots. GTK and Win32 translate those same emissions into native widgets or native GDI drawing.

The toolkit-free renderer test now records header and footer emission as well as navigation. It verifies header action order, product identity, footer action order, the Refresh now → Refreshing… state change, and status/version propagation. Linux native, self-hosted Windows build, hosted Windows runtime and clean Debian build/install/smoke qualification remain required gates.

This slice deliberately does not absorb the page-specific contextual footer actions (`Open selected job`, `Open diagnostic`, `Restart selected runner`) or page-switch event handling. Those still depend on platform/backend parity work and remain separate follow-on slices rather than being hidden behind placeholder controls.

"""
validation = replace_once(validation, anchor, section + anchor, "validation section")
write("docs/VALIDATION.md", validation)

for transient in [
    ROOT / ".github/workflows/apply-release-1.3.2.yml",
    ROOT / "tools/apply_release_1_3_2.py",
]:
    if transient.exists():
        transient.unlink()
