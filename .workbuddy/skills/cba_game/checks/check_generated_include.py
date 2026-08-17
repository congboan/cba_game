"""Reject #include ".generated.h" when it is not the last include in a .cpp.

Protocol: stdin JSON (path, content, ...); exit 0 = not hit, exit 1 = hit,
exit 2 = gate failure (task abort).
"""
from __future__ import annotations

import json
import re
import sys
import traceback

_GENERATED_INCLUDE = re.compile(r'#include\s+"[^"]*\.generated\.h"')


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
    if not path.endswith(".cpp"):
        return 0

    content = payload.get("content")
    if content is None:
        return 0
    if not isinstance(content, str):
        print("gate input content must be string or null", file=sys.stderr)
        return 2
    if not content:
        return 0

    last_include_line = -1
    gen_line = -1
    for index, line in enumerate(content.split("\n")):
        stripped = line.strip()
        if stripped.startswith("#include"):
            last_include_line = index
            if _GENERATED_INCLUDE.match(stripped):
                gen_line = index

    if gen_line >= 0 and gen_line != last_include_line:
        print("%s: .generated.h must be the last include (line %d), found include after it at line %d"
              % (path, gen_line + 1, last_include_line + 1), file=sys.stderr)
        return 1

    return 0


if __name__ == "__main__":
    try:
        code = main()
    except Exception:
        traceback.print_exc()
        code = 2
    raise SystemExit(code)
