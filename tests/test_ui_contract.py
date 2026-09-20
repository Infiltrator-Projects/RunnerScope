#!/usr/bin/env python3
"""Source-level regression guard for the Runner Monitor application shell."""
from pathlib import Path

source = Path("runnerscope.py").read_text(encoding="utf-8")

required = (
    'VERSION = "1.1.7"',
    '"Topbar.TFrame"',
    '"Sidebar.TFrame"',
    '"SidebarFill.TFrame"',
    '"Nav.TButton"',
    '"NavSelected.TButton"',
    '"SummaryCard.TFrame"',
    '"SummaryKicker.TLabel"',
    '"SummaryValue.TLabel"',
    '"Toolbar.TFrame"',
    '"TablePanel.TFrame"',
    '"DetailBar.TFrame"',
    '"StatusBar.TFrame"',
    '"Content.TNotebook"',
    'text="Runner Monitor"',
    'text="GitHub Actions runner operations & health"',
    '(0, "●  Runners")',
    '(1, "▶  Active jobs")',
    '(2, "◷  History")',
    '(3, "◆  Local service")',
    'self.counter_vars[name].set(str(value))',
    'STATE_PURPLE = palette["selected_summary"]',
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
