#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Reject code writes when no active spec is selected.

Purpose:
    Enforce the "start from a spec" workflow gate. When an AI attempts to write
    to code paths (Source/**, Plugins/**) while harness_state.json has an empty
    active_spec, the write is denied so the task must first go through spec
    construction (and spec activation via state).

Protocol:
    stdin JSON (path, operation, content, ...); exit 0 = not hit,
    exit 1 = hit, other = gate failure (task abort).
"""
from __future__ import annotations

import json
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
# checks/ -> .workbuddy/skills/cba_game/ -> .workbuddy/skills/ -> .workbuddy/ -> repo root
REPO_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "..", ".."))
STATE_FILE = os.path.join(REPO_ROOT, "harness", "state", "harness_state.json")

# Code paths that require an active spec. Normalize to forward slashes for matching.
_CODE_PATTERNS = (
    re.compile(r"^Source/", re.IGNORECASE),
    re.compile(r"^Plugins/", re.IGNORECASE),
)


def _normalize(path: str) -> str:
    return path.replace("\\", "/").lstrip("/")


def _is_code_path(path: str) -> bool:
    norm = _normalize(path)
    return any(pattern.match(norm) for pattern in _CODE_PATTERNS)


def _read_active_spec() -> str | None:
    """Return the active_spec value; None on unreadable/corrupt state (fail-open:
    we cannot evaluate, so do not block writes -- state_field handles that case)."""
    try:
        with open(STATE_FILE, "r", encoding="utf-8") as handle:
            data = json.load(handle)
    except (OSError, ValueError):
        return None
    if not isinstance(data, dict):
        return None
    value = data.get("active_spec")
    return value if isinstance(value, str) else None


def main() -> int:
    try:
        payload = json.loads(sys.stdin.read() or "{}")
    except (TypeError, ValueError) as error:
        print("gate input not valid JSON: " + str(error), file=sys.stderr)
        return 2
    if not isinstance(payload, dict):
        print("gate input root must be object", file=sys.stderr)
        return 2

    path = payload.get("path")
    if not isinstance(path, str) or not path:
        # No path evidence -> cannot evaluate; not a hit (abstain, let other gates run).
        return 0

    if not _is_code_path(path):
        return 0

    active_spec = _read_active_spec()
    if active_spec is None:
        # Cannot read state -> cannot prove the precondition; not a hit.
        return 0

    if not active_spec.strip():
        print("code write blocked: no active spec selected (harness_state.active_spec is empty); build a spec first and activate it via state")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
