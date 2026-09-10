#!/usr/bin/env python3
"""Subagent parallel write-scope gate.

Hit (exit 1): the call comes from a subagent (agent_type non-empty and != "cli"),
the active spec declares parallel_write_sets, and the write target falls outside
that declared scope. Protocol: stdin JSON; 0 = no hit, 1 = hit, other = failure.
Stdlib only (no PyYAML).
"""
from __future__ import annotations

import io
import json
import os
import re
import sys

MAIN_AGENT_TYPES = {"", "cli"}
WRITE_OPERATIONS = {"write", "create", "edit", "delete", "move", "copy"}


def _read_payload() -> dict:
    try:
        sys.stdin = io.TextIOWrapper(
            sys.stdin.buffer, encoding="utf-8", errors="replace")
    except Exception:
        pass
    try:
        raw = sys.stdin.read()
    except Exception:
        return {}
    try:
        value = json.loads(raw)
    except ValueError:
        return {}
    return value if isinstance(value, dict) else {}


def _glob_match(pattern: str, path: str) -> bool:
    """Same semantics as scope_guard._glob_match: a two-star slash matches zero
    or more segments, a single star stays within one segment, and a trailing
    two-star slash also matches the node itself."""
    pattern = str(pattern).lower().replace("\\", "/")
    if pattern.startswith("./"):
        pattern = pattern[2:]
    path = str(path).lower().replace("\\", "/")
    trailing_tree = pattern.endswith("/**")
    if trailing_tree:
        pattern = pattern[:-3]
    parts: list[str] = []
    i = 0
    while i < len(pattern):
        if pattern.startswith("**/", i):
            parts.append("(?:.*/)?")
            i += 3
        elif pattern.startswith("**", i):
            parts.append(".*")
            i += 2
        elif pattern[i] == "*":
            parts.append("[^/]*")
            i += 1
        elif pattern[i] == "?":
            parts.append("[^/]")
            i += 1
        else:
            parts.append(re.escape(pattern[i]))
            i += 1
    if trailing_tree:
        parts.append("(?:/.*)?")
    return re.fullmatch("".join(parts), path) is not None


def _spec_write_sets(repo_root: str, spec_file: str) -> list[str] | None:
    """Read the flat parallel_write_sets string list from spec frontmatter."""
    if not repo_root or not spec_file:
        return None
    try:
        with open(os.path.join(repo_root, spec_file),
                  "r", encoding="utf-8") as handle:
            text = handle.read()
    except OSError:
        return None
    if not text.startswith("---"):
        return None
    end = text.find("\n---", 3)
    frontmatter = text[3:end] if end != -1 else text[3:]
    patterns: list[str] = []
    collecting = False
    for line in frontmatter.splitlines():
        stripped = line.strip()
        if not collecting:
            if stripped.startswith("parallel_write_sets:"):
                collecting = True
            continue
        if stripped.startswith("- "):
            item = stripped[2:].strip().strip('"').strip("'")
            if item:
                patterns.append(item)
            continue
        if stripped and not stripped.startswith("#"):
            break
    return patterns or None


def main() -> int:
    ctx = _read_payload()
    agent_type = str(ctx.get("agent_type") or "").strip()
    if agent_type in MAIN_AGENT_TYPES:
        return 0
    if str(ctx.get("event") or "") != "pre_write":
        return 0
    path = str(ctx.get("path") or "").strip()
    operation = str(ctx.get("operation") or "").strip()
    if not path or (operation and operation not in WRITE_OPERATIONS):
        return 0
    patterns = _spec_write_sets(
        str(ctx.get("repo_root") or ""),
        str(ctx.get("active_spec_file") or ""))
    if not patterns:
        return 0
    allowed = [item for item in patterns if not item.startswith("!")]
    excluded = [item[1:].strip() for item in patterns
                if item.startswith("!") and item[1:].strip()]
    for pattern in excluded:
        if _glob_match(pattern, path):
            print("subagent(" + agent_type
                  + ") wrote an excluded interface/hub file: " + path)
            return 1
    for pattern in allowed:
        if _glob_match(pattern, path):
            return 0
    print("subagent(" + agent_type
          + ") wrote outside the spec parallel write sets: " + path)
    return 1


if __name__ == "__main__":
    sys.exit(main())
