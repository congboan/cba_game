#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Reject writes targeting the engine source directory.

Resolves the engine root dynamically via resolve_engine()
(uproject EngineAssociation -> registry/path), then checks whether
the write target path falls inside the engine tree.
"""
from __future__ import annotations

import json
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "..", ".."))
_HARNESS_SCRIPTS = os.path.join(REPO_ROOT, "harness", "scripts")
if _HARNESS_SCRIPTS not in sys.path:
    sys.path.insert(0, _HARNESS_SCRIPTS)


def _normalize(p: str) -> str:
    return os.path.abspath(str(p)).replace("\\", "/").rstrip("/").lower()


def main() -> int:
    try:
        payload = json.loads(sys.stdin.read() or "{}")
    except (TypeError, ValueError) as error:
        print("gate input not valid JSON: " + str(error), file=sys.stderr)
        return 2
    if not isinstance(payload, dict):
        print("gate input root must be object", file=sys.stderr)
        return 2

    target = payload.get("path", "")
    if not target:
        return 0

    try:
        from resolve_engine import resolve_engine  # noqa: E402
        engine_root = resolve_engine()
    except Exception:
        return 0

    if not engine_root:
        return 0

    norm_engine = _normalize(engine_root) + "/"
    norm_target = _normalize(target)

    if norm_target == _normalize(engine_root) or norm_target.startswith(norm_engine):
        print("write target inside engine dir: " + str(target))
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
