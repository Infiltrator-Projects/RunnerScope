#!/usr/bin/env python3
"""Source-level regression guard for the Runner Monitor application shell."""
from pathlib import Path

source = Path("runnerscope.py").read_text(encoding="utf-8")

required = (
    'VERSION = "1.1.9"',
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

assert 'background=MB_SELECT_BG' in source
assert 'foreground=MB_ACCENT' in source
assert 'background=MB_CONNECTION' in source
assert 'style="Accent.TButton"' in source
assert 'style="Warning.TButton"' in source
