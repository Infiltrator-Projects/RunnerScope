#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
impl = ROOT / "tools" / "apply_ui_contract_migration_impl.py"
source = impl.read_text(encoding="utf-8")
old = '''    count = text.count(old)\n    if count != 1:\n        raise RuntimeError(f"{label}: expected one match, found {count}")\n    return text.replace(old, new, 1)\n'''
new = '''    count = text.count(old)\n    if label == "link Windows UI contract" and count == 2:\n        return text.replace(old, new, 1)\n    if count != 1:\n        raise RuntimeError(f"{label}: expected one match, found {count}")\n    return text.replace(old, new, 1)\n'''
if source.count(old) != 1:
    raise RuntimeError("Unable to patch migration helper deterministically")
source = source.replace(old, new, 1)
namespace = {"__file__": str(impl), "__name__": "__main__"}
exec(compile(source, str(impl), "exec"), namespace)
if impl.exists():
    impl.unlink()
