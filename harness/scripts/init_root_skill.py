#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""从空模板初始化项目 root skill；只创建，不覆盖，也不补造项目规则。"""
from __future__ import annotations

import json
import os
import re
import sys
from pathlib import Path


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parent.parent
TEMPLATE_PATH = REPO_ROOT / "harness" / "templates" / "root-skill.md"
PROJECT_NAME_PATTERN = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_.-]{0,63}$")


def _detect_project_name() -> str:
    projects = sorted(REPO_ROOT.glob("*.uproject"))
    if len(projects) != 1:
        names = [path.name for path in projects]
        raise ValueError(
            "无法确定项目名：初始化 root skill 需要仓库根目录恰有一个 .uproject；"
            f"当前发现 {names or '[]'}。请先由人工解决项目身份。")
    name = projects[0].stem
    if not PROJECT_NAME_PATTERN.fullmatch(name):
        raise ValueError(
            "项目名只能包含字母、数字、下划线、点和连字符，且必须以字母或数字开头。")
    return name


def _emit(payload: dict) -> None:
    print(json.dumps(payload, ensure_ascii=False, indent=2))


def _display_path(path: Path) -> str:
    try:
        return path.relative_to(REPO_ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def main() -> int:
    try:
        project_name = _detect_project_name()
    except ValueError as exc:
        _emit({"status": "error", "code": "root_skill.project_name_unresolved",
               "message": str(exc)})
        return 2

    target = REPO_ROOT / ".workbuddy" / "skills" / project_name / "SKILL.md"
    relative_target = target.relative_to(REPO_ROOT).as_posix()
    if os.path.lexists(target):
        _emit({
            "status": "refused",
            "code": "root_skill.already_exists",
            "target": relative_target,
            "message": "root skill 已存在，初始化器拒绝覆盖。",
        })
        return 2
    if not TEMPLATE_PATH.is_file():
        _emit({
            "status": "error",
            "code": "root_skill.template_missing",
            "template": _display_path(TEMPLATE_PATH),
            "message": "root skill 模板不存在，请交由人工修复 harness。",
        })
        return 1

    content = TEMPLATE_PATH.read_text(encoding="utf-8").replace(
        "__PROJECT_NAME__", project_name)
    target.parent.mkdir(parents=True, exist_ok=True)
    try:
        with target.open("x", encoding="utf-8", newline="\n") as stream:
            stream.write(content)
    except FileExistsError:
        _emit({
            "status": "refused",
            "code": "root_skill.already_exists",
            "target": relative_target,
            "message": "root skill 已存在，初始化器拒绝覆盖。",
        })
        return 2

    _emit({
        "status": "created",
        "project": project_name,
        "project_name_source": "uproject",
        "target": relative_target,
        "template": _display_path(TEMPLATE_PATH),
        "next_action": "请与用户确认后完善项目事实、配表和约束；不要把空骨架视为已完成治理。",
    })
    return 0


if __name__ == "__main__":
    sys.exit(main())
