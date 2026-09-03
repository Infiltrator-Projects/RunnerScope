#!/usr/bin/env python3
# Copyright (C) 2026 Shannon Smith
# SPDX-License-Identifier: GPL-3.0-or-later
"""RunnerScope - cross-platform monitor for GitHub Actions self-hosted runners."""
from __future__ import annotations

import base64
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
    from tkinter import filedialog, messagebox, ttk
except ImportError as exc:
    raise SystemExit(f"Tkinter is unavailable: {exc}") from exc

APP = "RunnerScope"
VERSION = "1.0.0"
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
BORDER, TEXT, MUTED = "#353a40", "#e8ecef", "#aeb6bd"
GREEN, BLUE, RED, AMBER, PURPLE = "#63ab7c", "#9aa7b2", "#c96b6b", "#d19e47", "#7fa7c9"


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


class ConfigDialog(tk.Toplevel):
    FIELDS = (
        ("organisation", "GitHub organisation"),
        ("expected_runners", "Expected runners (0 = don't check)"),
        ("runner_poll_seconds", "Runner poll seconds"),
        ("activity_scan_seconds", "Activity scan seconds"),
        ("repository_scan_limit", "Repositories to scan"),
        ("local_health_seconds", "Local health seconds"),
    )

    def __init__(self, parent: tk.Misc, cfg: dict[str, Any] | None = None) -> None:
        super().__init__(parent)
        source = dict(DEFAULTS)
        if cfg:
            source.update(cfg)
        self.result: dict[str, Any] | None = None
        self.vars = {key: tk.StringVar(value=str(source[key])) for key, _ in self.FIELDS}
        self.title(f"{APP} setup")
        self.resizable(False, False)
        self.transient(parent)
        self.grab_set()
        frame = ttk.Frame(self, padding=16)
        frame.pack(fill=tk.BOTH, expand=True)
        ttk.Label(frame, text=f"{APP} configuration", font=("TkDefaultFont", 12, "bold")).grid(row=0, column=0, columnspan=2, sticky="w")
        ttk.Label(frame, text="GitHub credentials remain with gh auth login.").grid(row=1, column=0, columnspan=2, sticky="w", pady=(4, 12))
        for row, (key, label) in enumerate(self.FIELDS, start=2):
            ttk.Label(frame, text=label).grid(row=row, column=0, sticky="w", padx=(0, 12), pady=4)
            ttk.Entry(frame, textvariable=self.vars[key], width=32).grid(row=row, column=1, pady=4)
        buttons = ttk.Frame(frame)
        buttons.grid(row=9, column=0, columnspan=2, sticky="e", pady=(12, 0))
        ttk.Button(buttons, text="Cancel", command=self.destroy).pack(side=tk.RIGHT)
        ttk.Button(buttons, text="Save", command=self.save).pack(side=tk.RIGHT, padx=(0, 8))
        self.protocol("WM_DELETE_WINDOW", self.destroy)

    def save(self) -> None:
        org = self.vars["organisation"].get().strip()
        if not re.fullmatch(r"[A-Za-z0-9_.-]+", org):
            messagebox.showerror(APP, "Enter the GitHub organisation name only.", parent=self)
            return
        try:
            cfg = dict(DEFAULTS)
            cfg.update(
                organisation=org,
                expected_runners=max(0, int(self.vars["expected_runners"].get())),
                runner_poll_seconds=max(1.0, float(self.vars["runner_poll_seconds"].get())),
                activity_scan_seconds=max(10.0, float(self.vars["activity_scan_seconds"].get())),
                repository_scan_limit=max(1, int(self.vars["repository_scan_limit"].get())),
                local_health_seconds=max(5.0, float(self.vars["local_health_seconds"].get())),
            )
        except ValueError:
            messagebox.showerror(APP, "One of the numeric settings is invalid.", parent=self)
            return
        self.result = cfg
        self.destroy()


def first_run_config() -> dict[str, Any] | None:
    root = tk.Tk()
    root.withdraw()
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

        self.title(f"{APP} {VERSION}")
        self.geometry("1450x760")
        self.minsize(1000, 580)
        self.protocol("WM_DELETE_WINDOW", self.close)
        self.configure(background=BG)
        self.configure_style()
        self.build_ui()
        self.after(100, self.drain)
        self.after(1000, self.tick)
        self.request_runners()
        self.request_activity()
        self.request_local()

    def configure_style(self) -> None:
        style = ttk.Style(self)
        if "clam" in style.theme_names():
            style.theme_use("clam")
        style.configure(".", background=BG, foreground=TEXT, fieldbackground=SURFACE, bordercolor=BORDER)
        style.configure("TFrame", background=BG)
        style.configure("TLabel", background=BG, foreground=TEXT)
        style.configure("Meta.TLabel", foreground=MUTED)
        style.configure("Title.TLabel", font=("TkDefaultFont", 18, "bold"))
        style.configure("TButton", background="#d7dde2", foreground="#111418", padding=(10, 5))
        style.configure("TNotebook", background=BG)
        style.configure("TNotebook.Tab", background=PANEL, foreground=MUTED, padding=(14, 7))
        style.map("TNotebook.Tab", background=[("selected", CARD)], foreground=[("selected", TEXT)])
        style.configure("Treeview", background=SURFACE, fieldbackground=SURFACE, foreground=TEXT, rowheight=27)
        style.configure("Treeview.Heading", background=PANEL, foreground=TEXT, padding=(6, 6))

    def build_ui(self) -> None:
        outer = ttk.Frame(self, padding=12)
        outer.pack(fill=tk.BOTH, expand=True)
        head = ttk.Frame(outer)
        head.pack(fill=tk.X)
        ttk.Label(head, text=APP, style="Title.TLabel").pack(side=tk.LEFT)
        self.updated = tk.StringVar(value="Starting…")
        ttk.Label(head, textvariable=self.updated, style="Meta.TLabel").pack(side=tk.RIGHT)
        ttk.Label(outer, text=f"Organisation: {self.org}  •  runner poll {self.poll:g}s  •  activity scan {self.activity_poll:g}s", style="Meta.TLabel").pack(fill=tk.X, pady=(3, 8))

        counters = ttk.Frame(outer)
        counters.pack(fill=tk.X, pady=(0, 8))
        self.counter_vars: dict[str, tk.StringVar] = {}
        for name in ("TOTAL", "RUNNING", "IDLE", "OFFLINE", "SELF-HOSTED ACTIVE", "GITHUB ACTIVE", "QUEUED"):
            var = tk.StringVar(value=f"{name}  0")
            self.counter_vars[name] = var
            ttk.Label(counters, textvariable=var).pack(side=tk.LEFT, padx=(0, 15))

        bar = ttk.Frame(outer)
        bar.pack(fill=tk.X, pady=(0, 6))
        ttk.Label(bar, text="Filter:").pack(side=tk.LEFT)
        self.filter_var = tk.StringVar()
        ttk.Entry(bar, textvariable=self.filter_var, width=38).pack(side=tk.LEFT, padx=6)
        self.filter_var.trace_add("write", lambda *_: self.render_all())
        ttk.Button(bar, text="Clear", command=lambda: self.filter_var.set("")).pack(side=tk.LEFT)

        self.tabs = ttk.Notebook(outer)
        self.tabs.pack(fill=tk.BOTH, expand=True)
        self.runner_tree = self.make_tab("Runners", (
            ("name", "Runner", 190), ("os", "OS", 70), ("state", "State", 90),
            ("repo", "Repository", 160), ("job", "Current job", 280),
            ("runtime", "Runtime", 90), ("labels", "Labels", 300),
        ))
        self.job_tree = self.make_tab("Active jobs", (
            ("where", "Where", 100), ("repo", "Repository", 170), ("workflow", "Workflow", 200),
            ("job", "Job", 220), ("step", "Current step", 220), ("status", "Status", 90),
            ("runner", "Runner", 170), ("runtime", "Runtime/queue", 100), ("branch", "Branch", 120),
        ))
        self.history_tree = self.make_tab("History", (
            ("time", "Time", 130), ("runner", "Runner", 190), ("event", "Event", 100), ("detail", "Detail", 760),
        ))
        self.local_tree = self.make_tab("Local runner health", (
            ("runner", "Runner", 190), ("service", "Service", 250), ("service_state", "Service state", 95),
            ("github", "GitHub", 90), ("pid", "PID", 70), ("diag", "Latest diagnostic", 220),
            ("diag_age", "Diag age", 90), ("path", "Runner path", 360),
        ))
        self.job_tree.bind("<Double-1>", lambda _e: self.open_job())
        self.local_tree.bind("<Double-1>", lambda _e: self.open_diag())

        footer = ttk.Frame(outer)
        footer.pack(fill=tk.X, pady=(8, 0))
        self.status = tk.StringVar(value="Starting…")
        ttk.Label(footer, textvariable=self.status).pack(side=tk.LEFT, fill=tk.X, expand=True)
        ttk.Button(footer, text="Refresh now", command=self.refresh_all).pack(side=tk.RIGHT)
        ttk.Button(footer, text="Settings", command=self.settings).pack(side=tk.RIGHT, padx=(0, 8))
        ttk.Button(footer, text="Export CSV", command=self.export_csv).pack(side=tk.RIGHT, padx=(0, 8))
        ttk.Button(footer, text="Restart local runner", command=self.restart_selected).pack(side=tk.RIGHT, padx=(0, 8))
        ttk.Button(footer, text="Open _diag", command=self.open_diag).pack(side=tk.RIGHT, padx=(0, 8))
        ttk.Button(footer, text="Open job", command=self.open_job).pack(side=tk.RIGHT, padx=(0, 8))

    def make_tab(self, title: str, columns: tuple[tuple[str, str, int], ...]) -> ttk.Treeview:
        frame = ttk.Frame(self.tabs)
        self.tabs.add(frame, text=title)
        tree = ttk.Treeview(frame, columns=[x[0] for x in columns], show="headings")
        for key, label, width in columns:
            tree.heading(key, text=label)
            tree.column(key, width=width, minwidth=50, stretch=True)
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
        self.render_jobs()
        if not self.stop.is_set():
            self.after(1000, self.tick)

    def refresh_all(self) -> None:
        self.request_runners(); self.request_activity(); self.request_local()

    def request_runners(self) -> None:
        with self.lock:
            if self.runner_busy or self.stop.is_set(): return
            self.runner_busy = True
        threading.Thread(target=self.runner_worker, daemon=True).start()
        self.after(int(self.poll * 1000), self.request_runners)

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

    def request_activity(self) -> None:
        with self.lock:
            if self.activity_busy or self.stop.is_set(): return
            self.activity_busy = True
        threading.Thread(target=self.activity_worker, daemon=True).start()
        self.after(int(self.activity_poll * 1000), self.request_activity)

    def activity_worker(self) -> None:
        try:
            repos = self.gh_json(f"/orgs/{self.org}/repos?type=all&sort=pushed&direction=desc&per_page=100")
            names = [r.get("name") for r in repos if r.get("name")][:self.repo_limit]
            active: list[dict[str, Any]] = []
            for repo in names:
                try:
                    runs = self.gh_json(f"/repos/{self.org}/{repo}/actions/runs?per_page=30")
                except Exception:
                    continue
                for run in runs.get("workflow_runs", []):
                    if str(run.get("status") or "").lower() not in self.ACTIVE: continue
                    try:
                        jobs = self.gh_json(f"/repos/{self.org}/{repo}/actions/runs/{run['id']}/jobs?per_page=100&filter=latest")
                    except Exception:
                        continue
                    for job in jobs.get("jobs", []):
                        status = str(job.get("status") or "").lower()
                        if status not in self.ACTIVE: continue
                        runner = job.get("runner_name") or "—"
                        labels = [str(x) for x in job.get("labels", [])]
                        where = "SELF-HOSTED" if "self-hosted" in labels or runner in {r["name"] for r in self.runners} else "GITHUB"
                        started = parse_time(job.get("started_at")); created = parse_time(run.get("created_at"))
                        active.append({
                            "id": str(job.get("id")), "where": where, "repo": repo,
                            "workflow": run.get("name") or "—", "job": job.get("name") or "—",
                            "step": current_step(job), "status": status.upper(), "runner": runner,
                            "started_ts": started, "created_ts": created,
                            "runtime": duration(time.time() - (started or created)) if (started or created) else "—",
                            "branch": run.get("head_branch") or "—", "url": job.get("html_url") or run.get("html_url") or "",
                        })
            active.sort(key=lambda x: (x["where"], x["repo"], x["job"]))
            self.jobs = active
            self.job_by_runner = {x["runner"]: x for x in active if x["where"] == "SELF-HOSTED" and x["runner"] != "—"}
            self.post(self.render_all)
        except Exception as exc:
            self.post(self.status.set, f"Activity scan warning: {exc}")
        finally:
            with self.lock: self.activity_busy = False

    def request_local(self) -> None:
        if sys.platform != "win32" and not sys.platform.startswith("linux"): return
        with self.lock:
            if self.local_busy or self.stop.is_set(): return
            self.local_busy = True
        threading.Thread(target=self.local_worker, daemon=True).start()
        self.after(int(self.local_poll * 1000), self.request_local)

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

    @staticmethod
    def fill(tree: ttk.Treeview, rows: list[dict[str, Any]]) -> None:
        tree.delete(*tree.get_children())
        cols = list(tree["columns"])
        for row in rows: tree.insert("", tk.END, values=[row.get(c, "") for c in cols])

    def render_all(self) -> None:
        self.render_runners(); self.render_jobs(); self.render_history(); self.render_local(); self.render_counters()
        self.updated.set(time.strftime("Updated %H:%M:%S"))

    def render_runners(self) -> None: self.fill(self.runner_tree, self.filtered(self.runners))
    def render_jobs(self) -> None: self.fill(self.job_tree, self.filtered(self.jobs))
    def render_history(self) -> None: self.fill(self.history_tree, self.filtered(list(self.history)))
    def render_local(self) -> None: self.fill(self.local_tree, self.filtered(self.local_rows))

    def render_counters(self) -> None:
        states = [r["state"] for r in self.runners]
        values = {
            "TOTAL": len(states), "RUNNING": states.count("RUNNING"), "IDLE": states.count("IDLE"), "OFFLINE": states.count("OFFLINE"),
            "SELF-HOSTED ACTIVE": sum(j["where"] == "SELF-HOSTED" and j["status"] == "IN_PROGRESS" for j in self.jobs),
            "GITHUB ACTIVE": sum(j["where"] == "GITHUB" and j["status"] == "IN_PROGRESS" for j in self.jobs),
            "QUEUED": sum(j["status"] == "QUEUED" for j in self.jobs),
        }
        for key, value in values.items(): self.counter_vars[key].set(f"{key}  {value}")
        if self.expected and len(states) != self.expected:
            self.status.set(f"GitHub reports {len(states)} runners; expected {self.expected}.")
        else:
            self.status.set(f"Monitoring {len(states)} runner(s) for {self.org}.")

    def add_history(self, runner: str, event: str, detail: str) -> None:
        self.history.appendleft({"time": time.strftime("%d/%m %H:%M:%S"), "runner": runner, "event": event, "detail": detail})
        self.save_history()

    def load_history(self) -> None:
        try:
            data = json.loads(STATE_FILE.read_text(encoding="utf-8")) if STATE_FILE.is_file() else {}
            for row in reversed(data.get("history", [])[-self.max_history:]): self.history.appendleft(row)
        except (OSError, ValueError, TypeError): pass

    def save_history(self) -> None:
        try:
            STATE_FILE.parent.mkdir(parents=True, exist_ok=True)
            STATE_FILE.write_text(json.dumps({"history": list(self.history)}, indent=2), encoding="utf-8")
        except OSError: pass

    def selected_job(self) -> dict[str, Any] | None:
        sel = self.job_tree.selection()
        if not sel: return None
        values = self.job_tree.item(sel[0], "values")
        if not values: return None
        repo, job = values[1], values[3]
        return next((x for x in self.jobs if x["repo"] == repo and x["job"] == job), None)

    def selected_local(self) -> dict[str, Any] | None:
        sel = self.local_tree.selection()
        if not sel: return None
        values = self.local_tree.item(sel[0], "values")
        service = values[1] if values else ""
        return next((x for x in self.local_rows if x["service"] == service), None)

    def open_job(self) -> None:
        row = self.selected_job()
        if row and row.get("url"): webbrowser.open(row["url"])

    def open_diag(self) -> None:
        row = self.selected_local()
        if not row or not row.get("diag_path"): return
        try: open_path(row["diag_path"])
        except OSError as exc: messagebox.showerror(APP, str(exc), parent=self)

    def restart_selected(self) -> None:
        row = self.selected_local()
        if not row: return
        runner, service = row["runner"], row["service"]
        if row.get("github") == "RUNNING" and not messagebox.askyesno(APP, f"{runner} is running a job. Restarting it will interrupt that job. Continue?", icon="warning", parent=self): return
        threading.Thread(target=self.restart_worker, args=(runner, service), daemon=True).start()

    def restart_worker(self, runner: str, service: str) -> None:
        try:
            if sys.platform == "win32":
                ps = shutil.which("powershell.exe") or "powershell.exe"
                quoted = "'" + service.replace("'", "''") + "'"
                script = f"$ErrorActionPreference='Stop'; $n={quoted}; Restart-Service -Name $n -Force -ErrorAction Stop"
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
    if "--self-test" in sys.argv: return self_test()
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
