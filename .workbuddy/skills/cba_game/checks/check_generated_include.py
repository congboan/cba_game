import json, re, sys

_GENERATED_INCLUDE = re.compile(r'#include\s+"[^"]*\.generated\.h"')

def main():
    try:
        ctx = json.load(sys.stdin)
    except Exception:
        sys.exit(0)

    path = ctx.get("path", "")
    if not path.endswith(".cpp"):
        sys.exit(0)

    content = ctx.get("content", "")
    if not content:
        sys.exit(0)

    lines = content.split("\n")
    last_include_line = -1
    gen_line = -1

    for i, line in enumerate(lines):
        stripped = line.strip()
        if stripped.startswith("#include"):
            last_include_line = i
            if _GENERATED_INCLUDE.match(stripped):
                gen_line = i

    if gen_line >= 0 and gen_line != last_include_line:
        print("%s: .generated.h must be the last include (line %d), found include after it at line %d" %
              (path, gen_line + 1, last_include_line + 1), file=sys.stderr)
        sys.exit(1)

    sys.exit(0)

if __name__ == "__main__":
    main()
