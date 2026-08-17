"""Reject UE reflection macros whose target type violates the UE prefix convention.

UCLASS/UINTERFACE -> U/A, USTRUCT -> F, UENUM -> E.

Protocol: stdin JSON (path, content, ...); exit 0 = not hit, exit 1 = hit,
exit 2 = gate failure (task abort).
"""
from __future__ import annotations

import json
import re
import sys
import traceback

_UCLASS = re.compile(r'UCLASS\s*\(')
_USTRUCT = re.compile(r'USTRUCT\s*\(')
_UENUM = re.compile(r'UENUM\s*\(')
_UINTERFACE = re.compile(r'UINTERFACE\s*\(')
_CLASS = re.compile(r'class\s+(\w+)\s*:')
_STRUCT = re.compile(r'struct\s+(\w+)\s*:')
_ENUM = re.compile(r'enum\s+class\s+(\w+)')

def main() -> int:
    try:
        ctx = json.loads(sys.stdin.read() or "{}")
    except (TypeError, ValueError) as error:
        print("gate input not valid JSON: " + str(error), file=sys.stderr)
        return 2
    if not isinstance(ctx, dict):
        print("gate input root must be object", file=sys.stderr)
        return 2

    path = ctx.get("path")
    if not isinstance(path, str) or not path:
        return 0
    if not (path.endswith(".h") or path.endswith(".hpp")):
        return 0

    content = ctx.get("content")
    if content is None:
        return 0
    if not isinstance(content, str):
        print("gate input content must be string or null", file=sys.stderr)
        return 2
    if not content:
        return 0

    macros = []
    for m in _UCLASS.finditer(content):
        macros.append(("UCLASS", m.end()))
    for m in _USTRUCT.finditer(content):
        macros.append(("USTRUCT", m.end()))
    for m in _UENUM.finditer(content):
        macros.append(("UENUM", m.end()))
    for m in _UINTERFACE.finditer(content):
        macros.append(("UINTERFACE", m.end()))

    if not macros:
        return 0

    for kind, pos in macros:
        tail = content[pos:pos + 500]
        name = None

        if kind in ("UCLASS", "UINTERFACE"):
            m = _CLASS.search(tail)
            if m:
                name = m.group(1)
                if not name.startswith("U") and not name.startswith("A"):
                    print("%s: %s macro found but class %s must start with U or A" %
                          (path, kind, name), file=sys.stderr)
                    return 1
        elif kind == "USTRUCT":
            m = _STRUCT.search(tail)
            if m:
                name = m.group(1)
                if not name.startswith("F"):
                    print("%s: USTRUCT macro found but struct %s must start with F" %
                          (path, name), file=sys.stderr)
                    return 1
        elif kind == "UENUM":
            m = _ENUM.search(tail)
            if m:
                name = m.group(1)
                if not name.startswith("E"):
                    print("%s: UENUM macro found but enum %s must start with E" %
                          (path, name), file=sys.stderr)
                    return 1

    return 0


if __name__ == "__main__":
    try:
        code = main()
    except Exception:
        traceback.print_exc()
        code = 2
    raise SystemExit(code)
