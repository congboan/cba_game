#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""resolve_engine.py — 从 .uproject 的 EngineAssociation 推断引擎根目录。

    可 import 为模块，也可命令行直接运行。

    EngineAssociation 支持三种格式：
      - GUID（如 {18279027-...}）→ Windows 注册表查找
      - 相对路径（如 ../../../UnrealEngine）→ 相对于 .uproject 解析
      - 版本号（如 5.4）→ 注册表 Installs 查找

    查找优先级：
      1. 环境变量 ENGINE_ROOT / UE_ENGINE_ROOT（显式覆盖）
      2. .uproject 的 EngineAssociation → 注册表/路径解析

    用法：
      python harness/scripts/resolve_engine.py          # 打印引擎路径
      python harness/scripts/resolve_engine.py --json   # JSON 输出
"""
from __future__ import annotations

import json
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
UBT_REL = os.path.join("Engine", "Binaries", "DotNET", "UnrealBuildTool", "UnrealBuildTool.exe")


def find_uprojects(project_dir: str | None = None) -> list[str]:
    """列出 project_dir 根目录下全部 .uproject，排序保证结果稳定。"""
    d = project_dir or PROJECT_DIR
    try:
        return sorted(
            os.path.join(d, name) for name in os.listdir(d)
            if name.lower().endswith(".uproject")
        )
    except OSError:
        return []


def find_uproject(project_dir: str | None = None) -> str | None:
    """只在恰好存在一个 .uproject 时返回，避免按目录顺序猜项目。"""
    projects = find_uprojects(project_dir)
    return projects[0] if len(projects) == 1 else None


def _read_engine_association(uproject_path: str) -> str | None:
    """从 .uproject 读取 EngineAssociation 字段。"""
    try:
        with open(uproject_path, "r", encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, ValueError):
        return None
    return str(data.get("EngineAssociation", "")).strip() or None


def _resolve_guid(guid: str) -> str | None:
    """GUID → 注册表 HKCU/HKLM 查找引擎路径。"""
    try:
        import winreg
    except ImportError:
        return None
    for hive in (winreg.HKEY_CURRENT_USER, winreg.HKEY_LOCAL_MACHINE):
        try:
            key = winreg.OpenKey(hive, r"Software\Epic Games\Unreal Engine\Builds")
            for i in range(winreg.QueryInfoKey(key)[1]):
                name, val = winreg.EnumValue(key, i)[0], winreg.EnumValue(key, i)[1]
                if name.lower() == guid.lower() and isinstance(val, str):
                    winreg.CloseKey(key)
                    return val
            winreg.CloseKey(key)
        except OSError:
            continue
    return None


def _registered_source_builds() -> list[str]:
    """列出 UnrealVersionSelector 注册的源码引擎；仅一个候选时可安全补偿失效 GUID。"""
    try:
        import winreg
    except ImportError:
        return []
    roots: list[str] = []
    for hive in (winreg.HKEY_CURRENT_USER, winreg.HKEY_LOCAL_MACHINE):
        try:
            key = winreg.OpenKey(hive, r"Software\Epic Games\Unreal Engine\Builds")
            for i in range(winreg.QueryInfoKey(key)[1]):
                value = winreg.EnumValue(key, i)[1]
                if isinstance(value, str) and _is_engine_root(value):
                    absolute = os.path.abspath(value)
                    if absolute not in roots:
                        roots.append(absolute)
            winreg.CloseKey(key)
        except OSError:
            continue
    return sorted(roots)


def _resolve_version(version: str) -> str | None:
    r"""版本号 → 注册表查找（EpicGames\Unreal Engine\<version>\InstalledDirectory）。"""
    try:
        import winreg
    except ImportError:
        return None
    for hive in (winreg.HKEY_CURRENT_USER, winreg.HKEY_LOCAL_MACHINE):
        key_path = rf"Software\EpicGames\Unreal Engine\{version}"
        try:
            key = winreg.OpenKey(hive, key_path)
            inst_dir = winreg.QueryValueEx(key, "InstalledDirectory")[0]
            winreg.CloseKey(key)
            if inst_dir:
                return inst_dir
        except OSError:
            continue
    return None


def resolve_engine_with_source(project_dir: str | None = None) -> tuple[str | None, str]:
    """从环境或 .uproject 推断引擎根目录，同时返回解析来源。"""
    project_dir = project_dir or PROJECT_DIR

    # 1. 环境变量（显式覆盖）
    for env in ("ENGINE_ROOT", "UE_ENGINE_ROOT"):
        v = os.environ.get(env)
        if v and _is_engine_root(v):
            return os.path.abspath(v), f"environment:{env}"

    # 2. .uproject EngineAssociation
    uproject = find_uproject(project_dir)
    if uproject:
        assoc = _read_engine_association(uproject)
        if assoc:
            engine = None
            source = "uproject:EngineAssociation"
            if assoc.startswith("{"):
                engine = _resolve_guid(assoc)
                if not engine:
                    registered = _registered_source_builds()
                    if len(registered) == 1:
                        engine = registered[0]
                        source = "registry:single-source-build-fallback"
            elif assoc.startswith(".") or "/" in assoc or "\\" in assoc:
                engine = os.path.abspath(os.path.join(os.path.dirname(uproject), assoc.strip('"')))
            else:
                engine = _resolve_version(assoc)
            if engine and _is_engine_root(engine):
                return os.path.abspath(engine), source

    return None, "not_found"


def resolve_engine(project_dir: str | None = None) -> str | None:
    """兼容调用方：只返回引擎根目录。"""
    return resolve_engine_with_source(project_dir)[0]


def _is_engine_root(root: str) -> bool:
    """检查路径是否包含 UnrealBuildTool.exe。"""
    return bool(root and os.path.isfile(os.path.join(root, UBT_REL)))


def main() -> int:
    projects = find_uprojects()
    if not projects:
        print("ERROR: No .uproject found in project root", file=sys.stderr)
        return 1
    if len(projects) > 1:
        names = [os.path.basename(path) for path in projects]
        print(f"ERROR: Multiple .uproject files found: {names}", file=sys.stderr)
        return 1

    engine, source = resolve_engine_with_source()
    if not engine:
        print("ERROR: Engine root not found", file=sys.stderr)
        return 1

    if "--json" in sys.argv:
        print(json.dumps({"engine_root": engine, "ubt": os.path.join(engine, UBT_REL),
                          "source": source},
                         ensure_ascii=False, indent=2))
    else:
        print(engine)
    return 0


if __name__ == "__main__":
    sys.exit(main())
