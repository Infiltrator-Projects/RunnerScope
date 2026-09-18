# Changelog

## 1.2.0 - 2026-09-18

- Replaces the installed Python/Tk monitor with a native C11/GTK application.
- Removes Python and Tkinter from Debian runtime dependencies.
- Links RunnerScope directly against pinned Infiltratr Common 1.19.2.
- Uses Common for System/Day/Night semantics, formatting, monotonic timing and durable publication.
- Retains `gh` as the authentication/API boundary so RunnerScope stores no token.
- Preserves live runner state, active job scanning, filtering, CSV export, history and Linux service monitoring.

## 1.1.0 - 2026-09-18

- Transitional hybrid release adding a C/Common bridge while retaining Python/Tk.

## 1.0.3

- Restored the rich runner monitor UI/data model from the proven Windows baseline.
