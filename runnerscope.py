#!/usr/bin/env python3
# RunnerScope - cross-platform GitHub Actions runner monitor
# Copyright (C) 2026 Shannon Smith
# SPDX-License-Identifier: GPL-3.0-or-later
"""RunnerScope - cross-platform desktop monitor for GitHub Actions self-hosted runners.

RunnerScope keeps the fast runner polling, rich activity resolution, session history,
local service health, and graphite/silver interface of the original monitor while
storing user-specific organisation and runner settings outside the source tree.

Runtime requirements:
  * Python 3.10 or newer with Tkinter
  * GitHub CLI (gh), authenticated for the organisation being monitored
  * Windows PowerShell for Windows local-service health/restart
  * systemd plus pkexec/sudo for Linux local-service health/restart

No third-party Python packages are required.
"""

from __future__ import annotations

import base64
import concurrent.futures
import csv
import datetime as dt
import json
import os
import queue
import re
import shutil
import subprocess
import sys
import threading
import time
import webbrowser
from collections import deque
from pathlib import Path
from typing import Any, Callable, Iterable

try:
    import tkinter as tk
    from tkinter import filedialog
    from tkinter import font as tkfont
    from tkinter import messagebox, ttk
except ImportError as exc:  # pragma: no cover - Windows Python normally includes it.
    raise SystemExit(
        "Tkinter is unavailable. Re-run the Python for Windows installer and "
        "enable 'tcl/tk and IDLE'.\n\n"
        f"Original error: {exc}"
    ) from exc


def _env_float(name: str, default: float, minimum: float) -> float:
    try:
        return max(minimum, float(os.environ.get(name, str(default))))
    except ValueError:
        return default


def _env_int(name: str, default: int, minimum: int) -> int:
    try:
        return max(minimum, int(os.environ.get(name, str(default))))
    except ValueError:
        return default


APP_NAME = "RunnerScope"
VERSION = "1.0.3"

DEFAULT_CONFIG: dict[str, Any] = {
    "organisation": "",
    "expected_runners": 0,
    "runner_poll_seconds": 2.0,
    "activity_scan_seconds": 45.0,
    "repository_scan_limit": 25,
    "repository_cache_seconds": 300.0,
    "history_entries": 300,
    "local_health_seconds": 10.0,
}

def config_dir() -> Path:
    if sys.platform == "win32":
        root = os.environ.get("APPDATA") or os.environ.get("LOCALAPPDATA")
        return Path(root) / APP_NAME if root else Path.home() / APP_NAME
    root = os.environ.get("XDG_CONFIG_HOME")
    return Path(root) / "runnerscope" if root else Path.home() / ".config" / "runnerscope"

def state_dir() -> Path:
    if sys.platform == "win32":
        root = os.environ.get("LOCALAPPDATA") or os.environ.get("APPDATA")
        return Path(root) / APP_NAME if root else Path.home() / APP_NAME
    root = os.environ.get("XDG_STATE_HOME")
    return Path(root) / "runnerscope" if root else Path.home() / ".local" / "state" / "runnerscope"

CONFIG_FILE = config_dir() / "config.json"
STATE_FILE = state_dir() / "state.json"

ORG = ""
REFRESH_SECONDS = 2.0
ACTIVITY_SECONDS = 45.0
REPO_LIMIT = 25
REPO_CACHE_SECONDS = 300.0
MAX_HISTORY = 300
EXPECTED_RUNNERS = 0
LOCAL_HEALTH_SECONDS = 10.0

def load_config() -> dict[str, Any] | None:
    if not CONFIG_FILE.is_file():
        return None
    try:
        data = json.loads(CONFIG_FILE.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    if not isinstance(data, dict):
        return None
    cfg = dict(DEFAULT_CONFIG)
    cfg.update(data)
    return cfg

def save_config(cfg: dict[str, Any]) -> None:
    CONFIG_FILE.parent.mkdir(parents=True, exist_ok=True)
    clean = {key: cfg.get(key, value) for key, value in DEFAULT_CONFIG.items()}
    tmp = CONFIG_FILE.with_suffix(".tmp")
    tmp.write_text(json.dumps(clean, indent=2) + "\n", encoding="utf-8")
    tmp.replace(CONFIG_FILE)

def apply_config(cfg: dict[str, Any]) -> None:
    global ORG, REFRESH_SECONDS, ACTIVITY_SECONDS, REPO_LIMIT
    global REPO_CACHE_SECONDS, MAX_HISTORY, EXPECTED_RUNNERS, LOCAL_HEALTH_SECONDS
    ORG = str(os.environ.get("GITHUB_RUNNER_ORG") or cfg.get("organisation") or "").strip()
    REFRESH_SECONDS = _env_float("GITHUB_RUNNER_REFRESH", float(cfg.get("runner_poll_seconds", 2.0)), 1.0)
    ACTIVITY_SECONDS = _env_float("GITHUB_RUNNER_ACTIVITY_REFRESH", float(cfg.get("activity_scan_seconds", 45.0)), 10.0)
    REPO_LIMIT = _env_int("GITHUB_RUNNER_REPO_LIMIT", int(cfg.get("repository_scan_limit", 25)), 1)
    REPO_CACHE_SECONDS = _env_float("GITHUB_RUNNER_REPO_CACHE", float(cfg.get("repository_cache_seconds", 300.0)), 60.0)
    MAX_HISTORY = _env_int("GITHUB_RUNNER_HISTORY", int(cfg.get("history_entries", 300)), 50)
    EXPECTED_RUNNERS = _env_int("GITHUB_RUNNER_EXPECTED", int(cfg.get("expected_runners", 0)), 0)
    LOCAL_HEALTH_SECONDS = _env_float("GITHUB_RUNNER_LOCAL_HEALTH_REFRESH", float(cfg.get("local_health_seconds", 10.0)), 5.0)

# RunnerScope graphite/silver visual language, shared on Windows and Linux.
MB_BG = "#050608"
MB_PANEL = "#101318"
MB_CARD = "#171b20"
MB_SURFACE = "#0d1014"
MB_BORDER = "#353a40"
MB_TEXT = "#e8ecef"
MB_TITLE = "#eef1f3"
MB_MUTED = "#aeb6bd"
MB_SUBTLE = "#899198"
MB_BUTTON_BG = "#d7dde2"
MB_BUTTON_FG = "#111418"
MB_SELECT_BG = "#2b3137"
MB_SELECT_FG = "#eef1f3"

# Status colours keep operational states distinct inside the graphite/silver palette.
STATE_RED = "#c96b6b"
STATE_GREEN = "#63ab7c"
STATE_BLUE = "#9aa7b2"
STATE_AMBER = "#d19e47"
STATE_PURPLE = "#7fa7c9"
STATE_GREY = "#899198"

MB_BODY_FONT = "MB Corpo S Title WEB"
MB_BRAND_FONT = "MB Corpo A Title Cond WEB"
MB_FONT_FILES = (
    "mb_corpo_a_cond_regular.ttf",
    "mb_corpo_s_bold.ttf",
    "mb_corpo_s_regular.ttf",
)


def register_optional_brand_fonts() -> None:
    """Use locally installed/private brand fonts when available; never distribute them."""
    if sys.platform != "win32":
        return
    try:
        import ctypes

        add_font = ctypes.windll.gdi32.AddFontResourceExW
        FR_PRIVATE = 0x10
        base = Path(__file__).resolve().parent
        roots = [
            base,
            base / "fonts",
            base / "assets",
            base / "assets" / "fonts",
        ]
        env_dir = os.environ.get("RUNNERSCOPE_FONT_DIR")
        if env_dir:
            roots.insert(0, Path(env_dir))
        for root in roots:
            for filename in MB_FONT_FILES:
                candidate = root / filename
                if candidate.is_file():
                    add_font(str(candidate), FR_PRIVATE, 0)
    except (AttributeError, OSError):
        pass


def apply_windows_dark_titlebar(window: tk.Tk) -> None:
    """Ask modern Windows to render the native title bar in dark mode."""
    if sys.platform != "win32":
        return
    try:
        import ctypes

        window.update_idletasks()
        hwnd = ctypes.windll.user32.GetParent(window.winfo_id()) or window.winfo_id()
        value = ctypes.c_int(1)
        for attribute in (20, 19):  # Windows 10/11 variants.
            result = ctypes.windll.dwmapi.DwmSetWindowAttribute(
                hwnd, attribute, ctypes.byref(value), ctypes.sizeof(value)
            )
            if result == 0:
                break
    except (AttributeError, OSError, tk.TclError):
        pass


def enable_windows_dpi_awareness() -> None:
    """Ask Windows for crisp Tk rendering; harmless on other platforms."""
    if sys.platform != "win32":
        return
    try:
        import ctypes

        ctypes.windll.shcore.SetProcessDpiAwareness(1)
    except (AttributeError, OSError):
        try:
            ctypes.windll.user32.SetProcessDPIAware()
        except (AttributeError, OSError):
            pass


def configure_shared_theme(window: tk.Misc) -> dict[str, tuple[Any, ...]]:
    """Apply RunnerScope's graphite/silver theme to any Tk or Toplevel window."""
    families = {str(name) for name in tkfont.families(window)}
    body_family = MB_BODY_FONT if MB_BODY_FONT in families else ("Segoe UI" if sys.platform == "win32" else "TkDefaultFont")
    brand_family = MB_BRAND_FONT if MB_BRAND_FONT in families else body_family
    fonts = {
        "body": (body_family, 10),
        "body_bold": (body_family, 10, "bold"),
        "small": (body_family, 9),
        "small_bold": (body_family, 9, "bold"),
        "counter": (body_family, 10, "bold"),
        "brand": (brand_family, 19),
        "section": (body_family, 12, "bold"),
    }
    try:
        window.configure(background=MB_BG)
        window.option_add("*Font", fonts["body"])
    except tk.TclError:
        pass
    style = ttk.Style(window)
    if "clam" in style.theme_names():
        style.theme_use("clam")
    style.configure(".", background=MB_BG, foreground=MB_TEXT, font=fonts["body"], bordercolor=MB_BORDER, darkcolor=MB_BORDER, lightcolor=MB_BORDER, troughcolor=MB_SURFACE, focuscolor=MB_BORDER)
    style.configure("TFrame", background=MB_BG)
    style.configure("TLabel", background=MB_BG, foreground=MB_TEXT)
    style.configure("Title.TLabel", font=fonts["brand"], foreground=MB_TITLE)
    style.configure("Section.TLabel", font=fonts["section"], foreground=MB_TITLE)
    style.configure("Meta.TLabel", foreground=MB_MUTED, font=fonts["small"])
    style.configure("Counter.TLabel", font=fonts["counter"], foreground=MB_TITLE)
    style.configure("Green.Counter.TLabel", font=fonts["counter"], foreground=STATE_GREEN)
    style.configure("Blue.Counter.TLabel", font=fonts["counter"], foreground=STATE_BLUE)
    style.configure("Red.Counter.TLabel", font=fonts["counter"], foreground=STATE_RED)
    style.configure("Purple.Counter.TLabel", font=fonts["counter"], foreground=STATE_PURPLE)
    style.configure("Amber.Counter.TLabel", font=fonts["counter"], foreground=STATE_AMBER)
    style.configure("CounterCard.TFrame", background=MB_CARD, bordercolor=MB_BORDER, relief="solid", borderwidth=1)
    style.configure("Detail.TLabel", foreground=MB_MUTED, font=fonts["small"])
    style.configure("TButton", background=MB_BUTTON_BG, foreground=MB_BUTTON_FG, bordercolor=MB_BORDER, font=fonts["body_bold"], padding=(12, 6), relief="flat")
    style.map("TButton", background=[("disabled", MB_PANEL), ("pressed", "#b9c0c6"), ("active", "#eef1f3")], foreground=[("disabled", MB_SUBTLE), ("pressed", MB_BUTTON_FG), ("active", MB_BUTTON_FG)])
    style.configure("TEntry", fieldbackground=MB_SURFACE, foreground=MB_TEXT, insertcolor=MB_TEXT, bordercolor=MB_BORDER, lightcolor=MB_BORDER, darkcolor=MB_BORDER, padding=(7, 5))
    style.map("TEntry", fieldbackground=[("focus", MB_PANEL), ("disabled", MB_PANEL)], foreground=[("disabled", MB_SUBTLE)], bordercolor=[("focus", MB_MUTED)])
    style.configure("TNotebook", background=MB_BG, bordercolor=MB_BORDER, tabmargins=(0, 5, 0, 0))
    style.configure("TNotebook.Tab", background=MB_PANEL, foreground=MB_MUTED, bordercolor=MB_BORDER, font=fonts["body_bold"], padding=(16, 8))
    style.map("TNotebook.Tab", background=[("selected", MB_CARD), ("active", "#14181d")], foreground=[("selected", MB_TITLE), ("active", MB_TEXT)])
    style.configure("Treeview", background=MB_SURFACE, fieldbackground=MB_SURFACE, foreground=MB_TEXT, bordercolor=MB_BORDER, rowheight=28, font=fonts["small"])
    style.map("Treeview", background=[("selected", MB_SELECT_BG)], foreground=[("selected", MB_SELECT_FG)])
    style.configure("Treeview.Heading", background=MB_PANEL, foreground=MB_TITLE, bordercolor=MB_BORDER, font=fonts["small_bold"], padding=(7, 7), relief="flat")
    style.map("Treeview.Heading", background=[("active", MB_CARD)], foreground=[("active", MB_TITLE)])
    style.configure("Vertical.TScrollbar", background=MB_PANEL, troughcolor=MB_SURFACE, bordercolor=MB_BORDER, arrowcolor=MB_MUTED)
    style.configure("Horizontal.TScrollbar", background=MB_PANEL, troughcolor=MB_SURFACE, bordercolor=MB_BORDER, arrowcolor=MB_MUTED)
    return fonts


class ConfigDialog(tk.Toplevel):
    FIELDS = (
        ("organisation", "GitHub organisation", "The organisation that owns the self-hosted runners."),
        ("expected_runners", "Expected runners", "0 disables the missing-runner count check."),
        ("runner_poll_seconds", "Runner refresh", "Fast status refresh in seconds; 2 is recommended."),
        ("activity_scan_seconds", "Job detail scan", "Full workflow/job scan interval in seconds. Busy transitions trigger an immediate scan."),
        ("repository_scan_limit", "Repositories to scan", "Most recently active repositories checked for jobs."),
        ("local_health_seconds", "Local service health", "How often local runner services are checked."),
    )

    def __init__(self, parent: tk.Misc, cfg: dict[str, Any] | None = None, title: str = "RunnerScope setup") -> None:
        super().__init__(parent)
        self.result: dict[str, Any] | None = None
        source = dict(DEFAULT_CONFIG)
        if cfg:
            source.update(cfg)
        self.vars = {key: tk.StringVar(value=str(source[key])) for key, _, _ in self.FIELDS}
        self.title(title)
        self.resizable(False, False)
        configure_shared_theme(self)
        apply_windows_dark_titlebar(self)
        if parent.winfo_viewable():
            self.transient(parent)
        self.grab_set()
        self.protocol("WM_DELETE_WINDOW", self.destroy)

        outer = ttk.Frame(self, padding=18)
        outer.pack(fill=tk.BOTH, expand=True)
        ttk.Label(outer, text="RunnerScope", style="Title.TLabel").grid(row=0, column=0, columnspan=3, sticky="w")
        ttk.Label(outer, text="Connect this monitor to your GitHub Actions runners.", style="Meta.TLabel").grid(row=1, column=0, columnspan=3, sticky="w", pady=(1, 14))

        first_entry = None
        for row, (key, label, hint) in enumerate(self.FIELDS, start=2):
            ttk.Label(outer, text=label).grid(row=row, column=0, sticky="w", padx=(0, 12), pady=5)
            entry = ttk.Entry(outer, textvariable=self.vars[key], width=31)
            entry.grid(row=row, column=1, sticky="ew", pady=5)
            ttk.Label(outer, text=hint, style="Meta.TLabel").grid(row=row, column=2, sticky="w", padx=(12, 0), pady=5)
            if first_entry is None:
                first_entry = entry

        ttk.Label(outer, text="Authentication stays in GitHub CLI (gh auth login); RunnerScope never stores your token.", style="Meta.TLabel").grid(row=8, column=0, columnspan=3, sticky="w", pady=(12, 4))
        self.test_status = tk.StringVar(value="")
        ttk.Label(outer, textvariable=self.test_status, style="Meta.TLabel").grid(row=9, column=0, columnspan=2, sticky="w")
        buttons = ttk.Frame(outer)
        buttons.grid(row=9, column=2, sticky="e", pady=(8, 0))
        ttk.Button(buttons, text="Test GitHub access", command=self._test_access).pack(side=tk.LEFT, padx=(0, 8))
        ttk.Button(buttons, text="Cancel", command=self.destroy).pack(side=tk.LEFT, padx=(0, 8))
        ttk.Button(buttons, text="Save", command=self._save).pack(side=tk.LEFT)
        outer.columnconfigure(1, weight=1)
        self.update_idletasks()
        self.lift()
        self.focus_force()
        if first_entry is not None:
            first_entry.focus_set()
        self.bind("<Return>", lambda _e: self._save())
        self.bind("<Escape>", lambda _e: self.destroy())

    def _values(self) -> dict[str, Any] | None:
        organisation = self.vars["organisation"].get().strip()
        if not re.fullmatch(r"[A-Za-z0-9_.-]+", organisation):
            messagebox.showerror(APP_NAME, "Enter the GitHub organisation name only.", parent=self)
            return None
        try:
            cfg = dict(DEFAULT_CONFIG)
            cfg.update(
                organisation=organisation,
                expected_runners=max(0, int(self.vars["expected_runners"].get())),
                runner_poll_seconds=max(1.0, float(self.vars["runner_poll_seconds"].get())),
                activity_scan_seconds=max(10.0, float(self.vars["activity_scan_seconds"].get())),
                repository_scan_limit=max(1, int(self.vars["repository_scan_limit"].get())),
                local_health_seconds=max(5.0, float(self.vars["local_health_seconds"].get())),
            )
        except ValueError:
            messagebox.showerror(APP_NAME, "One of the numeric settings is invalid.", parent=self)
            return None
        return cfg

    def _test_access(self) -> None:
        cfg = self._values()
        if not cfg:
            return
        gh = find_gh()
        if not gh:
            messagebox.showerror(APP_NAME, "GitHub CLI (gh) is not installed or not on PATH.", parent=self)
            return
        self.test_status.set("Testing GitHub access…")
        self.update_idletasks()
        try:
            proc = subprocess.run([gh, "api", f"/orgs/{cfg['organisation']}/actions/runners?per_page=1"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace", timeout=20, creationflags=_creation_flags(), check=False)
            if proc.returncode != 0:
                raise RuntimeError((proc.stderr or proc.stdout).strip() or "GitHub CLI request failed")
            payload = json.loads(proc.stdout or "{}")
            total = int(payload.get("total_count", 0))
            self.test_status.set(f"GitHub access OK — organisation reports {total} runner(s).")
        except Exception as exc:
            self.test_status.set("GitHub access failed.")
            messagebox.showerror(APP_NAME, str(exc), parent=self)

    def _save(self) -> None:
        cfg = self._values()
        if cfg is None:
            return
        self.result = cfg
        self.destroy()

def find_gh() -> str | None:
    """Return the GitHub CLI executable without assuming a shell."""
    found = shutil.which("gh.exe") or shutil.which("gh")
    if found:
        return found
    if sys.platform == "win32":
        roots = [os.environ.get("ProgramFiles"), os.environ.get("LOCALAPPDATA")]
        candidates = []
        if roots[0]:
            candidates.append(Path(roots[0]) / "GitHub CLI" / "gh.exe")
        if roots[1]:
            candidates.append(Path(roots[1]) / "Programs" / "GitHub CLI" / "gh.exe")
        for candidate in candidates:
            if candidate.is_file():
                return str(candidate)
    return None


def parse_github_time(value: Any) -> float | None:
    if not value or not isinstance(value, str):
        return None
    try:
        if value.endswith("Z"):
            value = value[:-1] + "+00:00"
        return dt.datetime.fromisoformat(value).timestamp()
    except ValueError:
        return None


def fmt_duration(seconds: float | None) -> str:
    if seconds is None:
        return "—"
    seconds = max(0, int(seconds))
    if seconds < 60:
        return f"{seconds}s"
    minutes, sec = divmod(seconds, 60)
    if minutes < 60:
        return f"{minutes}m {sec:02d}s"
    hours, minute = divmod(minutes, 60)
    if hours < 24:
        return f"{hours}h {minute:02d}m"
    days, hour = divmod(hours, 24)
    return f"{days}d {hour:02d}h"


def fmt_clock(timestamp: float | None) -> str:
    if not timestamp:
        return "—"
    return time.strftime("%H:%M:%S", time.localtime(timestamp))


def fmt_history_time(timestamp: float | None) -> str:
    if not timestamp:
        return "—"
    return time.strftime("%d/%m %H:%M:%S", time.localtime(timestamp))


def extract_service_executable(path_name: str) -> Path | None:
    """Extract the executable path from a Windows service PathName string."""
    text = str(path_name or "").strip()
    if not text:
        return None
    quoted = re.match(r'^"([^"]+)"', text)
    if quoted:
        return Path(quoted.group(1))
    # Unquoted runner service paths can contain no spaces in the executable
    # portion.  The official service normally uses a quoted path when needed.
    return Path(text.split()[0])


def current_job_step(job: dict[str, Any]) -> str:
    """Return the most useful live step label from a GitHub job payload."""
    steps = job.get("steps") or []
    for step in steps:
        if str(step.get("status") or "").casefold() == "in_progress":
            return str(step.get("name") or "Running step")
    # While GitHub transitions between steps there may briefly be no
    # in-progress step.  Show completed/total progress rather than blanking.
    if steps:
        complete = sum(str(step.get("status") or "").casefold() == "completed" for step in steps)
        return f"{complete}/{len(steps)} steps"
    return "—"


def short_labels(labels: Iterable[Any] | None) -> str:
    cleaned: list[str] = []
    for item in labels or []:
        if isinstance(item, dict):
            item = item.get("name", "")
        text = str(item or "").strip()
        if text and text != "self-hosted":
            cleaned.append(text)
    return ", ".join(cleaned)


def _creation_flags() -> int:
    """Prevent a console window flashing for every gh.exe request on Windows."""
    return getattr(subprocess, "CREATE_NO_WINDOW", 0) if sys.platform == "win32" else 0


class RunnerMonitor(tk.Tk):
    ACTIVE_STATUSES = {"queued", "in_progress", "waiting", "pending", "requested"}

    def __init__(self, config: dict[str, Any]) -> None:
        super().__init__()
        self.config_data = dict(config)
        self.title(f"RunnerScope {VERSION}")
        self.geometry("1480x780")
        self.minsize(1050, 600)
        self.protocol("WM_DELETE_WINDOW", self._on_close)
        apply_windows_dark_titlebar(self)

        self.stop_event = threading.Event()
        self.ui_queue: queue.Queue[tuple[Callable[..., Any], tuple[Any, ...]]] = queue.Queue()
        self.data_lock = threading.RLock()
        self.runner_refresh_in_progress = False
        self.activity_refresh_in_progress = False

        self.session_started = time.time()
        self.runner_session: dict[str, dict[str, Any]] = {}
        self.known_runner_names: set[str] = set()
        self.job_by_runner: dict[str, dict[str, Any]] = {}
        self.active_jobs: list[dict[str, Any]] = []
        self.seen_job_ids: set[str] = set()
        self.job_state: dict[str, dict[str, Any]] = {}
        self.observed_local_jobs = 0
        self.observed_hosted_jobs = 0
        self.history: deque[dict[str, Any]] = deque(maxlen=MAX_HISTORY)
        self._load_persistent_history()

        self.repo_cache: list[str] = []
        self.repo_cache_at = 0.0
        self.last_activity_scan = 0.0
        self.last_activity_error = ""
        self.last_runner_update = 0.0
        self.last_activity_update = 0.0
        self.last_scan_repo_errors: list[str] = []
        self.last_scan_job_errors: list[str] = []
        self.last_scan_repo_success = 0
        self.last_scan_job_success = 0

        self.runner_rows: list[dict[str, Any]] = []
        self.activity_rows: list[dict[str, Any]] = []
        self.local_health_rows: list[dict[str, Any]] = []
        self.local_health_refresh_in_progress = False
        self.last_local_health_update = 0.0
        self.last_local_health_error = ""
        self.sort_state: dict[str, tuple[str, bool]] = {}
        self.tree_row_maps: dict[str, dict[str, dict[str, Any]]] = {
            "runners": {},
            "activity": {},
            "history": {},
            "local": {},
        }

        self._configure_style()
        self._build_ui()
        self.after(100, self._drain_ui_queue)
        self.after(int(REFRESH_SECONDS * 1000), self._runner_timer)
        self.after(int(ACTIVITY_SECONDS * 1000), self._activity_timer)
        if sys.platform == "win32" or sys.platform.startswith("linux"):
            self.after(int(LOCAL_HEALTH_SECONDS * 1000), self._local_health_timer)
        self.after(1000, self._tick_timer)

        self.request_runner_refresh()
        self.request_activity_refresh(force=True)
        self.request_local_health_refresh()

    # ---------- UI ----------

    def _configure_style(self) -> None:
        fonts = configure_shared_theme(self)
        self.font_body = fonts["body"]
        self.font_body_bold = fonts["body_bold"]
        self.font_small = fonts["small"]
        self.font_small_bold = fonts["small_bold"]
        self.font_counter = fonts["counter"]
        self.font_brand = fonts["brand"]

    def _build_ui(self) -> None:
        outer = ttk.Frame(self, padding=12)
        outer.pack(fill=tk.BOTH, expand=True)

        heading = ttk.Frame(outer)
        heading.pack(fill=tk.X)
        ttk.Label(
            heading,
            text="RunnerScope",
            style="Title.TLabel",
        ).pack(side=tk.LEFT)
        self.updated_var = tk.StringVar(value="Runner data: —    Activity: —")
        ttk.Label(heading, textvariable=self.updated_var, style="Meta.TLabel").pack(
            side=tk.RIGHT, anchor=tk.S
        )

        meta = ttk.Frame(outer)
        meta.pack(fill=tk.X, pady=(5, 7))
        self.org_var = tk.StringVar(value=f"Organisation: {ORG}")
        ttk.Label(meta, textvariable=self.org_var).pack(side=tk.LEFT)
        self.scan_var = tk.StringVar(
            value=(
                f"Runner poll {REFRESH_SECONDS:g}s  •  Activity scan "
                f"{ACTIVITY_SECONDS:g}s  •  Up to {REPO_LIMIT} active repositories"
            )
        )
        ttk.Label(meta, textvariable=self.scan_var, style="Meta.TLabel").pack(
            side=tk.LEFT, padx=(22, 0)
        )

        counters = ttk.Frame(outer)
        counters.pack(fill=tk.X, pady=(0, 8))
        self.counter_vars: dict[str, tk.StringVar] = {}
        counter_specs = (
            ("TOTAL", "Counter.TLabel"),
            ("RUNNING", "Green.Counter.TLabel"),
            ("IDLE", "Blue.Counter.TLabel"),
            ("OFFLINE", "Red.Counter.TLabel"),
            ("LOCAL ACTIVE", "Green.Counter.TLabel"),
            ("GITHUB ACTIVE", "Purple.Counter.TLabel"),
            ("QUEUED", "Amber.Counter.TLabel"),
        )
        for name, label_style in counter_specs:
            card = ttk.Frame(counters, style="CounterCard.TFrame", padding=(10, 6))
            card.pack(side=tk.LEFT, padx=(0, 7))
            variable = tk.StringVar(value=f"{name}  0")
            self.counter_vars[name] = variable
            label = ttk.Label(card, textvariable=variable, style=label_style, cursor="hand2")
            label.pack()
            card.bind("<Button-1>", lambda _e, n=name: self._counter_filter(n))
            label.bind("<Button-1>", lambda _e, n=name: self._counter_filter(n))

        self.session_var = tk.StringVar(value="Observed this session: —")
        ttk.Label(outer, textvariable=self.session_var).pack(fill=tk.X, pady=(0, 7))

        filter_bar = ttk.Frame(outer)
        filter_bar.pack(fill=tk.X, pady=(0, 6))
        ttk.Label(filter_bar, text="Filter:").pack(side=tk.LEFT)
        self.filter_var = tk.StringVar()
        filter_entry = ttk.Entry(filter_bar, textvariable=self.filter_var, width=38)
        self.filter_entry = filter_entry
        filter_entry.pack(side=tk.LEFT, padx=(6, 6))
        self.filter_var.trace_add("write", lambda *_: self._apply_current_filter())
        ttk.Button(filter_bar, text="Clear", command=lambda: self.filter_var.set("")).pack(
            side=tk.LEFT
        )
        ttk.Label(
            filter_bar,
            text="Filters the selected tab. Click a column heading to sort.",
            style="Meta.TLabel",
        ).pack(side=tk.LEFT, padx=(14, 0))

        self.notebook = ttk.Notebook(outer)
        self.notebook.pack(fill=tk.BOTH, expand=True)
        self.notebook.bind("<<NotebookTabChanged>>", self._notebook_tab_changed)

        self.runner_tree = self._build_runner_tab()
        self.activity_tree = self._build_activity_tab()
        self.history_tree = self._build_history_tab()
        self.local_tree = self._build_local_health_tab()

        self.detail_var = tk.StringVar(value="Select a runner, job, or local service for details.")
        ttk.Label(outer, textvariable=self.detail_var, style="Detail.TLabel").pack(
            fill=tk.X, pady=(7, 0)
        )

        footer = ttk.Frame(outer)
        footer.pack(fill=tk.X, pady=(8, 0))
        self.status_var = tk.StringVar(value="Starting…")
        self.status_label = ttk.Label(footer, textvariable=self.status_var)
        self.status_label.pack(side=tk.LEFT, fill=tk.X, expand=True)
        ttk.Button(footer, text="Refresh now", command=self._manual_refresh).pack(
            side=tk.RIGHT
        )
        ttk.Button(footer, text="Settings", command=self._open_settings).pack(
            side=tk.RIGHT, padx=(0, 8)
        )
        ttk.Button(footer, text="Export CSV", command=self._export_selected_tab).pack(
            side=tk.RIGHT, padx=(0, 8)
        )
        self.restart_button = ttk.Button(
            footer,
            text="Restart selected runner",
            command=self._restart_selected_runner,
            state=tk.DISABLED,
        )
        self.restart_button.pack(side=tk.RIGHT, padx=(0, 8))
        self.diag_button = ttk.Button(
            footer, text="Open _diag", command=self._open_selected_diag, state=tk.DISABLED
        )
        self.diag_button.pack(side=tk.RIGHT, padx=(0, 8))
        self.open_button = ttk.Button(
            footer,
            text="Open selected job",
            command=self._open_selected_job,
            state=tk.DISABLED,
        )
        self.open_button.pack(side=tk.RIGHT, padx=(0, 8))

        self.bind("<F5>", lambda _e: self._manual_refresh())
        self.bind("<Control-f>", lambda _e: self.filter_entry.focus_set())
        self.bind("<Control-e>", lambda _e: self._export_selected_tab())
        self.bind("<Control-comma>", lambda _e: self._open_settings())

    def _open_settings(self) -> None:
        dialog = ConfigDialog(self, self.config_data, title="RunnerScope settings")
        self.wait_window(dialog)
        if not dialog.result:
            return
        save_config(dialog.result)
        self.config_data = dict(dialog.result)
        apply_config(self.config_data)
        self.history = deque(self.history, maxlen=MAX_HISTORY)
        self.repo_cache = []
        self.repo_cache_at = 0.0
        self.org_var.set(f"Organisation: {ORG}")
        self.scan_var.set(f"Runner poll {REFRESH_SECONDS:g}s  •  Activity scan {ACTIVITY_SECONDS:g}s  •  Up to {REPO_LIMIT} active repositories")
        self.status_var.set(f"Settings saved to {CONFIG_FILE}. Refreshing…")
        self.request_runner_refresh()
        self.request_activity_refresh(force=True)
        self.request_local_health_refresh()

    def _new_tree(
        self,
        parent: ttk.Frame,
        name: str,
        columns: tuple[tuple[str, str, int, str], ...],
    ) -> ttk.Treeview:
        tree = ttk.Treeview(parent, columns=[c[0] for c in columns], show="headings")
        tree.tag_configure("RUNNING", foreground=STATE_GREEN, font=self.font_small_bold)
        tree.tag_configure("IDLE", foreground=STATE_BLUE)
        tree.tag_configure("OFFLINE", foreground=STATE_RED, font=self.font_small_bold)
        tree.tag_configure("LOCAL", foreground=STATE_GREEN, font=self.font_small_bold)
        tree.tag_configure("GITHUB", foreground=STATE_PURPLE)
        tree.tag_configure("QUEUED", foreground=STATE_AMBER)
        tree.tag_configure("UNKNOWN", foreground=STATE_GREY)
        tree.tag_configure("SUCCESS", foreground=STATE_GREEN, font=self.font_small_bold)
        tree.tag_configure("FAILURE", foreground=STATE_RED, font=self.font_small_bold)
        tree.tag_configure("CANCELLED", foreground=STATE_AMBER)
        for key, title, width, anchor in columns:
            tree.heading(
                key,
                text=title,
                command=lambda col=key, table=name: self._sort_tree(table, col),
            )
            tree.column(key, width=width, minwidth=45, anchor=anchor, stretch=True)
        vertical = ttk.Scrollbar(parent, orient=tk.VERTICAL, command=tree.yview)
        horizontal = ttk.Scrollbar(parent, orient=tk.HORIZONTAL, command=tree.xview)
        tree.configure(yscrollcommand=vertical.set, xscrollcommand=horizontal.set)
        tree.grid(row=0, column=0, sticky="nsew")
        vertical.grid(row=0, column=1, sticky="ns")
        horizontal.grid(row=1, column=0, sticky="ew")
        parent.rowconfigure(0, weight=1)
        parent.columnconfigure(0, weight=1)
        return tree

    def _build_runner_tab(self) -> ttk.Treeview:
        frame = ttk.Frame(self.notebook)
        self.notebook.add(frame, text="Runners")
        columns = (
            ("name", "Runner", 190, tk.W),
            ("os", "OS", 75, tk.W),
            ("state", "State", 90, tk.W),
            ("repo", "Repository", 150, tk.W),
            ("job", "Current job", 260, tk.W),
            ("runtime", "Runtime / queue", 105, tk.W),
            ("state_for", "State for", 85, tk.W),
            ("jobs", "Jobs", 55, tk.E),
            ("busy_pct", "Busy", 60, tk.E),
            ("labels", "Labels", 330, tk.W),
        )
        tree = self._new_tree(frame, "runners", columns)
        tree.bind("<<TreeviewSelect>>", self._runner_selection_changed)
        return tree

    def _build_activity_tab(self) -> ttk.Treeview:
        frame = ttk.Frame(self.notebook)
        self.notebook.add(frame, text="Active jobs")
        columns = (
            ("environment", "Where", 115, tk.W),
            ("repo", "Repository", 175, tk.W),
            ("workflow", "Workflow", 210, tk.W),
            ("job", "Job", 240, tk.W),
            ("step", "Current step", 230, tk.W),
            ("status", "Status", 95, tk.W),
            ("runner", "Runner", 185, tk.W),
            ("runtime", "Runtime / queue", 105, tk.W),
            ("event", "Event", 75, tk.W),
            ("branch", "Branch", 120, tk.W),
        )
        tree = self._new_tree(frame, "activity", columns)
        tree.bind("<<TreeviewSelect>>", self._activity_selection_changed)
        tree.bind("<Double-1>", self._activity_double_click)
        return tree

    def _build_history_tab(self) -> ttk.Treeview:
        frame = ttk.Frame(self.notebook)
        self.notebook.add(frame, text="History")
        columns = (
            ("time_text", "Time", 95, tk.W),
            ("runner", "Runner", 210, tk.W),
            ("event", "Event", 105, tk.W),
            ("detail", "Detail", 800, tk.W),
        )
        return self._new_tree(frame, "history", columns)

    def _build_local_health_tab(self) -> ttk.Treeview:
        frame = ttk.Frame(self.notebook)
        local_label = "Local Windows health" if sys.platform == "win32" else "Local Linux health"
        self.notebook.add(frame, text=local_label)
        columns = (
            ("runner", "Runner", 190, tk.W),
            ("service_state", "Service", 85, tk.W),
            ("github_state", "GitHub", 80, tk.W),
            ("pid", "PID", 65, tk.E),
            ("start_mode", "Start", 80, tk.W),
            ("account", "Account", 140, tk.W),
            ("diag", "Latest diagnostic", 240, tk.W),
            ("diag_age", "Diag age", 85, tk.W),
            ("path", "Runner path", 360, tk.W),
        )
        tree = self._new_tree(frame, "local", columns)
        tree.bind("<<TreeviewSelect>>", self._local_selection_changed)
        tree.bind("<Double-1>", lambda _event: self._open_selected_diag())
        return tree

    # ---------- timers / thread hand-off ----------

    def _post(self, callback: Callable[..., Any], *args: Any) -> None:
        if not self.stop_event.is_set():
            self.ui_queue.put((callback, args))

    def _drain_ui_queue(self) -> None:
        try:
            while True:
                callback, args = self.ui_queue.get_nowait()
                callback(*args)
        except queue.Empty:
            pass
        if not self.stop_event.is_set():
            self.after(100, self._drain_ui_queue)

    def _runner_timer(self) -> None:
        if not self.stop_event.is_set():
            self.request_runner_refresh()
            self.after(int(REFRESH_SECONDS * 1000), self._runner_timer)

    def _activity_timer(self) -> None:
        if not self.stop_event.is_set():
            self.request_activity_refresh()
            self.after(int(ACTIVITY_SECONDS * 1000), self._activity_timer)

    def _local_health_timer(self) -> None:
        """Periodically refresh Windows runner service/diagnostic health."""
        if not self.stop_event.is_set():
            self.request_local_health_refresh()
            self.after(int(LOCAL_HEALTH_SECONDS * 1000), self._local_health_timer)

    def _tick_timer(self) -> None:
        if not self.stop_event.is_set():
            self._refresh_runtime_values()
            self.after(1000, self._tick_timer)

    def _on_close(self) -> None:
        self.stop_event.set()
        with self.data_lock:
            self._save_persistent_history_locked()
        self.destroy()

    def _manual_refresh(self) -> None:
        self.request_runner_refresh()
        self.request_activity_refresh(force=True)
        self.request_local_health_refresh()

    # ---------- GitHub CLI ----------

    def _run_gh(self, args: list[str], timeout: int = 25) -> str:
        executable = find_gh()
        if not executable:
            raise RuntimeError(
                "GitHub CLI (gh.exe) is not installed or is not on PATH. "
                "Install it, then run: gh auth login"
            )
        try:
            proc = subprocess.run(
                [executable, *args],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=timeout,
                creationflags=_creation_flags(),
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            raise RuntimeError(f"GitHub CLI timed out after {timeout} seconds") from exc
        if proc.returncode != 0:
            detail = (proc.stderr or proc.stdout).strip()
            if "403" in detail or "Resource not accessible" in detail:
                detail += (
                    "  Runner access may require: "
                    "gh auth refresh -h github.com -s admin:org"
                )
            raise RuntimeError(detail or "GitHub CLI command failed")
        return proc.stdout

    def _gh_json(self, endpoint: str, timeout: int = 25) -> Any:
        raw = self._run_gh(
            ["api", "-H", "Accept: application/vnd.github+json", endpoint],
            timeout=timeout,
        )
        try:
            return json.loads(raw)
        except json.JSONDecodeError as exc:
            raise RuntimeError("GitHub CLI returned invalid JSON") from exc

    # ---------- fast runner polling ----------

    def request_runner_refresh(self) -> None:
        with self.data_lock:
            if self.runner_refresh_in_progress or self.stop_event.is_set():
                return
            self.runner_refresh_in_progress = True
        threading.Thread(target=self._runner_worker, daemon=True).start()

    def _runner_worker(self) -> None:
        try:
            data = self._gh_json(f"/orgs/{ORG}/actions/runners?per_page=100")
            now = time.time()
            raw_runners = data.get("runners", [])
            rows: list[dict[str, Any]] = []
            activity_needed = False

            with self.data_lock:
                self.known_runner_names = {
                    runner.get("name") or "Unnamed runner" for runner in raw_runners
                }
                for runner in raw_runners:
                    name = runner.get("name") or "Unnamed runner"
                    status = runner.get("status") or "unknown"
                    busy = bool(runner.get("busy"))
                    if status != "online":
                        state = "OFFLINE"
                    elif busy:
                        state = "RUNNING"
                    else:
                        state = "IDLE"

                    session = self.runner_session.setdefault(
                        name,
                        {
                            "state": None,
                            "state_since": now,
                            "busy_started": None,
                            "busy_seconds": 0.0,
                            "jobs": 0,
                            "last_seen_online": None,
                            "last_job": "",
                            "last_repo": "",
                        },
                    )
                    previous = session["state"]
                    if previous != state:
                        if previous == "RUNNING" and session["busy_started"]:
                            session["busy_seconds"] += now - session["busy_started"]
                            session["busy_started"] = None
                        if state == "RUNNING":
                            session["busy_started"] = now
                            session["jobs"] += 1
                            # Never show the previous job while GitHub has already
                            # reported a new busy transition.  The activity scan
                            # will resolve the fresh job assignment.
                            self.job_by_runner.pop(name, None)
                            activity_needed = True
                        session["state"] = state
                        session["state_since"] = now
                        current_job = self.job_by_runner.get(name)
                        detail = self._job_detail(current_job) if current_job else ""
                        self._append_history_locked(name, state, detail, state)

                    if status == "online":
                        session["last_seen_online"] = now

                    job = self.job_by_runner.get(name)
                    if job:
                        session["last_job"] = job.get("job", "")
                        session["last_repo"] = job.get("repo", "")

                    busy_seconds = float(session["busy_seconds"])
                    if state == "RUNNING" and session["busy_started"]:
                        busy_seconds += now - session["busy_started"]
                    elapsed = max(1.0, now - self.session_started)
                    busy_pct = min(100.0, max(0.0, busy_seconds * 100.0 / elapsed))

                    if state == "RUNNING" and job:
                        repo = job.get("repo") or "—"
                        job_text = self._job_display(job)
                        step = job.get("step")
                        if step and step != "—":
                            job_text += f" › {step}"
                        started = job.get("started_ts")
                        runtime = fmt_duration(now - started) if started else "—"
                    elif state == "RUNNING":
                        repo = "Resolving…"
                        job_text = "GitHub reports runner busy"
                        runtime = fmt_duration(now - session["state_since"])
                    else:
                        repo = session.get("last_repo") or "—"
                        job_text = (
                            "Last: " + session["last_job"] if session.get("last_job") else "—"
                        )
                        runtime = "—"

                    rows.append(
                        {
                            "name": name,
                            "os": runner.get("os") or "—",
                            "state": state,
                            "repo": repo,
                            "job": job_text,
                            "runtime": runtime,
                            "state_for": fmt_duration(now - session["state_since"]),
                            "jobs": int(session["jobs"]),
                            "busy_pct": f"{busy_pct:.1f}%",
                            "labels": short_labels(runner.get("labels")),
                        }
                    )

            rows.sort(key=lambda row: row["name"].casefold())
            total = len(rows)
            running = sum(row["state"] == "RUNNING" for row in rows)
            idle = sum(row["state"] == "IDLE" for row in rows)
            offline = sum(row["state"] == "OFFLINE" for row in rows)
            self.last_runner_update = now
            self._post(self._apply_runner_data, rows, total, running, idle, offline)

            if activity_needed and now - self.last_activity_scan > 4:
                self.request_activity_refresh(force=True)
        except Exception as exc:  # noqa: BLE001 - error must reach the GUI.
            self._post(self._show_runner_error, str(exc))
        finally:
            with self.data_lock:
                self.runner_refresh_in_progress = False

    # ---------- slower job/activity scan ----------

    def request_activity_refresh(self, force: bool = False) -> None:
        with self.data_lock:
            if self.activity_refresh_in_progress or self.stop_event.is_set():
                return
            if not force and time.time() - self.last_activity_scan < min(5.0, ACTIVITY_SECONDS):
                return
            self.activity_refresh_in_progress = True
        threading.Thread(target=self._activity_worker, daemon=True).start()

    def _get_repositories(self) -> list[str]:
        now = time.time()
        with self.data_lock:
            if self.repo_cache and now - self.repo_cache_at < REPO_CACHE_SECONDS:
                return list(self.repo_cache)
        repos = self._gh_json(
            f"/orgs/{ORG}/repos?type=all&sort=pushed&direction=desc&per_page=100"
        )
        names = [
            repo["name"]
            for repo in repos
            if repo.get("name") and not repo.get("archived") and not repo.get("disabled")
        ][:REPO_LIMIT]
        with self.data_lock:
            self.repo_cache = names
            self.repo_cache_at = now
        return list(names)

    def _activity_worker(self) -> None:
        try:
            self.last_activity_scan = time.time()
            repos = self._get_repositories()
            repo_errors: list[str] = []
            job_errors: list[str] = []
            runs: list[dict[str, Any]] = []
            workers = min(6, max(1, len(repos)))
            with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
                futures = {pool.submit(self._fetch_repo_runs, repo): repo for repo in repos}
                for future in concurrent.futures.as_completed(futures):
                    if self.stop_event.is_set():
                        return
                    repo = futures[future]
                    try:
                        runs.extend(future.result())
                    except Exception as exc:
                        repo_errors.append(f"{repo}: {exc}")

            # Deduplicate runs before job retrieval.
            deduped: dict[tuple[str, str], dict[str, Any]] = {}
            for run in runs:
                deduped[(str(run.get("repo")), str(run.get("run_id")))] = run
            runs = list(deduped.values())

            jobs: list[dict[str, Any]] = []
            with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
                futures = {pool.submit(self._fetch_run_jobs, run): run for run in runs}
                for future in concurrent.futures.as_completed(futures):
                    if self.stop_event.is_set():
                        return
                    run = futures[future]
                    try:
                        jobs.extend(future.result())
                    except Exception as exc:
                        job_errors.append(f"{run.get('repo', '—')}#{run.get('run_id', '—')}: {exc}")

            # The broad 100-run scan is normally enough.  If GitHub says a known
            # local runner is busy but its job is still unresolved, make one
            # targeted status=in_progress pass.  This keeps API usage low while
            # removing the old 'latest eight runs' blind spot.
            with self.data_lock:
                busy_names = {
                    name for name, state in self.runner_session.items()
                    if state.get("state") == "RUNNING"
                }
            matched_names = {
                str(job.get("runner_name")) for job in jobs
                if job.get("status") == "in_progress" and job.get("runner_name")
            }
            if busy_names - matched_names and repos:
                fallback_runs: list[dict[str, Any]] = []
                with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
                    futures = {
                        pool.submit(self._fetch_repo_runs, repo, "in_progress"): repo
                        for repo in repos
                    }
                    for future in concurrent.futures.as_completed(futures):
                        if self.stop_event.is_set():
                            return
                        repo = futures[future]
                        try:
                            fallback_runs.extend(future.result())
                        except Exception as exc:
                            repo_errors.append(f"{repo} fallback: {exc}")
                extra = []
                known_run_keys = {(str(r.get("repo")), str(r.get("run_id"))) for r in runs}
                for run in fallback_runs:
                    key = (str(run.get("repo")), str(run.get("run_id")))
                    if key not in known_run_keys:
                        known_run_keys.add(key)
                        extra.append(run)
                if extra:
                    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
                        futures = {pool.submit(self._fetch_run_jobs, run): run for run in extra}
                        for future in concurrent.futures.as_completed(futures):
                            if self.stop_event.is_set():
                                return
                            run = futures[future]
                            try:
                                jobs.extend(future.result())
                            except Exception as exc:
                                job_errors.append(
                                    f"{run.get('repo', '—')}#{run.get('run_id', '—')}: {exc}"
                                )

            now = time.time()
            rows: list[dict[str, Any]] = []
            job_by_runner: dict[str, dict[str, Any]] = {}
            local_active = hosted_active = queued = 0

            with self.data_lock:
                known = set(self.known_runner_names)
                previous_job_state = {key: dict(value) for key, value in self.job_state.items()}
                for job in jobs:
                    status = str(job.get("status") or "unknown")
                    if status == "completed":
                        continue
                    environment = self._classify_job_environment(job, known)
                    job_id = str(job.get("id") or "")
                    if status == "in_progress":
                        if environment == "LOCAL":
                            local_active += 1
                        elif environment == "GITHUB":
                            hosted_active += 1
                    else:
                        queued += 1

                    runner_name = job.get("runner_name") or "—"
                    started_ts = parse_github_time(job.get("started_at"))
                    if not started_ts:
                        started_ts = parse_github_time(job.get("run_started_at"))
                    if started_ts:
                        age = fmt_duration(now - started_ts)
                        runtime = age if status == "in_progress" else f"queue {age}"
                    else:
                        runtime = "—"
                    display_environment = environment
                    if status != "in_progress":
                        display_environment += " QUEUE"
                    row = {
                        "environment": display_environment,
                        "repo": job.get("repo") or "—",
                        "workflow": job.get("workflow") or "—",
                        "job": job.get("name") or "—",
                        "step": current_job_step(job) if status == "in_progress" else "—",
                        "status": status.upper(),
                        "runner": runner_name,
                        "runtime": runtime,
                        "event": job.get("event") or "—",
                        "branch": job.get("branch") or "—",
                        "url": job.get("html_url") or job.get("run_url") or "",
                        "started_ts": started_ts,
                        "job_id": job_id,
                    }
                    rows.append(row)
                    if status == "in_progress" and runner_name in known:
                        job_by_runner[runner_name] = dict(row)

                    previous = previous_job_state.get(job_id) if job_id else None
                    if job_id and job_id not in self.seen_job_ids:
                        self.seen_job_ids.add(job_id)
                        if environment == "LOCAL":
                            self.observed_local_jobs += 1
                        elif environment == "GITHUB":
                            self.observed_hosted_jobs += 1
                    if job_id and previous is None:
                        event = "START" if status == "in_progress" else "QUEUED"
                        self._append_history_locked(
                            runner_name if runner_name != "—" else environment,
                            event,
                            self._job_detail(row),
                            environment if event == "START" else "QUEUED",
                        )
                    elif previous and previous.get("status") != row["status"]:
                        if row["status"] == "IN_PROGRESS":
                            self._append_history_locked(
                                runner_name if runner_name != "—" else environment,
                                "START",
                                self._job_detail(row),
                                environment,
                            )

                current_ids = {row["job_id"] for row in rows if row.get("job_id")}
                disappeared = [
                    dict(old) for job_id, old in previous_job_state.items()
                    if job_id and job_id not in current_ids
                ]
                self.job_by_runner = job_by_runner
                self.active_jobs = rows
                self.job_state = {
                    str(row["job_id"]): dict(row) for row in rows if row.get("job_id")
                }

            # Resolve the actual conclusion only for jobs that disappeared from
            # the active set.  This adds API calls only at job completion and
            # yields a useful SUCCESS/FAILURE/CANCELLED lifecycle history.
            for old in disappeared:
                completion = self._fetch_job_completion(old)
                with self.data_lock:
                    if completion:
                        conclusion, tag = completion
                        self._append_history_locked(
                            old.get("runner") or old.get("environment") or "—",
                            conclusion,
                            self._job_detail(old),
                            tag,
                        )
                    else:
                        # The job may only be missing because one part of a
                        # partial scan failed. Preserve its prior state until a
                        # later scan or the direct job endpoint confirms finish.
                        job_id = str(old.get("job_id") or "")
                        if job_id:
                            self.job_state.setdefault(job_id, old)

            rank = {
                "LOCAL": 0,
                "LOCAL QUEUE": 1,
                "GITHUB": 2,
                "GITHUB QUEUE": 3,
                "UNKNOWN": 4,
                "UNKNOWN QUEUE": 5,
            }
            rows.sort(
                key=lambda row: (
                    rank.get(row["environment"], 9),
                    row["repo"].casefold(),
                    row["job"].casefold(),
                )
            )
            self.last_activity_update = now
            self.last_scan_repo_errors = repo_errors
            self.last_scan_job_errors = job_errors
            self.last_scan_repo_success = max(0, len(repos) - len({e.split(':', 1)[0] for e in repo_errors}))
            self.last_scan_job_success = max(0, len(runs) - len(job_errors))
            error_count = len(repo_errors) + len(job_errors)
            self.last_activity_error = (
                f"{error_count} partial scan error(s)" if error_count else ""
            )
            self._post(
                self._apply_activity_data,
                rows,
                local_active,
                hosted_active,
                queued,
                len(repos),
            )
            self.request_runner_refresh()
        except Exception as exc:  # noqa: BLE001 - error must reach the GUI.
            self.last_activity_error = str(exc)
            self._post(self._show_activity_error, str(exc))
        finally:
            with self.data_lock:
                self.activity_refresh_in_progress = False

    def _fetch_repo_runs(
        self, repo: str, status: str | None = None
    ) -> list[dict[str, Any]]:
        endpoint = f"/repos/{ORG}/{repo}/actions/runs?per_page=100&exclude_pull_requests=true"
        if status:
            endpoint += f"&status={status}"
        data = self._gh_json(endpoint)
        result = []
        for run in data.get("workflow_runs", []):
            if run.get("status") not in self.ACTIVE_STATUSES:
                continue
            result.append(
                {
                    "repo": repo,
                    "run_id": run.get("id"),
                    "workflow": run.get("name") or run.get("display_title") or "Workflow",
                    "event": run.get("event") or "—",
                    "branch": run.get("head_branch") or "—",
                    "run_started_at": run.get("run_started_at") or run.get("created_at"),
                    "run_url": run.get("html_url") or "",
                }
            )
        return result

    def _fetch_job_completion(self, row: dict[str, Any]) -> tuple[str, str] | None:
        job_id = row.get("job_id")
        repo = row.get("repo")
        if not job_id or not repo or repo == "—":
            return None
        try:
            job = self._gh_json(f"/repos/{ORG}/{repo}/actions/jobs/{job_id}")
            if job.get("status") != "completed":
                return None
            conclusion = str(job.get("conclusion") or "completed").upper()
            tag = "SUCCESS" if conclusion == "SUCCESS" else (
                "CANCELLED" if conclusion == "CANCELLED" else "FAILURE"
            )
            return conclusion, tag
        except Exception:
            return None

    def _fetch_run_jobs(self, run: dict[str, Any]) -> list[dict[str, Any]]:
        run_id = run.get("run_id")
        if not run_id:
            return []
        data = self._gh_json(
            f"/repos/{ORG}/{run['repo']}/actions/runs/{run_id}/jobs?per_page=100&filter=latest"
        )
        result = []
        for job in data.get("jobs", []):
            row = dict(job)
            row.update(
                {
                    "repo": run["repo"],
                    "workflow": run["workflow"],
                    "event": run["event"],
                    "branch": run["branch"],
                    "run_started_at": run["run_started_at"],
                    "run_url": run["run_url"],
                }
            )
            result.append(row)
        return result

    @staticmethod
    def _classify_job_environment(job: dict[str, Any], known: set[str]) -> str:
        runner_name = job.get("runner_name") or ""
        runner_group = job.get("runner_group_name") or ""
        labels = {str(label).lower() for label in (job.get("labels") or [])}
        if runner_name in known or "self-hosted" in labels:
            return "LOCAL"
        if runner_group == "GitHub Actions":
            return "GITHUB"
        if any(
            label.endswith("-latest")
            or label.startswith("ubuntu-")
            or label.startswith("windows-")
            or label.startswith("macos-")
            for label in labels
        ):
            return "GITHUB"
        return "UNKNOWN"

    # ---------- display/model handling ----------

    def _apply_runner_data(
        self,
        rows: list[dict[str, Any]],
        total: int,
        running: int,
        idle: int,
        offline: int,
    ) -> None:
        self.runner_rows = rows
        self._set_counter("TOTAL", total)
        self._set_counter("RUNNING", running)
        self._set_counter("IDLE", idle)
        self._set_counter("OFFLINE", offline)
        self._render_runner_rows()
        self._update_summary()
        if EXPECTED_RUNNERS and total != EXPECTED_RUNNERS:
            base = f"GitHub reports {total} runners; expected {EXPECTED_RUNNERS}."
        elif EXPECTED_RUNNERS:
            base = (
                f"All {EXPECTED_RUNNERS} runners detected. "
                f"{running} running, {idle} idle, {offline} offline."
            )
        else:
            base = f"GitHub reports {total} runners."
        if self.last_activity_error:
            base += f"  Activity scan warning: {self.last_activity_error}."
        if self.last_local_health_error:
            base += f"  Local health warning: {self.last_local_health_error}."
        self.status_var.set(base)

    def _apply_activity_data(
        self,
        rows: list[dict[str, Any]],
        local_active: int,
        hosted_active: int,
        queued: int,
        repo_count: int,
    ) -> None:
        self.activity_rows = rows
        self._set_counter("LOCAL ACTIVE", local_active)
        self._set_counter("GITHUB ACTIVE", hosted_active)
        self._set_counter("QUEUED", queued)
        scan_errors = len(self.last_scan_repo_errors) + len(self.last_scan_job_errors)
        coverage = f"{self.last_scan_repo_success}/{repo_count} repositories"
        if scan_errors:
            coverage += f"  •  {scan_errors} scan error(s)"
        else:
            coverage += "  •  complete"
        self.scan_var.set(
            f"Runner poll {REFRESH_SECONDS:g}s  •  Activity scan {ACTIVITY_SECONDS:g}s  •  "
            f"{coverage}"
        )
        self._render_activity_rows()
        self._render_history_rows()
        self._update_summary()

    def _set_counter(self, name: str, value: int) -> None:
        self.counter_vars[name].set(f"{name}  {value}")

    def _filtered(self, rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
        needle = self.filter_var.get().strip().casefold()
        if not needle:
            return list(rows)
        return [
            row
            for row in rows
            if needle in " ".join(str(value) for value in row.values()).casefold()
        ]

    def _apply_current_filter(self) -> None:
        selected = self.notebook.index(self.notebook.select())
        if selected == 0:
            self._render_runner_rows()
        elif selected == 1:
            self._render_activity_rows()
        elif selected == 2:
            self._render_history_rows()
        else:
            self._render_local_health_rows()

    def _replace_tree_rows(
        self,
        table: str,
        tree: ttk.Treeview,
        rows: list[dict[str, Any]],
        columns: tuple[str, ...],
        tagger: Callable[[dict[str, Any]], str],
    ) -> None:
        selection = tree.selection()
        children = tree.get_children()
        if children:
            tree.delete(*children)
        row_map: dict[str, dict[str, Any]] = {}
        for row in rows:
            item_id = f"{table}-{id(row)}"
            tree.insert(
                "",
                tk.END,
                iid=item_id,
                values=[row.get(column, "") for column in columns],
                tags=(tagger(row),),
            )
            row_map[item_id] = row
        self.tree_row_maps[table] = row_map
        surviving = [item_id for item_id in selection if item_id in row_map]
        if surviving:
            tree.selection_set(surviving)
            tree.focus(surviving[0])

    def _render_runner_rows(self) -> None:
        columns = tuple(self.runner_tree["columns"])
        rows = self._sort_rows("runners", self._filtered(self.runner_rows))
        self._replace_tree_rows(
            "runners",
            self.runner_tree,
            rows,
            columns,
            lambda row: row.get("state", "UNKNOWN"),
        )

    def _render_activity_rows(self) -> None:
        columns = tuple(self.activity_tree["columns"])
        rows = self._sort_rows("activity", self._filtered(self.activity_rows))

        def tag(row: dict[str, Any]) -> str:
            if row.get("status") != "IN_PROGRESS":
                return "QUEUED"
            return str(row.get("environment", "UNKNOWN")).split()[0]

        self._replace_tree_rows("activity", self.activity_tree, rows, columns, tag)
        self._activity_selection_changed()

    def _render_history_rows(self) -> None:
        with self.data_lock:
            all_rows = list(self.history)
        columns = tuple(self.history_tree["columns"])
        rows = self._sort_rows("history", self._filtered(all_rows))
        self._replace_tree_rows(
            "history",
            self.history_tree,
            rows,
            columns,
            lambda row: row.get("tag", "UNKNOWN"),
        )

    def _sort_tree(self, table: str, column: str) -> None:
        previous = self.sort_state.get(table)
        descending = bool(previous and previous[0] == column and not previous[1])
        self.sort_state[table] = (column, descending)
        if table == "runners":
            self._render_runner_rows()
        elif table == "activity":
            self._render_activity_rows()
        elif table == "history":
            self._render_history_rows()
        else:
            self._render_local_health_rows()

    def _sort_rows(self, table: str, rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
        state = self.sort_state.get(table)
        if not state:
            return rows
        column, descending = state

        def key(row: dict[str, Any]) -> tuple[int, Any]:
            value = row.get(column, "")
            if isinstance(value, (int, float)):
                return (0, value)
            text = str(value)
            if text.endswith("%"):
                try:
                    return (0, float(text[:-1]))
                except ValueError:
                    pass
            return (1, text.casefold())

        return sorted(rows, key=key, reverse=descending)

    def _refresh_runtime_values(self) -> None:
        now = time.time()
        changed = False
        with self.data_lock:
            sessions = {name: dict(data) for name, data in self.runner_session.items()}
            jobs = {name: dict(data) for name, data in self.job_by_runner.items()}
            for row in self.runner_rows:
                session = sessions.get(row["name"])
                if not session:
                    continue
                row["state_for"] = fmt_duration(now - session.get("state_since", now))
                busy_seconds = float(session.get("busy_seconds", 0.0))
                if row["state"] == "RUNNING" and session.get("busy_started"):
                    busy_seconds += now - session["busy_started"]
                elapsed = max(1.0, now - self.session_started)
                row["busy_pct"] = f"{min(100.0, busy_seconds * 100.0 / elapsed):.1f}%"
                job = jobs.get(row["name"])
                if row["state"] == "RUNNING" and job and job.get("started_ts"):
                    row["runtime"] = fmt_duration(now - job["started_ts"])
                changed = True
            for row in self.activity_rows:
                if row["status"] == "IN_PROGRESS" and row.get("started_ts"):
                    row["runtime"] = fmt_duration(now - row["started_ts"])
                    changed = True
        if changed:
            self._render_runner_rows()
            self._render_activity_rows()
        self._update_summary()

    def _update_summary(self) -> None:
        local = self.observed_local_jobs
        hosted = self.observed_hosted_jobs
        observed = local + hosted
        if observed:
            self.session_var.set(
                f"Observed this session: self-hosted {local} jobs  •  "
                f"GitHub-hosted {hosted} jobs  •  self-hosted share "
                f"{local * 100.0 / observed:.0f}%  •  "
                f"monitor up {fmt_duration(time.time() - self.session_started)}"
            )
        else:
            self.session_var.set(
                "Observed this session: no active jobs discovered yet  •  "
                f"monitor up {fmt_duration(time.time() - self.session_started)}"
            )
        self.updated_var.set(
            f"Runner data: {fmt_clock(self.last_runner_update)}    "
            f"Activity: {fmt_clock(self.last_activity_update)}"
        )

    def _counter_filter(self, name: str) -> None:
        if name in {"TOTAL", "RUNNING", "IDLE", "OFFLINE"}:
            self.notebook.select(0)
            self.filter_var.set("" if name == "TOTAL" else name)
        else:
            self.notebook.select(1)
            mapping = {"LOCAL ACTIVE": "LOCAL", "GITHUB ACTIVE": "GITHUB", "QUEUED": "QUEUE"}
            self.filter_var.set(mapping.get(name, ""))

    def _notebook_tab_changed(self, _event: tk.Event[Any] | None = None) -> None:
        self._apply_current_filter()
        self._update_restart_button()

    def _selected_runner_row(self) -> dict[str, Any] | None:
        selection = self.runner_tree.selection()
        if not selection:
            return None
        return self.tree_row_maps["runners"].get(selection[0])

    def _runner_selection_changed(self, _event: tk.Event[Any] | None = None) -> None:
        row = self._selected_runner_row()
        if not row:
            self._update_restart_button()
            return
        self.detail_var.set(
            f"{row.get('name', '—')}  •  {row.get('state', '—')}  •  "
            f"{row.get('repo', '—')}  •  {row.get('job', '—')}  •  "
            f"labels: {row.get('labels', '—')}"
        )
        self._update_restart_button()

    def _update_restart_button(self) -> None:
        if not hasattr(self, "restart_button"):
            return
        try:
            selected_tab = self.notebook.index(self.notebook.select())
        except tk.TclError:
            self.restart_button.configure(state=tk.DISABLED)
            return
        enabled = False
        if selected_tab == 0:
            row = self._selected_runner_row()
            if row:
                os_name = str(row.get("os", "")).casefold()
                platform_matches = (sys.platform == "win32" and os_name == "windows") or (sys.platform.startswith("linux") and os_name == "linux")
                enabled = platform_matches and any(str(local.get("runner", "")).casefold() == str(row.get("name", "")).casefold() and local.get("service_name") for local in self.local_health_rows)
        elif selected_tab == 3:
            row = self._selected_local_row()
            enabled = bool(row and row.get("service_name"))
        self.restart_button.configure(state=tk.NORMAL if enabled else tk.DISABLED)

    def request_local_health_refresh(self) -> None:
        if sys.platform != "win32" and not sys.platform.startswith("linux"):
            return
        with self.data_lock:
            if self.local_health_refresh_in_progress or self.stop_event.is_set():
                return
            self.local_health_refresh_in_progress = True
        threading.Thread(target=self._local_health_worker, daemon=True).start()

    def _local_health_worker(self) -> None:
        try:
            rows = self._windows_local_health_rows() if sys.platform == "win32" else self._linux_local_health_rows()
            now = time.time()
            rows.sort(key=lambda row: str(row.get("runner", "")).casefold())
            self.local_health_rows = rows
            self.last_local_health_update = now
            self.last_local_health_error = ""
            self._post(self._render_local_health_rows)
        except Exception as exc:
            self.last_local_health_error = str(exc)
            self._post(self._show_local_health_error, str(exc))
        finally:
            with self.data_lock:
                self.local_health_refresh_in_progress = False

    def _windows_local_health_rows(self) -> list[dict[str, Any]]:
        powershell = shutil.which("pwsh.exe") or shutil.which("powershell.exe")
        if not powershell:
            raise RuntimeError("PowerShell is unavailable")
        command = (
            "Get-CimInstance Win32_Service | "
            "Where-Object {$_.Name -like 'actions.runner.*'} | "
            "Select-Object Name,DisplayName,State,ProcessId,StartMode,PathName,StartName | "
            "ConvertTo-Json -Compress"
        )
        proc = subprocess.run([powershell, "-NoProfile", "-NonInteractive", "-Command", command], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace", timeout=20, creationflags=_creation_flags(), check=False)
        if proc.returncode != 0:
            raise RuntimeError((proc.stderr or proc.stdout).strip() or "PowerShell service query failed")
        raw = proc.stdout.strip()
        payload: Any = json.loads(raw) if raw else []
        services = payload if isinstance(payload, list) else [payload]
        now = time.time()
        github_states = {row.get("name"): row.get("state") for row in self.runner_rows}
        known_names = set(github_states)
        rows: list[dict[str, Any]] = []
        for service in services:
            display = str(service.get("DisplayName") or service.get("Name") or "—")
            service_name = str(service.get("Name") or "—")
            runner_name = next((name for name in known_names if name and (name in display or name in service_name)), "—")
            path_name = str(service.get("PathName") or "")
            executable = extract_service_executable(path_name)
            root: Path | None = None
            if executable:
                parent = executable.parent
                root = parent.parent if parent.name.casefold() == "bin" else parent
            rows.append(self._make_local_health_row(now, runner_name if runner_name != "—" else display, service_name, str(service.get("State") or "Unknown").upper(), github_states.get(runner_name, "—"), int(service.get("ProcessId") or 0) or "—", service.get("StartMode") or "—", service.get("StartName") or "—", root, path_name))
        return rows

    def _linux_local_health_rows(self) -> list[dict[str, Any]]:
        systemctl = shutil.which("systemctl")
        if not systemctl:
            raise RuntimeError("systemctl is unavailable")
        proc = subprocess.run([systemctl, "list-units", "--all", "--type=service", "--no-legend", "--plain", "actions.runner.*"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace", timeout=20, check=False)
        if proc.returncode != 0:
            raise RuntimeError((proc.stderr or proc.stdout).strip() or "systemd service query failed")
        service_names = [line.split()[0] for line in proc.stdout.splitlines() if line.strip() and line.split()[0].startswith("actions.runner.")]
        now = time.time()
        github_states = {row.get("name"): row.get("state") for row in self.runner_rows}
        known_names = set(github_states)
        rows: list[dict[str, Any]] = []
        for service_name in service_names:
            show = subprocess.run([systemctl, "show", service_name, "--no-pager", "-p", "Id", "-p", "Description", "-p", "ActiveState", "-p", "SubState", "-p", "MainPID", "-p", "UnitFileState", "-p", "User", "-p", "ExecStart"], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace", timeout=10, check=False)
            if show.returncode != 0:
                continue
            props: dict[str, str] = {}
            for line in show.stdout.splitlines():
                if "=" in line:
                    key, value = line.split("=", 1)
                    props[key] = value
            display = props.get("Description") or service_name
            runner_name = next((name for name in known_names if name and (name.casefold() in display.casefold() or name.casefold() in service_name.casefold())), "—")
            exec_start = props.get("ExecStart", "")
            path_match = re.search(r"path=([^ ;}]+)", exec_start) or re.search(r"(/[^ ;}]+)", exec_start)
            executable = Path(path_match.group(1)) if path_match else None
            root: Path | None = None
            if executable:
                if executable.name in {"runsvc.sh", "svc.sh", "run.sh"}:
                    root = executable.parent
                elif executable.parent.name.casefold() == "bin":
                    root = executable.parent.parent
                else:
                    root = executable.parent
            active = props.get("ActiveState", "unknown")
            service_state = "RUNNING" if active == "active" else active.upper()
            pid = int(props.get("MainPID") or 0) or "—"
            rows.append(self._make_local_health_row(now, runner_name if runner_name != "—" else display, service_name, service_state, github_states.get(runner_name, "—"), pid, props.get("UnitFileState") or "—", props.get("User") or "—", root, exec_start))
        return rows

    def _make_local_health_row(self, now: float, runner: str, service_name: str, service_state: str, github_state: str, pid: Any, start_mode: Any, account: Any, root: Path | None, fallback_path: str) -> dict[str, Any]:
        diag_dir = root / "_diag" if root else None
        newest: Path | None = None
        if diag_dir and diag_dir.is_dir():
            candidates = list(diag_dir.glob("Runner_*.log")) + list(diag_dir.glob("Worker_*.log"))
            if candidates:
                newest = max(candidates, key=lambda path: path.stat().st_mtime)
        return {
            "runner": runner,
            "service_name": service_name,
            "service_state": service_state,
            "github_state": github_state,
            "pid": pid,
            "start_mode": start_mode,
            "account": account,
            "diag": newest.name if newest else "—",
            "diag_age": fmt_duration(now - newest.stat().st_mtime) if newest else "—",
            "path": str(root) if root else fallback_path or "—",
            "diag_path": str(diag_dir) if diag_dir and diag_dir.is_dir() else "",
        }

    def _render_local_health_rows(self) -> None:
        if not hasattr(self, "local_tree"):
            return
        columns = tuple(self.local_tree["columns"])
        rows = self._sort_rows("local", self._filtered(self.local_health_rows))
        def tag(row: dict[str, Any]) -> str:
            if row.get("service_state") != "RUNNING":
                return "OFFLINE"
            if row.get("github_state") == "OFFLINE":
                return "QUEUED"
            return "IDLE"
        self._replace_tree_rows("local", self.local_tree, rows, columns, tag)
        self._local_selection_changed()

    def _selected_local_row(self) -> dict[str, Any] | None:
        selection = self.local_tree.selection()
        if not selection:
            return None
        return self.tree_row_maps["local"].get(selection[0])

    def _local_selection_changed(self, _event: tk.Event[Any] | None = None) -> None:
        row = self._selected_local_row()
        enabled = bool(row and row.get("diag_path"))
        self.diag_button.configure(state=tk.NORMAL if enabled else tk.DISABLED)
        if row:
            self.detail_var.set(
                f"{row.get('runner', '—')}  •  service {row.get('service_state', '—')}  •  "
                f"GitHub {row.get('github_state', '—')}  •  PID {row.get('pid', '—')}  •  "
                f"latest diagnostic {row.get('diag', '—')} ({row.get('diag_age', '—')} ago)"
            )
        self._update_restart_button()

    @staticmethod
    def _powershell_quote(value: str) -> str:
        return "'" + value.replace("'", "''") + "'"

    def _find_local_runner_service(self, runner_name: str) -> dict[str, Any] | None:
        needle = runner_name.casefold()
        for row in self.local_health_rows:
            if str(row.get("runner", "")).casefold() == needle and row.get("service_name"):
                return {"Name": row.get("service_name"), "DisplayName": row.get("runner"), "Status": row.get("service_state")}
        if sys.platform != "win32":
            return None
        powershell = shutil.which("pwsh.exe") or shutil.which("powershell.exe")
        if not powershell:
            raise RuntimeError("PowerShell is unavailable")
        command = "Get-Service -Name 'actions.runner.*' -ErrorAction SilentlyContinue | Select-Object Name,DisplayName,Status | ConvertTo-Json -Compress"
        proc = subprocess.run([powershell, "-NoProfile", "-NonInteractive", "-Command", command], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace", timeout=15, creationflags=_creation_flags(), check=False)
        if proc.returncode != 0:
            raise RuntimeError((proc.stderr or proc.stdout).strip() or "Unable to query Windows runner services")
        raw = proc.stdout.strip()
        if not raw:
            return None
        data = json.loads(raw)
        services = data if isinstance(data, list) else [data]
        matches = [svc for svc in services if needle in str(svc.get("Name", "")).casefold() or needle in str(svc.get("DisplayName", "")).casefold()]
        if not matches:
            return None
        matches.sort(key=lambda svc: (not str(svc.get("Name", "")).casefold().endswith("." + needle), len(str(svc.get("Name", "")))))
        return matches[0]

    def _restart_selected_runner(self) -> None:
        selected_tab = self.notebook.index(self.notebook.select())
        runner_name = ""
        service_name = ""
        active = False

        if selected_tab == 0:
            row = self._selected_runner_row()
            if not row:
                return
            runner_name = str(row.get("name") or "")
            local = next((item for item in self.local_health_rows if str(item.get("runner", "")).casefold() == runner_name.casefold()), None)
            if not local:
                return
            service_name = str(local.get("service_name") or "")
            active = str(row.get("state", "")).upper() == "RUNNING"
        elif selected_tab == 3:
            row = self._selected_local_row()
            if not row:
                return
            runner_name = str(row.get("runner") or "")
            service_name = str(row.get("service_name") or "")
            active = str(row.get("github_state", "")).upper() == "RUNNING"
        else:
            return

        if not runner_name:
            return
        if active and not messagebox.askyesno(
            "Restart active runner?",
            f"{runner_name} is currently running a job.\n\n"
            "Restarting its local runner service will interrupt that job. Restart it anyway?",
            icon="warning",
        ):
            return

        self.restart_button.configure(state=tk.DISABLED)
        self.status_var.set(f"Locating local service for {runner_name}…")
        threading.Thread(
            target=self._restart_runner_worker,
            args=(runner_name, service_name),
            daemon=True,
        ).start()

    def _restart_runner_worker(self, runner_name: str, service_name: str = "") -> None:
        try:
            if not service_name:
                service = self._find_local_runner_service(runner_name)
                if not service:
                    raise RuntimeError(f"No local runner service matches {runner_name}. The runner may be installed on another computer.")
                service_name = str(service.get("Name") or "")
            if not service_name:
                raise RuntimeError(f"No service name was found for {runner_name}")

            if sys.platform == "win32":
                quoted = self._powershell_quote(service_name)
                elevated_script = ("$ErrorActionPreference='Stop'; " f"$name={quoted}; " "$svc=Get-Service -Name $name -ErrorAction Stop; " "if ($svc.Status -eq 'Running') { Restart-Service -Name $name -Force -ErrorAction Stop } else { Start-Service -Name $name -ErrorAction Stop }; " "$svc=Get-Service -Name $name; $svc.WaitForStatus('Running',[TimeSpan]::FromSeconds(20)); " "if ($svc.Status -ne 'Running') { throw 'Runner service did not reach Running state' }")
                encoded = base64.b64encode(elevated_script.encode("utf-16le")).decode("ascii")
                powershell = shutil.which("powershell.exe") or "powershell.exe"
                launcher = ("$p=Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -PassThru " f"-ArgumentList @('-NoProfile','-EncodedCommand','{encoded}'); exit $p.ExitCode")
                proc = subprocess.run([powershell, "-NoProfile", "-Command", launcher], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace", timeout=60, creationflags=_creation_flags(), check=False)
            elif sys.platform.startswith("linux"):
                systemctl = shutil.which("systemctl")
                if not systemctl:
                    raise RuntimeError("systemctl is unavailable")
                if hasattr(os, "geteuid") and os.geteuid() == 0:
                    prefix: list[str] = []
                elif shutil.which("pkexec"):
                    prefix = ["pkexec"]
                elif shutil.which("sudo"):
                    prefix = ["sudo"]
                else:
                    raise RuntimeError("Neither pkexec nor sudo is available for service restart")
                proc = subprocess.run(prefix + [systemctl, "restart", service_name], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, encoding="utf-8", errors="replace", timeout=60, check=False)
                if proc.returncode == 0:
                    check = subprocess.run([systemctl, "is-active", service_name], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=10, check=False)
                    if check.stdout.strip() != "active":
                        raise RuntimeError(f"{service_name} did not return to active state")
            else:
                raise RuntimeError("Local runner restart is supported only on Windows and Linux")
            if proc.returncode != 0:
                detail = (proc.stderr or proc.stdout).strip()
                raise RuntimeError(detail or "Elevated restart was cancelled or failed")
            self._post(self._restart_runner_done, runner_name, service_name)
        except Exception as exc:
            self._post(self._restart_runner_failed, runner_name, str(exc))

    def _restart_runner_done(self, runner_name: str, service_name: str) -> None:
        self.status_var.set(
            f"Restarted {runner_name} ({service_name}). Waiting for GitHub to reconnect…"
        )
        self._update_restart_button()
        self.after(1200, self.request_local_health_refresh)
        self.after(1500, self.request_runner_refresh)
        self.after(4000, self.request_runner_refresh)
        self.after(5000, lambda: self.request_activity_refresh(force=True))

    def _restart_runner_failed(self, runner_name: str, error: str) -> None:
        self.status_var.set(f"Could not restart {runner_name}: {error}")
        self._update_restart_button()
        messagebox.showerror(
            "Runner restart failed",
            f"Could not restart {runner_name}.\n\n{error}",
        )

    def _open_selected_diag(self) -> None:
        row = self._selected_local_row()
        path = str(row.get("diag_path") or "") if row else ""
        if not path:
            return
        try:
            if sys.platform == "win32":
                os.startfile(path)  # type: ignore[attr-defined]
            elif sys.platform.startswith("linux"):
                opener = shutil.which("xdg-open")
                if not opener:
                    raise OSError("xdg-open is unavailable")
                subprocess.Popen([opener, path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except OSError as exc:
            messagebox.showerror("Open diagnostics", str(exc))

    def _export_selected_tab(self) -> None:
        selected = self.notebook.index(self.notebook.select())
        if selected == 0:
            tree, rows, label = self.runner_tree, self._filtered(self.runner_rows), "runners"
        elif selected == 1:
            tree, rows, label = self.activity_tree, self._filtered(self.activity_rows), "active-jobs"
        elif selected == 2:
            tree, rows, label = self.history_tree, self._filtered(list(self.history)), "history"
        else:
            tree, rows, label = self.local_tree, self._filtered(self.local_health_rows), "local-health"
        columns = list(tree["columns"])
        filename = filedialog.asksaveasfilename(
            title="Export GitHub runner data",
            defaultextension=".csv",
            initialfile=f"github-runner-{label}-{time.strftime('%Y%m%d-%H%M%S')}.csv",
            filetypes=[("CSV files", "*.csv"), ("All files", "*.*")],
        )
        if not filename:
            return
        with open(filename, "w", newline="", encoding="utf-8-sig") as handle:
            writer = csv.writer(handle)
            writer.writerow(columns)
            for row in rows:
                writer.writerow([row.get(column, "") for column in columns])
        self.status_var.set(f"Exported {len(rows)} row(s) to {filename}")

    def _load_persistent_history(self) -> None:
        try:
            if not STATE_FILE.is_file():
                return
            data = json.loads(STATE_FILE.read_text(encoding="utf-8"))
            rows = data.get("history", []) if isinstance(data, dict) else []
            for row in reversed(rows[-MAX_HISTORY:]):
                if isinstance(row, dict):
                    self.history.appendleft(dict(row))
        except (OSError, ValueError, TypeError):
            return

    def _save_persistent_history_locked(self) -> None:
        try:
            STATE_FILE.parent.mkdir(parents=True, exist_ok=True)
            temp = STATE_FILE.with_suffix(".tmp")
            temp.write_text(
                json.dumps({"history": list(self.history)}, ensure_ascii=False, indent=2),
                encoding="utf-8",
            )
            temp.replace(STATE_FILE)
        except OSError:
            pass

    def _show_local_health_error(self, message: str) -> None:
        self.status_var.set(f"Local service health warning: {message}")

    # ---------- history / links / errors ----------

    def _append_history_locked(
        self,
        runner: str,
        event: str,
        detail: str,
        tag: str,
    ) -> None:
        event_time = time.time()
        self.history.appendleft(
            {
                "time": event_time,
                "time_text": fmt_history_time(event_time),
                "runner": runner,
                "event": event,
                "detail": detail or "—",
                "tag": tag,
            }
        )
        self._save_persistent_history_locked()
        self._post(self._render_history_rows)

    def _selected_activity_row(self) -> dict[str, Any] | None:
        selection = self.activity_tree.selection()
        if not selection:
            return None
        return self.tree_row_maps["activity"].get(selection[0])

    def _activity_selection_changed(self, _event: tk.Event[Any] | None = None) -> None:
        row = self._selected_activity_row()
        self.open_button.configure(state=tk.NORMAL if row and row.get("url") else tk.DISABLED)
        if row and hasattr(self, "detail_var"):
            self.detail_var.set(
                f"{row.get('repo', '—')}  •  {row.get('workflow', '—')} › {row.get('job', '—')}  •  "
                f"{row.get('status', '—')}  •  {row.get('runner', '—')}  •  "
                f"step {row.get('step', '—')}  •  branch {row.get('branch', '—')}"
            )

    def _activity_double_click(self, _event: tk.Event[Any]) -> None:
        self._open_selected_job()

    def _open_selected_job(self) -> None:
        row = self._selected_activity_row()
        if row and row.get("url"):
            webbrowser.open(str(row["url"]))

    @staticmethod
    def _job_display(job: dict[str, Any]) -> str:
        workflow = job.get("workflow") or ""
        name = job.get("job") or job.get("name") or ""
        if workflow and name and workflow != name:
            return f"{workflow} › {name}"
        return name or workflow or "—"

    def _job_detail(self, job: dict[str, Any]) -> str:
        repo = job.get("repo") or "—"
        display = self._job_display(job)
        runner = job.get("runner") or job.get("runner_name") or ""
        if runner and runner != "—":
            return f"{repo} • {display} • {runner}"
        return f"{repo} • {display}"

    def _show_runner_error(self, message: str) -> None:
        self.status_var.set(f"Runner API error: {message}")

    def _show_activity_error(self, message: str) -> None:
        self.status_var.set(f"Activity scan warning: {message}")


def run_self_test() -> int:
    assert fmt_duration(None) == "—"
    assert fmt_duration(0) == "0s"
    assert fmt_duration(65) == "1m 05s"
    assert fmt_duration(3661) == "1h 01m"
    assert parse_github_time("2026-09-02T01:02:03Z") is not None
    assert parse_github_time("invalid") is None
    assert RunnerMonitor._powershell_quote("a'b") == "'a''b'"
    assert extract_service_executable('"C:\\actions-runner\\bin\\RunnerService.exe"') == Path(r"C:\actions-runner\bin\RunnerService.exe")
    assert current_job_step({"steps": [{"name": "Build", "status": "in_progress"}]}) == "Build"
    assert current_job_step({"steps": [{"name": "One", "status": "completed"}, {"name": "Two", "status": "queued"}]}) == "1/2 steps"
    assert short_labels([{"name": "self-hosted"}, {"name": "Windows"}]) == "Windows"
    assert RunnerMonitor._classify_job_environment(
        {"runner_name": "Runner-1", "labels": ["self-hosted", "Windows"]},
        {"Runner-1"},
    ) == "LOCAL"
    assert RunnerMonitor._classify_job_environment(
        {"runner_group_name": "GitHub Actions", "labels": ["windows-latest"]},
        set(),
    ) == "GITHUB"
    assert DEFAULT_CONFIG["organisation"] == ""
    print(f"RunnerScope {VERSION} self-test passed")
    return 0


def main() -> int:
    if "--self-test" in sys.argv:
        return run_self_test()
    enable_windows_dpi_awareness()
    register_optional_brand_fonts()
    cfg = load_config()
    if cfg is None or not str(cfg.get("organisation") or "").strip():
        setup_root = tk.Tk()
        setup_root.withdraw()
        configure_shared_theme(setup_root)
        setup = ConfigDialog(setup_root, cfg)
        setup_root.wait_window(setup)
        cfg = setup.result
        setup_root.destroy()
        if not cfg:
            return 1
        save_config(cfg)
    apply_config(cfg)
    try:
        app = RunnerMonitor(cfg)
        app.mainloop()
        return 0
    except tk.TclError as exc:
        message = f"Unable to start RunnerScope: {exc}"
        try:
            messagebox.showerror(APP_NAME, message)
        except tk.TclError:
            pass
        print(message, file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
