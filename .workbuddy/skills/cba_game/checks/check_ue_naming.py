import json, re, sys

_UCLASS = re.compile(r'UCLASS\s*\(')
_USTRUCT = re.compile(r'USTRUCT\s*\(')
_UENUM = re.compile(r'UENUM\s*\(')
_UINTERFACE = re.compile(r'UINTERFACE\s*\(')
_CLASS = re.compile(r'class\s+(\w+)\s*:')
_STRUCT = re.compile(r'struct\s+(\w+)\s*:')
_ENUM = re.compile(r'enum\s+class\s+(\w+)')

def main():
    try:
        ctx = json.load(sys.stdin)
    except Exception:
        sys.exit(0)

    path = ctx.get("path", "")
    if not (path.endswith(".h") or path.endswith(".hpp")):
        sys.exit(0)

    content = ctx.get("content", "")
    if not content:
        sys.exit(0)

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
        sys.exit(0)

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
                    sys.exit(1)
        elif kind == "USTRUCT":
            m = _STRUCT.search(tail)
            if m:
                name = m.group(1)
                if not name.startswith("F"):
                    print("%s: USTRUCT macro found but struct %s must start with F" %
                          (path, name), file=sys.stderr)
                    sys.exit(1)
        elif kind == "UENUM":
            m = _ENUM.search(tail)
            if m:
                name = m.group(1)
                if not name.startswith("E"):
                    print("%s: UENUM macro found but enum %s must start with E" %
                          (path, name), file=sys.stderr)
                    sys.exit(1)

    sys.exit(0)

if __name__ == "__main__":
    main()
