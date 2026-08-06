#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Reject hardcoded engine paths in project files.

Detection (two layers):
1. Exact: resolve the real engine path on this machine, deny if content contains it.
2. Fallback: any drive-letter absolute path containing /Engine/ is highly suspicious.
"""
from __future__ import annotations

import json
import os
import re
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "..", ".."))
_HARNESS_SCRIPTS = os.path.join(REPO_ROOT, "harness", "scripts")
if _HARNESS_SCRIPTS not in sys.path:
    sys.path.insert(0, _HARNESS_SCRIPTS)

from resolve_engine import resolve_engine  # noqa: E402

_SUSPICIOUS_ENGINE = re.compile(
    r'[A-Za-z]:[\\/](?:.*[\\/])?Engine[\\/]',
    re.IGNORECASE,
)

# gate version: dynamic engine path detection via resolve_engine()


def _normalize(path):
    return path.replace("\\", "/").rstrip("/").lower()


def main():
    try:
        payload = json.loads(sys.stdin.read() or "{}")
    except (TypeError, ValueError) as error:
        print("gate input not valid JSON: " + str(error), file=sys.stderr)
        return 2
    if not isinstance(payload, dict):
        print("gate input root must be object", file=sys.stderr)
        return 2

    if payload.get("operation") == "delete":
        return 0

    # 豁免 .workbuddy/ 目录：治理配置/memory 中可合法引用引擎路径
    path = payload.get("path", "")
    if ".workbuddy/" in path.replace("\\", "/"):
        return 0

    content = payload.get("content")
    if content is None:
        return 0
    if not isinstance(content, str):
        print("gate input content must be string or null", file=sys.stderr)
        return 2

    try:
        engine_root = resolve_engine()
    except Exception:
        engine_root = None

    if engine_root:
        if _normalize(engine_root) in _normalize(content):
            print("content contains engine path: " + engine_root)
            return 1

    if _SUSPICIOUS_ENGINE.search(content):
        match = _SUSPICIOUS_ENGINE.search(content)
        print("content contains suspicious engine path: " + match.group())
        return 1

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
