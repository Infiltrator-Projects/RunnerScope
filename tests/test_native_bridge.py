#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
from __future__ import annotations

import os
from pathlib import Path
import stat
import subprocess
import sys
import tempfile


def run(helper: str, *args: str, data: bytes | None = None) -> subprocess.CompletedProcess[bytes]:
    return subprocess.run([helper, *args], input=data, stdout=subprocess.PIPE,
                          stderr=subprocess.PIPE, check=True)


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_native_bridge.py PATH-TO-runnerscope-native")
    helper = sys.argv[1]

    expected_version = Path("VERSION").read_text(encoding="utf-8").strip().encode("ascii")
    expected_common = Path("infiltratr-common/VERSION").read_text(encoding="utf-8").strip().encode("ascii")
    assert run(helper, "--version").stdout.strip() == expected_version
    assert run(helper, "--common-version").stdout.strip() == expected_common

    info = run(helper, "--project-info").stdout.decode("utf-8")
    assert "name=Runner Monitor" in info
    assert f"version={expected_version.decode()}" in info
    assert f"common-library=infiltratr-common-{expected_common.decode()}" in info
    assert "source-id=Infiltrator-Projects/RunnerScope" in info

    palette = {}
    for line in run(helper, "--palette", "night").stdout.decode("ascii").splitlines():
        key, value = line.split("=", 1)
        palette[key] = value
    for key in ("background", "text", "success", "warning", "fault", "operation"):
        assert key in palette and len(palette[key]) == 7 and palette[key].startswith("#")

    with tempfile.TemporaryDirectory(prefix="runnerscope-native-") as directory:
        path = Path(directory) / "state.json"
        first = b'{"first":true}\n'
        run(helper, "--atomic-write", str(path), data=first)
        assert path.read_bytes() == first
        assert stat.S_IMODE(path.stat().st_mode) == 0o600

        os.chmod(path, 0o640)
        second = b'{"second":true}\n'
        run(helper, "--atomic-write", str(path), data=second)
        assert path.read_bytes() == second
        assert stat.S_IMODE(path.stat().st_mode) == 0o640

    print("Runner Monitor native bridge contract tests passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
