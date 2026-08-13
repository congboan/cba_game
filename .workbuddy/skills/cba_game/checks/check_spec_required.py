#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Reject code writes when no active spec is selected.

Purpose:
    Enforce the "start from a spec" workflow gate. When an AI attempts to write
    to code paths (configured via constraint data.code_paths) while
    harness_state.json has an empty active_spec, the write is denied.

Constraint data fields:
    code_paths: list of glob patterns for code paths (default: ["Source/**", "Plugins/**"])

Protocol:
    stdin JSON (path, data, ...); exit 0 = not hit, exit 1 = hit, other = gate failure.
"""
from __future__ import annotations

import json
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "..", ".."))
STATE_FILE = os.path.join(REPO_ROOT, "harness", "state", "harness_state.json")

_DEFAULT_CODE_PATHS = ("Source/**", "Plugins/**")


def _compile_patterns(patterns):
    result = []
    for p in patterns:
        rx = "^" + re.escape(str(p)).replace("\\*\\*", ".*?").replace("\\*", "[^/]*") + "$"
        result.append(re.compile(rx, re.IGNORECASE))
    return tuple(result)


def _normalize(path: str) -> str:
    return path.replace("\\", "/").lstrip("/")


def _is_code_path(path: str, patterns) -> bool:
    norm = _normalize(path)
    return any(p.match(norm) for p in patterns)


def _read_active_spec() -> str:
    try:
        with open(STATE_FILE, "r", encoding="utf-8") as handle:
            state = json.load(handle)
    except (OSError, ValueError) as error:
        raise RuntimeError("state file missing or corrupted") from error
    if not isinstance(state, dict):
        raise RuntimeError("state root must be object")
    value = state.get("active_spec")
    if not isinstance(value, str):
        raise RuntimeError("active_spec must be string")
    return value


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
        return 0

    constraint_data = payload.get("data")
    if not isinstance(constraint_data, dict):
        constraint_data = {}
    raw_paths = constraint_data.get("code_paths", _DEFAULT_CODE_PATHS)
    code_patterns = _compile_patterns(raw_paths)

    if not _is_code_path(path, code_patterns):
        return 0

    try:
        active_spec = _read_active_spec()
    except RuntimeError as error:
        print("gate state failure: " + str(error), file=sys.stderr)
        return 2

    if not active_spec.strip():
        print("code write blocked: no active spec selected")
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
