#!/usr/bin/env python3
"""Source-level regression guard for the Runner Monitor application shell."""
from pathlib import Path

source = Path("runnerscope.py").read_text(encoding="utf-8")
version = Path("VERSION").read_text(encoding="utf-8").strip()
assert f'VERSION = "{version}"' in source

required = (
    '"Topbar.TFrame"',
    '"Titlebar.TButton"',
    '"TitlebarIcon.TButton"',
    '"Titlebar.TSeparator"',
    '"Sidebar.TFrame"',
    '"SidebarFill.TFrame"',
    '"Nav.TButton"',
    '"NavSelected.TButton"',
    '"SummaryCard.TFrame"',
    '"Accent.SummaryCard.TFrame"',
    '"Success.SummaryCard.TFrame"',
    '"Warning.SummaryCard.TFrame"',
    '"Fault.SummaryCard.TFrame"',
    '"SummaryKicker.TLabel"',
    '"SummaryValue.TLabel"',
    '"Toolbar.TFrame"',
    '"TablePanel.TFrame"',
    '"DetailBar.TFrame"',
    '"StatusBar.TFrame"',
    '"Content.TNotebook"',
    'text="Runner Monitor"',
    'self.title("Runner Monitor")',
    'text="GitHub Actions runner operations & health"',
    '(0, "●  Runners")',
    '(1, "▶  Active jobs")',
    '(2, "◷  History")',
    '(3, "◆  Local service")',
    'self.counter_vars[name].set(str(value))',
    'STATE_PURPLE = palette["selected_summary"]',
    'MB_ACCENT = palette["neutral_accent"]',
    'MB_CONNECTION = palette["connection"]',
    'MB_WARNING_BORDER = palette["warning_border"]',
    'MB_SUCCESS_BORDER = palette["success_border"]',
    'text="ⓘ"',
    'text="↻"',
    'return f"Theme: {label}"',
    'def _cycle_theme(self) -> None:',
)
for token in required:
    assert token in source, f"missing Runner Monitor shell contract: {token}"

for forbidden in (
    '"Courier New"',
    '"Courier"',
    '"Cascadia Mono"',
    '"Liberation Mono"',
):
    assert forbidden not in source, f"application shell regressed to terminal typography: {forbidden}"

assert 'style.layout("Content.TNotebook.Tab", [])' in source
assert 'command=lambda tab=index: self._select_section(tab)' in source
assert 'self._sync_navigation()' in source

print("Runner Monitor UI shell contract passed")

assert 'nav_selected_bg = _blend_hex(MB_PANEL, MB_ACCENT, 0.075)' in source
assert 'nav_selected_border = _blend_hex(MB_BORDER, MB_ACCENT, 0.48)' in source
assert 'accent_card_bg = _blend_hex(MB_CARD, MB_ACCENT, 0.08)' in source
assert 'warning_card_bg = _blend_hex(MB_CARD, STATE_AMBER, 0.08)' in source
assert 'fault_card_bg = _blend_hex(MB_CARD, STATE_RED, 0.08)' in source
assert 'foreground=MB_ACCENT' in source
assert 'background=MB_CONNECTION' in source
assert 'style="Accent.TButton"' in source
assert 'style="Warning.TButton"' in source
assert 'height=44, padding=(6, 0)' in source
assert '"titlebar_subtitle": (body_family, 12, "bold")' in source
assert 'tree.tag_configure("RUNNING", foreground=MB_ACCENT' in source
assert 'tree.tag_configure("IDLE", foreground=STATE_GREEN)' in source

assert '_blend_hex(MB_PANEL, MB_ACCENT, 0.075)' in source
assert '_blend_hex(MB_CARD, STATE_AMBER, 0.08)' in source

assert source.count('tree.tag_configure("RUNNING"') == 1
assert source.count('tree.tag_configure("IDLE"') == 1
assert 'def _gh_json_pages(' in source
assert 'semantic_filter' in source
assert '_runner_is_currently_busy' in source
