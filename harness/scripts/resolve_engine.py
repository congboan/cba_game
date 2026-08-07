#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
resolve_engine.py

Resolve the Unreal Engine root used by a .uproject.

Resolution rules:
1. Optional --engine-root explicit override.
2. Read the .uproject EngineAssociation.
3. Launcher builds are resolved from LauncherInstalled.dat under %PROGRAMDATA%.
4. Registered source/custom builds are resolved from UnrealVersionSelector registry entries.
5. Legacy InstalledDirectory registry entries are supported as a compatibility fallback.
6. Path-like EngineAssociation values are resolved relative to the .uproject.
7. Empty/missing EngineAssociation values search parent directories for an Engine root.
8. No "single candidate" guessing is performed.
9. ENGINE_ROOT / UE_ENGINE_ROOT are used only when --allow-env is requested.

The module keeps resolve_engine() / resolve_engine_with_source() so it can replace
the previous script with minimal changes.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys
from pathlib import Path
from typing import Iterable


# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

SOURCE_BUILD_REGISTRY_KEY = r"Software\Epic Games\Unreal Engine\Builds"
LEGACY_INSTALLED_REGISTRY_ROOT = r"Software\EpicGames\Unreal Engine"

LAUNCHER_MANIFEST_PARTS = (
    "Epic",
    "UnrealEngineLauncher",
    "LauncherInstalled.dat",
)

BUILD_VERSION_REL = Path("Engine", "Build", "Build.version")

UBT_CANDIDATES = (
    Path("Engine", "Binaries", "DotNET", "UnrealBuildTool", "UnrealBuildTool.exe"),
    Path("Engine", "Binaries", "DotNET", "UnrealBuildTool", "UnrealBuildTool.dll"),
)

SCRIPT_DIR = Path(__file__).resolve().parent
ORIGINAL_LAYOUT_PROJECT_DIR = SCRIPT_DIR.parent.parent


# ---------------------------------------------------------------------------
# Generic helpers
# ---------------------------------------------------------------------------

def _norm_path(path: str | os.PathLike[str]) -> str:
    return os.path.normcase(
        os.path.abspath(
            os.path.expandvars(
                os.path.expanduser(str(path))
            )
        )
    )


def _normalize_identifier(identifier: str) -> str:
    return identifier.strip().strip('"').casefold()


def _looks_like_guid(value: str) -> bool:
    return bool(
        re.fullmatch(
            r"\{?[0-9a-fA-F]{8}-"
            r"[0-9a-fA-F]{4}-"
            r"[0-9a-fA-F]{4}-"
            r"[0-9a-fA-F]{4}-"
            r"[0-9a-fA-F]{12}\}?",
            value.strip(),
        )
    )


def _looks_like_path(value: str) -> bool:
    value = value.strip().strip('"')
    if not value:
        return False

    if re.match(r"^[A-Za-z]:[\\/]", value):
        return True

    if value.startswith("\\\\") or value.startswith("//"):
        return True

    if value.startswith("."):
        return True

    return "/" in value or "\\" in value


def _is_engine_root(root: str | os.PathLike[str] | None) -> bool:
    """
    Validate an Unreal Engine root without requiring a prebuilt UBT executable.
    """
    if not root:
        return False

    root_path = Path(_norm_path(root))
    engine_dir = root_path / "Engine"

    if not engine_dir.is_dir():
        return False

    if not (root_path / BUILD_VERSION_REL).is_file():
        return False

    return (
        (engine_dir / "Source").is_dir()
        or (engine_dir / "Binaries").is_dir()
    )


def _find_ubt(root: str | os.PathLike[str] | None) -> str | None:
    if not root:
        return None

    root_path = Path(_norm_path(root))

    for relative in UBT_CANDIDATES:
        candidate = root_path / relative
        if candidate.is_file():
            return str(candidate)

    return None


# ---------------------------------------------------------------------------
# .uproject discovery
# ---------------------------------------------------------------------------

def find_uprojects(project_dir: str | os.PathLike[str] | None = None) -> list[str]:
    """
    List .uproject files directly under project_dir. No recursive guessing.
    """
    directory = Path(_norm_path(project_dir or ORIGINAL_LAYOUT_PROJECT_DIR))

    if not directory.is_dir():
        return []

    try:
        projects = [
            str(path)
            for path in directory.iterdir()
            if path.is_file() and path.suffix.lower() == ".uproject"
        ]
    except OSError:
        return []

    return sorted(projects, key=lambda p: os.path.basename(p).casefold())


def find_uproject(project_or_dir: str | os.PathLike[str] | None = None) -> str | None:
    """
    Accept either a .uproject file path or a project directory.
    A directory is accepted only when it contains exactly one .uproject.
    """
    if project_or_dir is None:
        projects = find_uprojects(ORIGINAL_LAYOUT_PROJECT_DIR)
        return projects[0] if len(projects) == 1 else None

    path = Path(_norm_path(project_or_dir))

    if path.is_file() and path.suffix.lower() == ".uproject":
        return str(path)

    if path.is_dir():
        projects = find_uprojects(path)
        return projects[0] if len(projects) == 1 else None

    return None


def _read_uproject(uproject_path: str | os.PathLike[str]) -> dict:
    with open(uproject_path, "r", encoding="utf-8-sig") as f:
        return json.load(f)


def _read_engine_association_raw(
    uproject_path: str | os.PathLike[str],
) -> tuple[bool, str]:
    """
    Return (field_exists, value), preserving the distinction between
    a missing EngineAssociation and an explicitly empty one.
    """
    try:
        data = _read_uproject(uproject_path)
    except (OSError, ValueError, TypeError):
        return False, ""

    if "EngineAssociation" not in data:
        return False, ""

    value = data.get("EngineAssociation")
    if value is None:
        return True, ""

    return True, str(value).strip()


# ---------------------------------------------------------------------------
# Epic Games Launcher installations
# ---------------------------------------------------------------------------

def _launcher_manifest_path() -> str | None:
    """
    Resolve LauncherInstalled.dat via PROGRAMDATA instead of hard-coding a drive.
    """
    program_data = os.environ.get("PROGRAMDATA")
    if not program_data:
        return None

    path = Path(program_data).joinpath(*LAUNCHER_MANIFEST_PARTS)
    return str(path) if path.is_file() else None


def _launcher_installations() -> dict[str, str]:
    """
    Return normalized EngineAssociation -> engine root for Launcher builds.
    """
    result: dict[str, str] = {}

    manifest = _launcher_manifest_path()
    if not manifest:
        return result

    try:
        with open(manifest, "r", encoding="utf-8-sig") as f:
            data = json.load(f)
    except (OSError, ValueError, TypeError):
        return result

    installation_list = data.get("InstallationList", [])
    if not isinstance(installation_list, list):
        return result

    for item in installation_list:
        if not isinstance(item, dict):
            continue

        app_name = str(item.get("AppName", "")).strip()
        install_location = str(item.get("InstallLocation", "")).strip()

        if not app_name or not install_location:
            continue

        # Standard UE Launcher records are named UE_5.x.
        if not app_name.casefold().startswith("ue_"):
            continue

        identifier = app_name[3:].strip()
        if not identifier:
            continue

        root = _norm_path(install_location)
        if _is_engine_root(root):
            result[_normalize_identifier(identifier)] = root

    return result


# ---------------------------------------------------------------------------
# Registered source/custom builds
# ---------------------------------------------------------------------------

def _registry_hives() -> Iterable[object]:
    try:
        import winreg
    except ImportError:
        return ()

    return (
        winreg.HKEY_CURRENT_USER,
        winreg.HKEY_LOCAL_MACHINE,
    )


def _source_installations() -> dict[str, str]:
    """
    Return normalized GUID/identifier -> engine root from UnrealVersionSelector
    registration entries.
    """
    try:
        import winreg
    except ImportError:
        return {}

    result: dict[str, str] = {}

    for hive in _registry_hives():
        try:
            key = winreg.OpenKey(hive, SOURCE_BUILD_REGISTRY_KEY)
        except OSError:
            continue

        try:
            value_count = winreg.QueryInfoKey(key)[1]

            for index in range(value_count):
                try:
                    name, value, _ = winreg.EnumValue(key, index)
                except OSError:
                    continue

                if not isinstance(name, str) or not isinstance(value, str):
                    continue

                root = _norm_path(value)

                if _is_engine_root(root):
                    result[_normalize_identifier(name)] = root
        finally:
            winreg.CloseKey(key)

    return result


def _resolve_legacy_installed_version(version: str) -> str | None:
    """
    Compatibility path for older InstalledDirectory registry records.
    """
    try:
        import winreg
    except ImportError:
        return None

    version = version.strip()
    if not version:
        return None

    key_path = rf"{LEGACY_INSTALLED_REGISTRY_ROOT}\{version}"

    for hive in _registry_hives():
        try:
            key = winreg.OpenKey(hive, key_path)
        except OSError:
            continue

        installed_directory = None

        try:
            try:
                installed_directory = winreg.QueryValueEx(
                    key,
                    "InstalledDirectory",
                )[0]
            except OSError:
                pass
        finally:
            winreg.CloseKey(key)

        if isinstance(installed_directory, str):
            root = _norm_path(installed_directory)

            if _is_engine_root(root):
                return root

    return None


def enumerate_engine_installations() -> dict[str, str]:
    """
    Merge Launcher builds and UnrealVersionSelector-registered source builds.
    """
    result: dict[str, str] = {}
    result.update(_launcher_installations())
    result.update(_source_installations())
    return result


# ---------------------------------------------------------------------------
# Relative / parent-directory resolution
# ---------------------------------------------------------------------------

def _resolve_association_path(
    association: str,
    uproject_path: str,
) -> str | None:
    raw = association.strip().strip('"')
    if not raw:
        return None

    raw = os.path.expandvars(os.path.expanduser(raw))
    path = Path(raw)

    if not path.is_absolute():
        path = Path(uproject_path).parent / path

    root = _norm_path(path)
    return root if _is_engine_root(root) else None


def _find_parent_engine(uproject_path: str | os.PathLike[str]) -> str | None:
    """
    For empty/missing EngineAssociation, walk upward until an Engine root is found.
    """
    current = Path(_norm_path(Path(uproject_path).parent))

    while True:
        if _is_engine_root(current):
            return str(current)

        parent = current.parent
        if parent == current:
            break

        current = parent

    return None


# ---------------------------------------------------------------------------
# Optional explicit environment fallback
# ---------------------------------------------------------------------------

def _resolve_environment_override() -> tuple[str | None, str | None]:
    for env_name in ("ENGINE_ROOT", "UE_ENGINE_ROOT"):
        value = os.environ.get(env_name)
        if not value:
            continue

        root = _norm_path(value)

        if _is_engine_root(root):
            return root, env_name

    return None, None


# ---------------------------------------------------------------------------
# Public resolver
# ---------------------------------------------------------------------------

def resolve_engine_with_source(
    project_dir: str | os.PathLike[str] | None = None,
    *,
    explicit_engine_root: str | os.PathLike[str] | None = None,
    allow_env: bool = False,
) -> tuple[str | None, str]:
    """
    Resolve an engine root and return (root, source_description).
    """
    # 0. Explicit CLI/API override.
    if explicit_engine_root:
        root = _norm_path(explicit_engine_root)

        if _is_engine_root(root):
            return root, "explicit:engine-root"

        return None, "invalid_explicit_engine_root"

    # 1. Locate the .uproject.
    if project_dir is None:
        uproject = find_uproject()
    else:
        uproject = find_uproject(project_dir)

    if not uproject:
        return None, "uproject_not_found_or_ambiguous"

    # 2. Read EngineAssociation.
    field_exists, association = _read_engine_association_raw(uproject)

    if field_exists and association:
        # 2a. Association is a path.
        if _looks_like_path(association):
            root = _resolve_association_path(association, uproject)

            if root:
                return root, f"uproject:path:{association}"

            if allow_env:
                env_root, env_name = _resolve_environment_override()
                if env_root:
                    return env_root, f"environment:{env_name}:fallback"

            return None, f"invalid_uproject_path_association:{association}"

        # 2b. Association is an identifier: exact-match known installations.
        installations = enumerate_engine_installations()
        key = _normalize_identifier(association)

        root = installations.get(key)

        if root and _is_engine_root(root):
            if _looks_like_guid(association):
                return root, f"registry:source-build:{association}"

            return root, f"launcher-or-registered:{association}"

        # 2c. Compatibility with legacy version registry entries.
        if not _looks_like_guid(association):
            root = _resolve_legacy_installed_version(association)

            if root:
                return root, f"registry:legacy-version:{association}"

        # Never guess another engine just because it is the only candidate.
        if allow_env:
            env_root, env_name = _resolve_environment_override()

            if env_root:
                return env_root, f"environment:{env_name}:fallback"

        return None, f"association_not_registered:{association}"

    # 3. Empty or missing EngineAssociation: search parents.
    root = _find_parent_engine(uproject)

    if root:
        if field_exists:
            return root, "uproject:empty-association:parent-engine"

        return root, "uproject:missing-association:parent-engine"

    # 4. Optional environment fallback.
    if allow_env:
        env_root, env_name = _resolve_environment_override()

        if env_root:
            return env_root, f"environment:{env_name}:fallback"

    if field_exists:
        return None, "empty_association_parent_engine_not_found"

    return None, "missing_association_parent_engine_not_found"


def resolve_engine(
    project_dir: str | os.PathLike[str] | None = None,
    *,
    explicit_engine_root: str | os.PathLike[str] | None = None,
    allow_env: bool = False,
) -> str | None:
    """
    Compatibility helper: return only the resolved engine root.
    """
    return resolve_engine_with_source(
        project_dir,
        explicit_engine_root=explicit_engine_root,
        allow_env=allow_env,
    )[0]


# ---------------------------------------------------------------------------
# CLI diagnostics
# ---------------------------------------------------------------------------

def _installation_rows() -> list[dict[str, str]]:
    rows: list[dict[str, str]] = []

    for identifier, root in _launcher_installations().items():
        rows.append(
            {
                "kind": "launcher",
                "identifier": identifier,
                "engine_root": root,
            }
        )

    for identifier, root in _source_installations().items():
        rows.append(
            {
                "kind": "registered_source",
                "identifier": identifier,
                "engine_root": root,
            }
        )

    rows.sort(key=lambda row: (row["kind"], row["identifier"]))
    return rows


def _default_project_input() -> str:
    """
    Preserve the previous script layout first, then try nearby/common locations.
    """
    candidates = (
        ORIGINAL_LAYOUT_PROJECT_DIR,
        SCRIPT_DIR,
        Path.cwd(),
    )

    for candidate in candidates:
        if len(find_uprojects(candidate)) == 1:
            return str(candidate)

    return str(ORIGINAL_LAYOUT_PROJECT_DIR)


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Resolve the Unreal Engine root used by a .uproject."
    )

    parser.add_argument(
        "--project",
        help="Path to a .uproject file or project directory.",
    )

    parser.add_argument(
        "--engine-root",
        help="Explicit engine-root override. Highest priority.",
    )

    parser.add_argument(
        "--allow-env",
        action="store_true",
        help="Allow ENGINE_ROOT / UE_ENGINE_ROOT only as a final fallback.",
    )

    parser.add_argument(
        "--json",
        action="store_true",
        help="Output JSON.",
    )

    parser.add_argument(
        "--list",
        action="store_true",
        help="List detected Launcher and registered source/custom engines.",
    )

    return parser


def main() -> int:
    parser = _build_parser()
    args = parser.parse_args()

    if args.list:
        rows = _installation_rows()
        manifest = _launcher_manifest_path()

        if args.json:
            print(
                json.dumps(
                    {
                        "launcher_manifest": manifest,
                        "installations": rows,
                    },
                    ensure_ascii=False,
                    indent=2,
                )
            )
        else:
            print(f"LauncherInstalled.dat: {manifest or '<not found>'}")

            if not rows:
                print("No Unreal Engine installations detected.")
                return 0

            for row in rows:
                print(
                    f"[{row['kind']}] "
                    f"{row['identifier']} -> {row['engine_root']}"
                )

        return 0

    project_input = args.project or _default_project_input()

    engine_root, source = resolve_engine_with_source(
        project_input,
        explicit_engine_root=args.engine_root,
        allow_env=args.allow_env,
    )

    uproject = find_uproject(project_input)
    association_exists = False
    association = ""

    if uproject:
        association_exists, association = _read_engine_association_raw(uproject)

    payload = {
        "ok": bool(engine_root),
        "project_input": _norm_path(project_input),
        "uproject": uproject,
        "engine_association_present": association_exists,
        "engine_association": association,
        "engine_root": engine_root,
        "ubt": _find_ubt(engine_root),
        "source": source,
        "launcher_manifest": _launcher_manifest_path(),
    }

    if args.json:
        print(json.dumps(payload, ensure_ascii=False, indent=2))
        return 0 if engine_root else 1

    if engine_root:
        # Keep the old script behavior: plain mode prints only Engine Root.
        print(engine_root)
        return 0

    print(
        f"ERROR: Engine root not found ({source})",
        file=sys.stderr,
    )

    if uproject:
        print(f"  uproject: {uproject}", file=sys.stderr)

        association_text = (
            repr(association)
            if association_exists
            else "<missing>"
        )

        print(
            f"  EngineAssociation: {association_text}",
            file=sys.stderr,
        )

    print(
        "  Tip: run with --list or --json for diagnostics.",
        file=sys.stderr,
    )

    return 1


if __name__ == "__main__":
    raise SystemExit(main())
