#!/usr/bin/env python3
"""Deterministic non-GUI contracts for Runner Monitor's Python compatibility core."""
from __future__ import annotations

from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

import runnerscope as rm


def main() -> int:
    pages = rm._decode_json_stream('{"page":1}\n{"page":2}\n')
    assert [page["page"] for page in pages] == [1, 2]

    assert rm._duration_sort_seconds("1h 02m") == 3720.0
    assert rm._duration_sort_seconds("queue 5m 03s") == 303.0
    assert rm._duration_sort_seconds("—") == float("inf")

    assert rm._match_runner_name(
        ("Actions.Runner.Org.BUILD-01.service",),
        {"build", "BUILD-01"},
    ) == "BUILD-01"
    assert rm._match_runner_name(("unrelated.service",), {"BUILD-01"}) == "—"

    with tempfile.TemporaryDirectory(prefix="runnerscope-config-") as directory:
        original = rm.CONFIG_FILE
        try:
            rm.CONFIG_FILE = Path(directory) / "config.json"
            cfg = dict(rm.DEFAULT_CONFIG)
            cfg.update(
                organisation="Infiltrator-Projects",
                repository_cache_seconds=777.0,
                history_entries=987,
            )
            rm.save_config(cfg)
            loaded = rm.load_config()
            assert loaded is not None
            assert loaded["repository_cache_seconds"] == 777.0
            assert loaded["history_entries"] == 987
        finally:
            rm.CONFIG_FILE = original

    print("Runner Monitor Python logic contracts passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
