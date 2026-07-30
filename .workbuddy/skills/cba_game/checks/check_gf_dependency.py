import json
import os
import sys


def derive_repo_root():
    return os.path.normpath(os.path.join(os.getcwd(), "..", "..", ".."))


def protocol_error(message):
    print(message, file=sys.stderr)
    return 2


def normalize_path(path):
    normalized = path.replace("\\", "/")
    while normalized.startswith("./"):
        normalized = normalized[2:]
    return normalized


def load_context():
    try:
        ctx = json.load(sys.stdin)
    except (TypeError, ValueError) as error:
        raise RuntimeError(f"gate input is not valid JSON: {error}") from error
    if not isinstance(ctx, dict):
        raise RuntimeError("gate input root must be an object")

    path = ctx.get("path")
    if not isinstance(path, str):
        raise RuntimeError("gate input path must be a string")
    content = ctx.get("content")
    if content is not None and not isinstance(content, str):
        raise RuntimeError("gate input content must be a string or null")
    operation = ctx.get("operation")
    if operation is not None and not isinstance(operation, str):
        raise RuntimeError("gate input operation must be a string or null")
    source_tool = ctx.get("source_tool")
    if source_tool is not None and not isinstance(source_tool, str):
        raise RuntimeError("gate input source_tool must be a string or null")
    return ctx


def read_existing_descriptor(norm):
    repo_root = os.path.realpath(derive_repo_root())
    file_path = os.path.realpath(os.path.join(repo_root, norm))
    try:
        if os.path.commonpath((repo_root, file_path)) != repo_root:
            return None, "descriptor path escapes the repository"
    except ValueError:
        return None, "descriptor path cannot be resolved inside the repository"
    try:
        with open(file_path, "r", encoding="utf-8") as stream:
            return json.load(stream), ""
    except (OSError, ValueError) as error:
        return None, f"cannot read a complete descriptor: {error}"


def candidate_descriptor(ctx, norm):
    content = ctx.get("content")
    if isinstance(content, str) and content.strip():
        try:
            data = json.loads(content)
        except (TypeError, ValueError) as error:
            return None, (
                "cannot prove the resulting .uplugin from partial or invalid JSON; "
                f"use a full-descriptor write ({error})")
    elif not ctx.get("source_tool"):
        data, error = read_existing_descriptor(norm)
        if error:
            return None, error
    else:
        return None, (
            "cannot prove the resulting .uplugin because this mutation did not "
            "provide complete descriptor content")

    if not isinstance(data, dict):
        return None, ".uplugin descriptor root must be an object"
    return data, ""


def game_feature_plugin_names(gf_dir):
    names = set()
    try:
        entries = list(os.scandir(gf_dir))
    except OSError as error:
        raise RuntimeError(
            f"cannot inspect Plugins/GameFeatures: {error}") from error

    for entry in entries:
        if not entry.is_dir():
            continue
        try:
            children = list(os.scandir(entry.path))
        except OSError as error:
            raise RuntimeError(
                f"cannot inspect GameFeature directory {entry.path}: {error}") from error
        for child in children:
            if child.is_file() and child.name.lower().endswith(".uplugin"):
                names.add(os.path.splitext(child.name)[0].casefold())
    return names


def main():
    try:
        ctx = load_context()
    except RuntimeError as error:
        return protocol_error(str(error))

    path = ctx.get("path", "")
    if not path.lower().endswith(".uplugin"):
        return 0

    norm = normalize_path(path)
    parts = norm.split("/")
    is_framework_descriptor = (
        len(parts) >= 3
        and parts[0].casefold() == "plugins"
        and parts[1].casefold() != "gamefeatures"
    )
    if not is_framework_descriptor or ctx.get("operation") == "delete":
        return 0

    repo_root = derive_repo_root()
    gf_dir = os.path.join(repo_root, "Plugins", "GameFeatures")
    if not os.path.isdir(gf_dir):
        return 0

    data, error = candidate_descriptor(ctx, norm)
    if error:
        print(error)
        return 1

    deps = data.get("Plugins", [])
    if deps is None:
        deps = []
    if not isinstance(deps, list):
        print(".uplugin Plugins field must be a list")
        return 1

    try:
        gf_names = game_feature_plugin_names(gf_dir)
    except RuntimeError as error:
        return protocol_error(str(error))

    for dep in deps:
        if not isinstance(dep, dict):
            print(".uplugin Plugins entries must be objects")
            return 1
        dep_name = dep.get("Name", "")
        if dep_name in (None, ""):
            continue
        if not isinstance(dep_name, str):
            print(".uplugin dependency Name must be a string")
            return 1
        if dep_name.casefold() in gf_names:
            print(
                "Framework Plugin cannot depend on GameFeature Plugin "
                f"{dep_name}: {norm}")
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
