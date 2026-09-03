#!/usr/bin/env python3
# Copyright (C) 2026 Shannon Smith
# SPDX-License-Identifier: GPL-3.0-or-later
"""RunnerScope - cross-platform monitor for GitHub Actions self-hosted runners."""
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
from typing import Any, Callable

try:
    import tkinter as tk
    from tkinter import filedialog, font as tkfont, messagebox, ttk
except ImportError as exc:
    raise SystemExit(f"Tkinter is unavailable: {exc}") from exc

APP = "RunnerScope"
VERSION = "1.0.2"
DEFAULTS: dict[str, Any] = {
    "organisation": "",
    "expected_runners": 0,
    "runner_poll_seconds": 2.0,
    "activity_scan_seconds": 45.0,
    "repository_scan_limit": 25,
    "history_entries": 300,
    "local_health_seconds": 10.0,
}
BG, PANEL, CARD, SURFACE = "#050608", "#101318", "#171b20", "#0d1014"
BORDER, TEXT, TITLE, MUTED, SUBTLE = "#353a40", "#e8ecef", "#eef1f3", "#aeb6bd", "#899198"
BUTTON_BG, BUTTON_FG = "#d7dde2", "#111418"
SELECT_BG, SELECT_FG = "#2b3137", "#eef1f3"
GREEN, BLUE, RED, AMBER, PURPLE = "#63ab7c", "#9aa7b2", "#c96b6b", "#d19e47", "#7fa7c9"
MB_BODY_FONT = "MB Corpo S Title WEB"
MB_BRAND_FONT = "MB Corpo A Title Cond WEB"
MB_FONT_FILES = ("mb_corpo_a_cond_regular.ttf", "mb_corpo_s_bold.ttf", "mb_corpo_s_regular.ttf")
REPO_CACHE_SECONDS = 300.0


def config_dir() -> Path:
    if sys.platform == "win32":
        root = os.environ.get("APPDATA") or os.environ.get("LOCALAPPDATA")
        return Path(root) / APP if root else Path.home() / APP
    root = os.environ.get("XDG_CONFIG_HOME")
    return Path(root) / "runnerscope" if root else Path.home() / ".config" / "runnerscope"


def state_dir() -> Path:
    if sys.platform == "win32":
        root = os.environ.get("LOCALAPPDATA") or os.environ.get("APPDATA")
        return Path(root) / APP if root else Path.home() / APP
    root = os.environ.get("XDG_STATE_HOME")
    return Path(root) / "runnerscope" if root else Path.home() / ".local" / "state" / "runnerscope"


CONFIG_FILE = config_dir() / "config.json"
STATE_FILE = state_dir() / "state.json"


def load_config() -> dict[str, Any] | None:
    if not CONFIG_FILE.is_file():
        return None
    try:
        data = json.loads(CONFIG_FILE.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    if not isinstance(data, dict):
        return None
    cfg = dict(DEFAULTS)
    cfg.update(data)
    return cfg


def save_config(cfg: dict[str, Any]) -> None:
    CONFIG_FILE.parent.mkdir(parents=True, exist_ok=True)
    clean = {key: cfg.get(key, default) for key, default in DEFAULTS.items()}
    tmp = CONFIG_FILE.with_suffix(".tmp")
    tmp.write_text(json.dumps(clean, indent=2) + "\n", encoding="utf-8")
    tmp.replace(CONFIG_FILE)


def parse_time(value: Any) -> float | None:
    if not isinstance(value, str) or not value:
        return None
    try:
        return dt.datetime.fromisoformat(value.replace("Z", "+00:00")).timestamp()
    except ValueError:
        return None


def duration(seconds: float | None) -> str:
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


def current_step(job: dict[str, Any]) -> str:
    steps = job.get("steps") or []
    for step in steps:
        if str(step.get("status") or "").lower() == "in_progress":
            return str(step.get("name") or "Running")
    if steps:
        done = sum(str(x.get("status") or "").lower() == "completed" for x in steps)
        return f"{done}/{len(steps)} steps"
    return "—"


def find_gh() -> str | None:
    found = shutil.which("gh.exe") or shutil.which("gh")
    if found:
        return found
    if sys.platform == "win32":
        for root in (os.environ.get("ProgramFiles"), os.environ.get("LOCALAPPDATA")):
            if root:
                candidate = Path(root) / ("GitHub CLI/gh.exe" if "Program" in root else "Programs/GitHub CLI/gh.exe")
                if candidate.is_file():
                    return str(candidate)
    return None


def run_command(args: list[str], timeout: int = 25) -> subprocess.CompletedProcess[str]:
    flags = getattr(subprocess, "CREATE_NO_WINDOW", 0) if sys.platform == "win32" else 0
    return subprocess.run(
        args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
        encoding="utf-8", errors="replace", timeout=timeout, check=False,
        creationflags=flags,
    )


def open_path(path: str) -> None:
    if sys.platform == "win32":
        os.startfile(path)  # type: ignore[attr-defined]
    elif sys.platform.startswith("linux"):
        opener = shutil.which("xdg-open")
        if not opener:
            raise OSError("xdg-open is unavailable")
        subprocess.Popen([opener, path], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def enable_windows_dpi_awareness() -> None:
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


def register_private_mercedes_fonts() -> None:
    # Use local MB Corpo fonts when available without distributing them.
    if sys.platform != "win32":
        return
    try:
        import ctypes
        add_font = ctypes.windll.gdi32.AddFontResourceExW
        roots = [
            Path(__file__).resolve().parent,
            Path(__file__).resolve().parent / "fonts",
            Path(__file__).resolve().parent / "assets",
            Path(__file__).resolve().parent / "assets" / "fonts",
        ]
        env_dir = os.environ.get("MBLINK_FONT_DIR")
        if env_dir:
            roots.insert(0, Path(env_dir))
        for root in roots:
            for filename in MB_FONT_FILES:
                candidate = root / filename
                if candidate.is_file():
                    add_font(str(candidate), 0x10, 0)
    except (AttributeError, OSError):
        pass


def apply_windows_dark_titlebar(window: tk.Misc) -> None:
    if sys.platform != "win32":
        return
    try:
        import ctypes
        window.update_idletasks()
        hwnd = ctypes.windll.user32.GetParent(window.winfo_id()) or window.winfo_id()
        value = ctypes.c_int(1)
        for attribute in (20, 19):
            if ctypes.windll.dwmapi.DwmSetWindowAttribute(hwnd, attribute, ctypes.byref(value), ctypes.sizeof(value)) == 0:
                break
    except (AttributeError, OSError, tk.TclError):
        pass


def configure_theme(window: tk.Misc) -> dict[str, tuple]:
    families = {str(name) for name in tkfont.families(window)}
    fallback = "Segoe UI" if sys.platform == "win32" else "DejaVu Sans"
    body_family = MB_BODY_FONT if MB_BODY_FONT in families else fallback
    brand_family = MB_BRAND_FONT if MB_BRAND_FONT in families else body_family
    fonts = {
        "body": (body_family, 10), "body_bold": (body_family, 10, "bold"),
        "small": (body_family, 9), "small_bold": (body_family, 9, "bold"),
        "title": (brand_family, 22), "dialog_title": (brand_family, 18),
        "counter": (body_family, 11, "bold"),
    }
    try:
        window.configure(background=BG)
    except tk.TclError:
        pass
    window.option_add("*Font", fonts["body"])
    style = ttk.Style(window)
    if "clam" in style.theme_names():
        style.theme_use("clam")
    style.configure(".", background=BG, foreground=TEXT, font=fonts["body"], bordercolor=BORDER, darkcolor=BORDER, lightcolor=BORDER, troughcolor=SURFACE, focuscolor=BORDER)
    style.configure("TFrame", background=BG)
    style.configure("TLabel", background=BG, foreground=TEXT)
    style.configure("Title.TLabel", font=fonts["title"], foreground=TITLE)
    style.configure("DialogTitle.TLabel", font=fonts["dialog_title"], foreground=TITLE)
    style.configure("Meta.TLabel", foreground=MUTED, font=fonts["small"])
    style.configure("Help.TLabel", foreground=SUBTLE, font=fonts["small"])
    style.configure("Counter.TLabel", font=fonts["counter"], foreground=TITLE)
    style.configure("Green.Counter.TLabel", font=fonts["counter"], foreground=GREEN)
    style.configure("Blue.Counter.TLabel", font=fonts["counter"], foreground=BLUE)
    style.configure("Red.Counter.TLabel", font=fonts["counter"], foreground=RED)
    style.configure("Purple.Counter.TLabel", font=fonts["counter"], foreground=PURPLE)
    style.configure("Amber.Counter.TLabel", font=fonts["counter"], foreground=AMBER)
    style.configure("CounterCard.TFrame", background=CARD, bordercolor=BORDER, relief="solid", borderwidth=1)
    style.configure("DetailCard.TFrame", background=PANEL, bordercolor=BORDER, relief="solid", borderwidth=1)
    style.configure("Detail.TLabel", background=PANEL, foreground=MUTED, font=fonts["small"])
    style.configure("TButton", background=BUTTON_BG, foreground=BUTTON_FG, bordercolor=BORDER, font=fonts["body_bold"], padding=(12, 7), relief="flat")
    style.map("TButton", background=[("disabled", PANEL), ("pressed", "#b9c0c6"), ("active", "#eef1f3")], foreground=[("disabled", SUBTLE), ("pressed", BUTTON_FG), ("active", BUTTON_FG)])
    style.configure("TEntry", fieldbackground=SURFACE, foreground=TEXT, insertcolor=TEXT, bordercolor=BORDER, lightcolor=BORDER, darkcolor=BORDER, padding=(8, 6))
    style.map("TEntry", fieldbackground=[("focus", PANEL), ("disabled", PANEL)], foreground=[("disabled", SUBTLE)], bordercolor=[("focus", MUTED)])
    style.configure("TNotebook", background=BG, bordercolor=BORDER, tabmargins=(0, 5, 0, 0))
    style.configure("TNotebook.Tab", background=PANEL, foreground=MUTED, bordercolor=BORDER, font=fonts["body_bold"], padding=(18, 9))
    style.map("TNotebook.Tab", background=[("selected", CARD), ("active", "#14181d")], foreground=[("selected", TITLE), ("active", TEXT)])
    style.configure("Treeview", background=SURFACE, fieldbackground=SURFACE, foreground=TEXT, bordercolor=BORDER, rowheight=31, font=fonts["body"])
    style.map("Treeview", background=[("selected", SELECT_BG)], foreground=[("selected", SELECT_FG)])
    style.configure("Treeview.Heading", background=PANEL, foreground=TITLE, bordercolor=BORDER, font=fonts["small_bold"], padding=(8, 8), relief="flat")
    style.map("Treeview.Heading", background=[("active", CARD)], foreground=[("active", TITLE)])
    for orient in ("Vertical", "Horizontal"):
        style.configure(f"{orient}.TScrollbar", background=PANEL, troughcolor=SURFACE, bordercolor=BORDER, arrowcolor=MUTED)
    return fonts


class ConfigDialog(tk.Toplevel):
    FIELDS = (
        ("organisation", "GitHub organisation", "The organisation that owns the self-hosted runners."),
        ("expected_runners", "Expected self-hosted runners", "Used only for the missing-runner warning. Set 0 to disable the warning."),
        ("runner_poll_seconds", "Runner status refresh", "How often RunnerScope checks online / busy state, in seconds."),
        ("activity_scan_seconds", "Active job refresh", "How often active workflow jobs are refreshed, in seconds."),
        ("repository_scan_limit", "Repositories to scan", "Most recently active repositories checked for running or queued jobs."),
        ("local_health_seconds", "Local service refresh", "How often this computer's runner services and diagnostics are checked."),
    )

    def __init__(self, parent: tk.Misc, cfg: dict[str, Any] | None = None) -> None:
        super().__init__(parent)
        self.fonts = configure_theme(self)
        source = dict(DEFAULTS)
        if cfg:
            source.update(cfg)
        self.result: dict[str, Any] | None = None
        self.vars = {key: tk.StringVar(value=str(source[key])) for key, _, _ in self.FIELDS}
        self.title(f"{APP} configuration")
        self.geometry("640x620")
        self.resizable(False, False)
        if parent.winfo_viewable():
            self.transient(parent)
        self.grab_set()
        outer = ttk.Frame(self, padding=24)
        outer.pack(fill=tk.BOTH, expand=True)
        ttk.Label(outer, text=APP, style="DialogTitle.TLabel").grid(row=0, column=0, columnspan=2, sticky="w")
        ttk.Label(outer, text="Connect RunnerScope to your GitHub organisation. Authentication stays with GitHub CLI; RunnerScope never stores a token.", style="Meta.TLabel", wraplength=570, justify=tk.LEFT).grid(row=1, column=0, columnspan=2, sticky="w", pady=(4, 18))
        row = 2
        first_entry: ttk.Entry | None = None
        for key, label, help_text in self.FIELDS:
            ttk.Label(outer, text=label).grid(row=row, column=0, sticky="nw", padx=(0, 18), pady=(4, 0))
            entry = ttk.Entry(outer, textvariable=self.vars[key], width=34)
            entry.grid(row=row, column=1, sticky="ew", pady=(2, 0))
            if first_entry is None:
                first_entry = entry
            ttk.Label(outer, text=help_text, style="Help.TLabel", wraplength=330, justify=tk.LEFT).grid(row=row + 1, column=1, sticky="w", pady=(2, 10))
            row += 2
        buttons = ttk.Frame(outer)
        buttons.grid(row=row, column=0, columnspan=2, sticky="e", pady=(14, 0))
        ttk.Button(buttons, text="Cancel", command=self.destroy).pack(side=tk.RIGHT)
        ttk.Button(buttons, text="Save", command=self.save).pack(side=tk.RIGHT, padx=(0, 10))
        outer.columnconfigure(1, weight=1)
        self.protocol("WM_DELETE_WINDOW", self.destroy)
        self.update_idletasks()
        self.deiconify()
        apply_windows_dark_titlebar(self)
        self.lift()
        if first_entry is not None:
            first_entry.focus_set()

    def save(self) -> None:
        org = self.vars["organisation"].get().strip()
        if not re.fullmatch(r"[A-Za-z0-9_.-]+", org):
            messagebox.showerror(APP, "Enter the GitHub organisation name only.", parent=self)
            return
        try:
            cfg = dict(DEFAULTS)
            cfg.update(organisation=org, expected_runners=max(0, int(self.vars["expected_runners"].get())), runner_poll_seconds=max(1.0, float(self.vars["runner_poll_seconds"].get())), activity_scan_seconds=max(10.0, float(self.vars["activity_scan_seconds"].get())), repository_scan_limit=max(1, int(self.vars["repository_scan_limit"].get())), local_health_seconds=max(5.0, float(self.vars["local_health_seconds"].get())))
        except ValueError:
            messagebox.showerror(APP, "One of the numeric settings is invalid.", parent=self)
            return
        self.result = cfg
        self.destroy()


def first_run_config() -> dict[str, Any] | None:
    root = tk.Tk()
    root.withdraw()
    configure_theme(root)
    dialog = ConfigDialog(root, load_config())
    root.wait_window(dialog)
    result = dialog.result
    root.destroy()
    return result


class RunnerScope(tk.Tk):
    ACTIVE = {"queued", "in_progress", "waiting", "pending", "requested"}

    def __init__(self, cfg: dict[str, Any]) -> None:
        super().__init__()
        self.cfg = cfg
        self.org = str(os.environ.get("GITHUB_RUNNER_ORG") or cfg["organisation"]).strip()
        self.poll = max(1.0, float(os.environ.get("GITHUB_RUNNER_REFRESH", cfg["runner_poll_seconds"])))
        self.activity_poll = max(10.0, float(os.environ.get("GITHUB_RUNNER_ACTIVITY_REFRESH", cfg["activity_scan_seconds"])))
        self.repo_limit = max(1, int(os.environ.get("GITHUB_RUNNER_REPO_LIMIT", cfg["repository_scan_limit"])))
        self.expected = max(0, int(os.environ.get("GITHUB_RUNNER_EXPECTED", cfg["expected_runners"])))
        self.local_poll = max(5.0, float(os.environ.get("GITHUB_RUNNER_LOCAL_HEALTH_REFRESH", cfg["local_health_seconds"])))
        self.max_history = max(50, int(os.environ.get("GITHUB_RUNNER_HISTORY", cfg["history_entries"])))
        self.gh = find_gh()
        if not self.gh:
            raise RuntimeError("GitHub CLI (gh) is not installed or not on PATH")

        self.stop = threading.Event()
        self.q: queue.Queue[tuple[Callable[..., Any], tuple[Any, ...]]] = queue.Queue()
        self.lock = threading.RLock()
        self.runner_busy = False
        self.activity_busy = False
        self.local_busy = False
        self.runners: list[dict[str, Any]] = []
        self.jobs: list[dict[str, Any]] = []
        self.local_rows: list[dict[str, Any]] = []
        self.job_by_runner: dict[str, dict[str, Any]] = {}
        self.previous_states: dict[str, str] = {}
        self.history: deque[dict[str, Any]] = deque(maxlen=self.max_history)
        self.load_history()
        self.repo_cache: list[str] = []
        self.repo_cache_at = 0.0
        self.last_activity_scan = 0.0

        self.title(f"{APP} {VERSION}")
        self.geometry("1460x820")
        self.minsize(1080, 650)
        self.protocol("WM_DELETE_WINDOW", self.close)
        self.configure_style()
        apply_windows_dark_titlebar(self)
        self.build_ui()
        self.after(100, self.drain)
        self.after(1000, self.tick)
        self.request_runners()
        self.request_activity()
        self.request_local()
        self.after(int(self.poll * 1000), self.runner_timer)
        self.after(int(self.activity_poll * 1000), self.activity_timer)
        if sys.platform == "win32" or sys.platform.startswith("linux"):
            self.after(int(self.local_poll * 1000), self.local_timer)

    def configure_style(self) -> None:
        self.fonts = configure_theme(self)

    def build_ui(self) -> None:
        outer = ttk.Frame(self, padding=16)
        outer.pack(fill=tk.BOTH, expand=True)
        heading = ttk.Frame(outer)
        heading.pack(fill=tk.X)
        title_box = ttk.Frame(heading)
        title_box.pack(side=tk.LEFT)
        ttk.Label(title_box, text=APP, style="Title.TLabel").pack(anchor=tk.W)
        ttk.Label(title_box, text="GitHub Actions self-hosted runner monitor", style="Meta.TLabel").pack(anchor=tk.W, pady=(1, 0))
        self.updated = tk.StringVar(value="Starting…")
        ttk.Label(heading, textvariable=self.updated, style="Meta.TLabel").pack(side=tk.RIGHT, anchor=tk.S, pady=(0, 4))
        meta = ttk.Frame(outer)
        meta.pack(fill=tk.X, pady=(10, 10))
        ttk.Label(meta, text=f"Organisation: {self.org}").pack(side=tk.LEFT)
        ttk.Label(meta, text=f"Runner refresh {self.poll:g}s  •  Active jobs {self.activity_poll:g}s  •  Up to {self.repo_limit} repositories", style="Meta.TLabel").pack(side=tk.LEFT, padx=(22, 0))
        counters = ttk.Frame(outer)
        counters.pack(fill=tk.X, pady=(0, 12))
        self.counter_vars: dict[str, tk.StringVar] = {}
        specs = (("TOTAL", "Counter.TLabel"), ("RUNNING", "Green.Counter.TLabel"), ("IDLE", "Blue.Counter.TLabel"), ("OFFLINE", "Red.Counter.TLabel"), ("SELF-HOSTED ACTIVE", "Green.Counter.TLabel"), ("GITHUB ACTIVE", "Purple.Counter.TLabel"), ("QUEUED", "Amber.Counter.TLabel"))
        for name, label_style in specs:
            card = ttk.Frame(counters, style="CounterCard.TFrame", padding=(12, 8))
            card.pack(side=tk.LEFT, padx=(0, 8))
            var = tk.StringVar(value=f"{name}  0")
            self.counter_vars[name] = var
            label = ttk.Label(card, textvariable=var, style=label_style, cursor="hand2")
            label.pack()
            card.bind("<Button-1>", lambda _e, n=name: self.counter_filter(n))
            label.bind("<Button-1>", lambda _e, n=name: self.counter_filter(n))
        search = ttk.Frame(outer)
        search.pack(fill=tk.X, pady=(0, 8))
        ttk.Label(search, text="Search").pack(side=tk.LEFT)
        self.filter_var = tk.StringVar()
        entry = ttk.Entry(search, textvariable=self.filter_var, width=42)
        entry.pack(side=tk.LEFT, padx=(8, 8))
        self.filter_var.trace_add("write", lambda *_: self.render_all())
        ttk.Button(search, text="Clear", command=lambda: self.filter_var.set("")).pack(side=tk.LEFT)
        ttk.Label(search, text="Filters the selected view", style="Meta.TLabel").pack(side=tk.LEFT, padx=(14, 0))
        self.tabs = ttk.Notebook(outer)
        self.tabs.pack(fill=tk.BOTH, expand=True)
        self.tabs.bind("<<NotebookTabChanged>>", self.tab_changed)
        self.runner_tree = self.make_tab("Runners", (("name", "Runner", 210), ("os", "OS", 85), ("state", "State", 100), ("repo", "Repository", 190), ("job", "Current job", 430), ("runtime", "Runtime", 110)))
        self.job_tree = self.make_tab("Active jobs", (("where", "Where", 115), ("repo", "Repository", 180), ("workflow", "Workflow", 220), ("job", "Job", 270), ("step", "Current step", 270), ("status", "Status", 105), ("runner", "Runner", 190), ("runtime", "Runtime / queue", 115)))
        self.history_tree = self.make_tab("History", (("time", "Time", 135), ("runner", "Runner", 210), ("event", "Event", 120), ("detail", "Detail", 820)))
        self.local_tree = self.make_tab("Local runner health", (("runner", "Runner", 210), ("service_state", "Service", 110), ("github", "GitHub", 100), ("diag", "Latest diagnostic", 260), ("diag_age", "Diag age", 100), ("path", "Runner path", 550)))
        self.runner_tree.bind("<<TreeviewSelect>>", self.runner_selection_changed)
        self.job_tree.bind("<<TreeviewSelect>>", self.job_selection_changed)
        self.local_tree.bind("<<TreeviewSelect>>", self.local_selection_changed)
        self.job_tree.bind("<Double-1>", lambda _e: self.open_job())
        self.local_tree.bind("<Double-1>", lambda _e: self.open_diag())
        detail = ttk.Frame(outer, style="DetailCard.TFrame", padding=(12, 9))
        detail.pack(fill=tk.X, pady=(10, 0))
        self.detail_var = tk.StringVar(value="Select a runner, active job, or local service to see details.")
        ttk.Label(detail, textvariable=self.detail_var, style="Detail.TLabel").pack(fill=tk.X)
        actions = ttk.Frame(outer)
        actions.pack(fill=tk.X, pady=(10, 0))
        context_actions = ttk.Frame(actions)
        context_actions.pack(side=tk.LEFT)
        self.open_button = ttk.Button(context_actions, text="Open selected job", command=self.open_job, state=tk.DISABLED)
        self.open_button.pack(side=tk.LEFT)
        self.diag_button = ttk.Button(context_actions, text="Open diagnostics", command=self.open_diag, state=tk.DISABLED)
        self.diag_button.pack(side=tk.LEFT, padx=(8, 0))
        self.restart_button = ttk.Button(context_actions, text="Restart local runner", command=self.restart_selected, state=tk.DISABLED)
        self.restart_button.pack(side=tk.LEFT, padx=(8, 0))
        ttk.Button(actions, text="Refresh now", command=self.refresh_all).pack(side=tk.RIGHT)
        ttk.Button(actions, text="Settings", command=self.settings).pack(side=tk.RIGHT, padx=(0, 8))
        ttk.Button(actions, text="Export CSV", command=self.export_csv).pack(side=tk.RIGHT, padx=(0, 8))
        self.status = tk.StringVar(value="Starting…")
        ttk.Label(outer, textvariable=self.status, style="Meta.TLabel").pack(fill=tk.X, pady=(9, 0))

    def make_tab(self, title: str, columns: tuple[tuple[str, str, int], ...]) -> ttk.Treeview:
        frame = ttk.Frame(self.tabs)
        self.tabs.add(frame, text=title)
        tree = ttk.Treeview(frame, columns=[x[0] for x in columns], show="headings")
        tree._runnerscope_rows = {}
        tree.tag_configure("RUNNING", foreground=GREEN, font=self.fonts["body_bold"])
        tree.tag_configure("IDLE", foreground=BLUE)
        tree.tag_configure("OFFLINE", foreground=RED, font=self.fonts["body_bold"])
        tree.tag_configure("IN_PROGRESS", foreground=GREEN, font=self.fonts["body_bold"])
        tree.tag_configure("QUEUED", foreground=AMBER)
        tree.tag_configure("SELF-HOSTED", foreground=GREEN)
        tree.tag_configure("GITHUB", foreground=PURPLE)
        tree.tag_configure("UNKNOWN", foreground=SUBTLE)
        for key, label, width in columns:
            tree.heading(key, text=label)
            tree.column(key, width=width, minwidth=60, stretch=True, anchor=tk.W)
        vs = ttk.Scrollbar(frame, orient=tk.VERTICAL, command=tree.yview)
        hs = ttk.Scrollbar(frame, orient=tk.HORIZONTAL, command=tree.xview)
        tree.configure(yscrollcommand=vs.set, xscrollcommand=hs.set)
        tree.grid(row=0, column=0, sticky="nsew")
        vs.grid(row=0, column=1, sticky="ns")
        hs.grid(row=1, column=0, sticky="ew")
        frame.rowconfigure(0, weight=1)
        frame.columnconfigure(0, weight=1)
        return tree
    def gh_json(self, endpoint: str, timeout: int = 25) -> Any:
        proc = run_command([self.gh, "api", "-H", "Accept: application/vnd.github+json", endpoint], timeout)
        if proc.returncode:
            detail = (proc.stderr or proc.stdout).strip()
            if "403" in detail:
                detail += "  Organisation runner access may require: gh auth refresh -h github.com -s admin:org"
            raise RuntimeError(detail or "GitHub CLI request failed")
        return json.loads(proc.stdout)

    def post(self, fn: Callable[..., Any], *args: Any) -> None:
        if not self.stop.is_set():
            self.q.put((fn, args))

    def drain(self) -> None:
        try:
            while True:
                fn, args = self.q.get_nowait()
                fn(*args)
        except queue.Empty:
            pass
        if not self.stop.is_set():
            self.after(100, self.drain)

    def tick(self) -> None:
        now = time.time()
        for row in self.jobs:
            ts = row.get("started_ts") or row.get("created_ts")
            row["runtime"] = duration(now - ts) if ts else "—"
        self.update_runtime_cells()
        if not self.stop.is_set():
            self.after(1000, self.tick)

    def update_runtime_cells(self) -> None:
        rows = getattr(self.job_tree, "_runnerscope_rows", {})
        columns = list(self.job_tree["columns"])
        if "runtime" not in columns:
            return
        runtime_index = columns.index("runtime")
        for iid, row in list(rows.items()):
            if not self.job_tree.exists(iid):
                continue
            values = list(self.job_tree.item(iid, "values"))
            if runtime_index < len(values):
                values[runtime_index] = row.get("runtime", "—")
                self.job_tree.item(iid, values=values)
    def refresh_all(self) -> None:
        self.request_runners(); self.request_activity(); self.request_local()

    def runner_timer(self) -> None:
        if self.stop.is_set(): return
        self.request_runners()
        self.after(int(self.poll * 1000), self.runner_timer)

    def activity_timer(self) -> None:
        if self.stop.is_set(): return
        self.request_activity()
        self.after(int(self.activity_poll * 1000), self.activity_timer)

    def local_timer(self) -> None:
        if self.stop.is_set(): return
        self.request_local()
        self.after(int(self.local_poll * 1000), self.local_timer)

    def request_runners(self) -> None:
        with self.lock:
            if self.runner_busy or self.stop.is_set(): return
            self.runner_busy = True
        threading.Thread(target=self.runner_worker, daemon=True).start()

    def runner_worker(self) -> None:
        try:
            data = self.gh_json(f"/orgs/{self.org}/actions/runners?per_page=100")
            rows = []
            jobs = self.job_by_runner.copy()
            for item in data.get("runners", []):
                state = "OFFLINE" if item.get("status") != "online" else "RUNNING" if item.get("busy") else "IDLE"
                name = item.get("name") or "Unnamed runner"
                job = jobs.get(name, {})
                labels = [x.get("name", "") for x in item.get("labels", []) if x.get("name") != "self-hosted"]
                rows.append({"name": name, "os": item.get("os") or "—", "state": state, "repo": job.get("repo", "—"), "job": job.get("job", "—"), "runtime": job.get("runtime", "—"), "labels": ", ".join(labels)})
                old = self.previous_states.get(name)
                if old and old != state:
                    self.add_history(name, state, f"{old} → {state}")
                self.previous_states[name] = state
            rows.sort(key=lambda x: x["name"].lower())
            self.runners = rows
            self.post(self.render_all)
        except Exception as exc:
            self.post(self.status.set, f"Runner API error: {exc}")
        finally:
            with self.lock: self.runner_busy = False

    def request_activity(self, force: bool = False) -> None:
        with self.lock:
            if self.activity_busy or self.stop.is_set():
                return
            if not force and time.time() - self.last_activity_scan < min(5.0, self.activity_poll):
                return
            self.activity_busy = True
        threading.Thread(target=self.activity_worker, daemon=True).start()

    def _get_repositories(self) -> list[str]:
        now = time.time()
        with self.lock:
            if self.repo_cache and now - self.repo_cache_at < REPO_CACHE_SECONDS:
                return list(self.repo_cache)
        repos = self.gh_json(f"/orgs/{self.org}/repos?type=all&sort=pushed&direction=desc&per_page=100")
        names = [r["name"] for r in repos if r.get("name") and not r.get("archived") and not r.get("disabled")][:self.repo_limit]
        with self.lock:
            self.repo_cache = names
            self.repo_cache_at = now
        return list(names)

    def _fetch_repo_runs(self, repo: str, status: str | None = None) -> list[dict[str, Any]]:
        endpoint = f"/repos/{self.org}/{repo}/actions/runs?per_page=100&exclude_pull_requests=true"
        if status:
            endpoint += f"&status={status}"
        data = self.gh_json(endpoint)
        result: list[dict[str, Any]] = []
        for run in data.get("workflow_runs", []):
            if str(run.get("status") or "").lower() not in self.ACTIVE:
                continue
            result.append({"repo": repo, "run_id": run.get("id"), "workflow": run.get("name") or run.get("display_title") or "Workflow", "event": run.get("event") or "—", "branch": run.get("head_branch") or "—", "run_started_at": run.get("run_started_at") or run.get("created_at"), "run_url": run.get("html_url") or ""})
        return result

    def _fetch_run_jobs(self, run: dict[str, Any]) -> list[dict[str, Any]]:
        run_id = run.get("run_id")
        if not run_id:
            return []
        data = self.gh_json(f"/repos/{self.org}/{run['repo']}/actions/runs/{run_id}/jobs?per_page=100&filter=latest")
        result: list[dict[str, Any]] = []
        for job in data.get("jobs", []):
            row = dict(job)
            row.update({"repo": run["repo"], "workflow": run["workflow"], "event": run["event"], "branch": run["branch"], "run_started_at": run["run_started_at"], "run_url": run["run_url"]})
            result.append(row)
        return result

    def activity_worker(self) -> None:
        try:
            self.last_activity_scan = time.time()
            self.post(self.status.set, "Scanning active jobs…")
            repos = self._get_repositories()
            runs: list[dict[str, Any]] = []
            workers = min(6, max(1, len(repos)))
            with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as pool:
                futures = {pool.submit(self._fetch_repo_runs, repo): repo for repo in repos}
                for future in concurrent.futures.as_completed(futures):
                    if self.stop.is_set():
                        return
                    try:
                        runs.extend(future.result())
                    except Exception:
                        pass
            deduped: dict[tuple[str, str], dict[str, Any]] = {}
            for run in runs:
                deduped[(str(run.get("repo")), str(run.get("run_id")))] = run
            runs = list(deduped.values())
            jobs: list[dict[str, Any]] = []
            with concurrent.futures.ThreadPoolExecutor(max_workers=6) as pool:
                futures = {pool.submit(self._fetch_run_jobs, run): run for run in runs}
                for future in concurrent.futures.as_completed(futures):
                    if self.stop.is_set():
                        return
                    try:
                        jobs.extend(future.result())
                    except Exception:
                        pass
            active: list[dict[str, Any]] = []
            known_names = {r["name"] for r in self.runners}
            now = time.time()
            for job in jobs:
                status = str(job.get("status") or "").lower()
                if status not in self.ACTIVE:
                    continue
                runner = job.get("runner_name") or "—"
                labels = [str(x) for x in job.get("labels", [])]
                where = "SELF-HOSTED" if "self-hosted" in labels or runner in known_names else "GITHUB"
                started = parse_time(job.get("started_at"))
                created = parse_time(job.get("run_started_at"))
                active.append({"id": str(job.get("id")), "where": where, "repo": job.get("repo") or "—", "workflow": job.get("workflow") or "—", "job": job.get("name") or "—", "step": current_step(job), "status": status.upper(), "runner": runner, "started_ts": started, "created_ts": created, "runtime": duration(now - (started or created)) if (started or created) else "—", "branch": job.get("branch") or "—", "event": job.get("event") or "—", "url": job.get("html_url") or job.get("run_url") or ""})
            active.sort(key=lambda x: (x["where"], x["repo"], x["job"]))
            self.jobs = active
            self.job_by_runner = {x["runner"]: x for x in active if x["where"] == "SELF-HOSTED" and x["runner"] != "—"}
            self.post(self.render_all)
        except Exception as exc:
            self.post(self.status.set, f"Activity scan warning: {exc}")
        finally:
            with self.lock:
                self.activity_busy = False
    def request_local(self) -> None:
        if sys.platform != "win32" and not sys.platform.startswith("linux"): return
        with self.lock:
            if self.local_busy or self.stop.is_set(): return
            self.local_busy = True
        threading.Thread(target=self.local_worker, daemon=True).start()

    def local_worker(self) -> None:
        try:
            rows = self.windows_services() if sys.platform == "win32" else self.linux_services()
            self.local_rows = rows
            self.post(self.render_local)
        except Exception as exc:
            self.post(self.status.set, f"Local runner health warning: {exc}")
        finally:
            with self.lock: self.local_busy = False

    def health_row(self, runner: str, service: str, state: str, pid: Any, root: Path | None, fallback: str) -> dict[str, Any]:
        github = next((x["state"] for x in self.runners if x["name"].lower() == runner.lower()), "—")
        diag = root / "_diag" if root else None
        newest = None
        if diag and diag.is_dir():
            files = list(diag.glob("Runner_*.log")) + list(diag.glob("Worker_*.log"))
            if files: newest = max(files, key=lambda p: p.stat().st_mtime)
        return {"runner": runner, "service": service, "service_state": state, "github": github, "pid": pid or "—", "diag": newest.name if newest else "—", "diag_age": duration(time.time() - newest.stat().st_mtime) if newest else "—", "path": str(root) if root else fallback or "—", "diag_path": str(diag) if diag and diag.is_dir() else ""}

    def windows_services(self) -> list[dict[str, Any]]:
        ps = shutil.which("pwsh.exe") or shutil.which("powershell.exe")
        if not ps: raise RuntimeError("PowerShell is unavailable")
        cmd = "Get-CimInstance Win32_Service | Where-Object {$_.Name -like 'actions.runner.*'} | Select Name,DisplayName,State,ProcessId,PathName | ConvertTo-Json -Compress"
        proc = run_command([ps, "-NoProfile", "-NonInteractive", "-Command", cmd], 20)
        if proc.returncode: raise RuntimeError((proc.stderr or proc.stdout).strip())
        data = json.loads(proc.stdout) if proc.stdout.strip() else []
        services = data if isinstance(data, list) else [data]
        rows = []
        for svc in services:
            name = str(svc.get("Name") or "—"); display = str(svc.get("DisplayName") or name)
            runner = next((x["name"] for x in self.runners if x["name"].lower() in (name + display).lower()), display)
            path_name = str(svc.get("PathName") or "")
            match = re.match(r'^"([^"]+)"', path_name)
            exe = Path(match.group(1) if match else path_name.split()[0]) if path_name else None
            root = exe.parent.parent if exe and exe.parent.name.lower() == "bin" else exe.parent if exe else None
            rows.append(self.health_row(runner, name, str(svc.get("State") or "UNKNOWN").upper(), svc.get("ProcessId"), root, path_name))
        return sorted(rows, key=lambda x: x["runner"].lower())

    def linux_services(self) -> list[dict[str, Any]]:
        systemctl = shutil.which("systemctl")
        if not systemctl: raise RuntimeError("systemctl is unavailable")
        proc = run_command([systemctl, "list-unit-files", "actions.runner.*.service", "--no-legend", "--no-pager"], 20)
        if proc.returncode: raise RuntimeError((proc.stderr or proc.stdout).strip())
        rows = []
        for line in proc.stdout.splitlines():
            parts = line.split()
            if not parts: continue
            unit = parts[0]
            show = run_command([systemctl, "show", unit, "--no-pager", "--property=Description,ActiveState,SubState,MainPID,WorkingDirectory,ExecStart"], 15)
            info = dict(x.split("=", 1) for x in show.stdout.splitlines() if "=" in x)
            display = info.get("Description") or unit
            runner = next((x["name"] for x in self.runners if x["name"].lower() in (unit + display).lower()), display)
            root = Path(info["WorkingDirectory"]) if info.get("WorkingDirectory") not in (None, "", "/") else None
            if not root:
                match = re.search(r"path=([^ ;]+)", info.get("ExecStart", ""))
                if match: root = Path(match.group(1)).parent
            state = "RUNNING" if info.get("ActiveState") == "active" else str(info.get("ActiveState") or "unknown").upper()
            rows.append(self.health_row(runner, unit, state, int(info.get("MainPID") or 0), root, info.get("ExecStart", "")))
        return sorted(rows, key=lambda x: x["runner"].lower())

    def filtered(self, rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
        needle = self.filter_var.get().strip().lower()
        return rows if not needle else [row for row in rows if needle in " ".join(str(v) for v in row.values()).lower()]

    def fill(self, tree: ttk.Treeview, rows: list[dict[str, Any]]) -> None:
        selected_row = None
        selection = tree.selection()
        old_map = getattr(tree, "_runnerscope_rows", {})
        if selection:
            selected_row = old_map.get(selection[0])
        tree.delete(*tree.get_children())
        tree._runnerscope_rows = {}
        cols = list(tree["columns"])
        selected_iid = None
        for row in rows:
            tag = str(row.get("state") or row.get("status") or row.get("where") or "UNKNOWN")
            if tag not in {"RUNNING", "IDLE", "OFFLINE", "IN_PROGRESS", "QUEUED", "SELF-HOSTED", "GITHUB"}:
                tag = "UNKNOWN"
            iid = tree.insert("", tk.END, values=[row.get(c, "") for c in cols], tags=(tag,))
            tree._runnerscope_rows[iid] = row
            if selected_row is row:
                selected_iid = iid
        if selected_iid:
            tree.selection_set(selected_iid)

    def render_all(self) -> None:
        self.render_runners(); self.render_jobs(); self.render_history(); self.render_local(); self.render_counters()
        self.updated.set(time.strftime("Updated %H:%M:%S"))
        self.update_action_states()

    def render_runners(self) -> None: self.fill(self.runner_tree, self.filtered(self.runners))
    def render_jobs(self) -> None: self.fill(self.job_tree, self.filtered(self.jobs))
    def render_history(self) -> None: self.fill(self.history_tree, self.filtered(list(self.history)))
    def render_local(self) -> None: self.fill(self.local_tree, self.filtered(self.local_rows))

    def render_counters(self) -> None:
        states = [r["state"] for r in self.runners]
        values = {"TOTAL": len(states), "RUNNING": states.count("RUNNING"), "IDLE": states.count("IDLE"), "OFFLINE": states.count("OFFLINE"), "SELF-HOSTED ACTIVE": sum(j["where"] == "SELF-HOSTED" and j["status"] == "IN_PROGRESS" for j in self.jobs), "GITHUB ACTIVE": sum(j["where"] == "GITHUB" and j["status"] == "IN_PROGRESS" for j in self.jobs), "QUEUED": sum(j["status"] == "QUEUED" for j in self.jobs)}
        for key, value in values.items():
            self.counter_vars[key].set(f"{key}  {value}")
        if self.expected and len(states) != self.expected:
            self.status.set(f"GitHub reports {len(states)} runners; expected {self.expected}.")
        elif not self.activity_busy:
            self.status.set(f"Monitoring {len(states)} runner(s) for {self.org}.")

    def counter_filter(self, name: str) -> None:
        if name in {"TOTAL", "RUNNING", "IDLE", "OFFLINE"}:
            self.tabs.select(0)
            self.filter_var.set("" if name == "TOTAL" else name)
        else:
            self.tabs.select(1)
            self.filter_var.set({"SELF-HOSTED ACTIVE": "SELF-HOSTED", "GITHUB ACTIVE": "GITHUB", "QUEUED": "QUEUED"}.get(name, ""))

    def add_history(self, runner: str, event: str, detail: str) -> None:
        self.history.appendleft({"time": time.strftime("%d/%m %H:%M:%S"), "runner": runner, "event": event, "detail": detail})
        self.save_history()

    def load_history(self) -> None:
        try:
            data = json.loads(STATE_FILE.read_text(encoding="utf-8")) if STATE_FILE.is_file() else {}
            for row in reversed(data.get("history", [])[-self.max_history:]):
                self.history.appendleft(row)
        except (OSError, ValueError, TypeError):
            pass

    def save_history(self) -> None:
        try:
            STATE_FILE.parent.mkdir(parents=True, exist_ok=True)
            STATE_FILE.write_text(json.dumps({"history": list(self.history)}, indent=2), encoding="utf-8")
        except OSError:
            pass

    @staticmethod
    def selected_row(tree: ttk.Treeview) -> dict[str, Any] | None:
        selection = tree.selection()
        if not selection:
            return None
        return getattr(tree, "_runnerscope_rows", {}).get(selection[0])

    def selected_runner(self) -> dict[str, Any] | None: return self.selected_row(self.runner_tree)
    def selected_job(self) -> dict[str, Any] | None: return self.selected_row(self.job_tree)
    def selected_local(self) -> dict[str, Any] | None: return self.selected_row(self.local_tree)

    def runner_selection_changed(self, _event: tk.Event | None = None) -> None:
        row = self.selected_runner()
        if row:
            labels = row.get("labels") or "No extra labels"
            self.detail_var.set(f"{row.get('name', '—')}  •  {row.get('state', '—')}  •  {row.get('os', '—')}  •  {row.get('repo', '—')}  •  {row.get('job', '—')}  •  labels: {labels}")
        self.update_action_states()

    def job_selection_changed(self, _event: tk.Event | None = None) -> None:
        row = self.selected_job()
        if row:
            self.detail_var.set(f"{row.get('repo', '—')}  •  {row.get('workflow', '—')} › {row.get('job', '—')}  •  {row.get('status', '—')}  •  {row.get('runner', '—')}  •  step {row.get('step', '—')}  •  branch {row.get('branch', '—')}")
        self.update_action_states()

    def local_selection_changed(self, _event: tk.Event | None = None) -> None:
        row = self.selected_local()
        if row:
            self.detail_var.set(f"{row.get('runner', '—')}  •  service {row.get('service_state', '—')}  •  GitHub {row.get('github', '—')}  •  PID {row.get('pid', '—')}  •  {row.get('service', '—')}  •  {row.get('path', '—')}")
        self.update_action_states()

    def tab_changed(self, _event: tk.Event | None = None) -> None:
        self.detail_var.set("Select an item for details and available actions.")
        self.update_action_states()

    def update_action_states(self) -> None:
        job = self.selected_job(); local = self.selected_local()
        self.open_button.configure(state=tk.NORMAL if job and job.get("url") else tk.DISABLED)
        self.diag_button.configure(state=tk.NORMAL if local and local.get("diag_path") else tk.DISABLED)
        self.restart_button.configure(state=tk.NORMAL if local else tk.DISABLED)

    def open_job(self) -> None:
        row = self.selected_job()
        if row and row.get("url"):
            webbrowser.open(str(row["url"]))

    def open_diag(self) -> None:
        row = self.selected_local()
        if not row or not row.get("diag_path"):
            return
        try:
            open_path(str(row["diag_path"]))
        except OSError as exc:
            messagebox.showerror(APP, str(exc), parent=self)

    def restart_selected(self) -> None:
        row = self.selected_local()
        if not row:
            return
        runner, service = row["runner"], row["service"]
        if row.get("github") == "RUNNING" and not messagebox.askyesno(APP, f"{runner} is running a job. Restarting it will interrupt that job. Continue?", icon="warning", parent=self):
            return
        threading.Thread(target=self.restart_worker, args=(runner, service), daemon=True).start()
    def restart_worker(self, runner: str, service: str) -> None:
        try:
            if sys.platform == "win32":
                ps = shutil.which("powershell.exe") or "powershell.exe"
                quoted = "'" + service.replace("'", "''") + "'"
                script = (
                    f"$ErrorActionPreference='Stop'; $n={quoted}; "
                    "$svc=Get-Service -Name $n -ErrorAction Stop; "
                    "if ($svc.Status -eq 'Running') { Restart-Service -Name $n -Force -ErrorAction Stop } "
                    "else { Start-Service -Name $n -ErrorAction Stop }"
                )
                encoded = base64.b64encode(script.encode("utf-16le")).decode("ascii")
                launcher = f"$p=Start-Process -FilePath 'powershell.exe' -Verb RunAs -Wait -PassThru -ArgumentList @('-NoProfile','-EncodedCommand','{encoded}'); exit $p.ExitCode"
                proc = run_command([ps, "-NoProfile", "-Command", launcher], 60)
            else:
                systemctl = shutil.which("systemctl")
                if not systemctl: raise RuntimeError("systemctl is unavailable")
                prefix = ["pkexec"] if shutil.which("pkexec") else ["sudo"] if shutil.which("sudo") else []
                if not prefix: raise RuntimeError("Neither pkexec nor sudo is available")
                proc = run_command(prefix + [systemctl, "restart", service], 60)
            if proc.returncode: raise RuntimeError((proc.stderr or proc.stdout).strip() or "Restart failed")
            self.post(self.status.set, f"Restarted {runner} ({service}).")
            self.post(self.refresh_all)
        except Exception as exc:
            self.post(messagebox.showerror, APP, f"Could not restart {runner}.\n\n{exc}")

    def export_csv(self) -> None:
        index = self.tabs.index(self.tabs.select())
        tree, rows, label = ((self.runner_tree, self.runners, "runners"), (self.job_tree, self.jobs, "active-jobs"), (self.history_tree, list(self.history), "history"), (self.local_tree, self.local_rows, "local-health"))[index]
        filename = filedialog.asksaveasfilename(defaultextension=".csv", initialfile=f"runnerscope-{label}-{time.strftime('%Y%m%d-%H%M%S')}.csv", filetypes=[("CSV files", "*.csv")])
        if not filename: return
        cols = list(tree["columns"])
        with open(filename, "w", newline="", encoding="utf-8-sig") as handle:
            writer = csv.writer(handle); writer.writerow(cols)
            for row in rows: writer.writerow([row.get(c, "") for c in cols])
        self.status.set(f"Exported {len(rows)} row(s).")

    def settings(self) -> None:
        dialog = ConfigDialog(self, load_config())
        self.wait_window(dialog)
        if dialog.result:
            save_config(dialog.result)
            messagebox.showinfo(APP, f"Configuration saved to:\n{CONFIG_FILE}\n\nRestart RunnerScope to apply the changed settings.", parent=self)

    def close(self) -> None:
        self.stop.set(); self.save_history(); self.destroy()


def self_test() -> int:
    assert duration(None) == "—"
    assert duration(65) == "1m 05s"
    assert duration(3661) == "1h 01m"
    assert parse_time("2026-09-02T01:02:03Z") is not None
    assert parse_time("bad") is None
    assert current_step({"steps": [{"name": "Build", "status": "in_progress"}]}) == "Build"
    assert "private-org-name" not in json.dumps(DEFAULTS)
    print(f"{APP} {VERSION} self-test passed")
    return 0


def main() -> int:
    if "--self-test" in sys.argv:
        return self_test()
    enable_windows_dpi_awareness()
    register_private_mercedes_fonts()
    cfg = load_config()
    if cfg is None or not str(cfg.get("organisation") or "").strip():
        cfg = first_run_config()
        if not cfg: return 1
        save_config(cfg)
    try:
        app = RunnerScope(cfg); app.mainloop(); return 0
    except (RuntimeError, OSError, tk.TclError) as exc:
        print(f"Unable to start {APP}: {exc}", file=sys.stderr); return 1


if __name__ == "__main__":
    raise SystemExit(main())
