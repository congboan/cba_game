#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""计算 UE 项目编译输入的稳定内容指纹。

本模块只定义跨项目稳定的 UE 编译输入集合与哈希协议，不判断门禁 allow/deny。
"""
from __future__ import annotations

import hashlib
import os
import time


FINGERPRINT_MODEL = "ue-compile-inputs-sha256/v1"

SOURCE_ROOTS = ("Source", "Plugins")
SOURCE_SUFFIXES = (
    ".c",
    ".cc",
    ".cpp",
    ".cppm",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".inc",
    ".inl",
    ".ipp",
    ".ixx",
    ".m",
    ".mm",
)
BUILD_RULE_SUFFIXES = (".build.cs", ".target.cs")
PLUGIN_DESCRIPTOR_SUFFIX = ".uplugin"
PROJECT_DESCRIPTOR_SUFFIX = ".uproject"
EXCLUDED_DIRECTORY_NAMES = {
    ".git",
    ".idea",
    ".vs",
    "binaries",
    "deriveddatacache",
    "intermediate",
    "saved",
}


class SourceFingerprintError(RuntimeError):
    """无法完整读取编译输入，不能建立可信源码快照。"""


def _check_deadline(deadline_monotonic: float | None) -> None:
    if (deadline_monotonic is not None
            and time.monotonic() >= deadline_monotonic):
        raise SourceFingerprintError(
            "UE 编译输入指纹计算超出治理求值总预算")


def _relative(path: str, repo_root: str) -> str:
    return os.path.relpath(path, repo_root).replace("\\", "/")


def _compile_input_files(
        repo_root: str, *,
        deadline_monotonic: float | None = None) -> list[tuple[str, str]]:
    root = os.path.abspath(repo_root)
    files: list[tuple[str, str]] = []

    _check_deadline(deadline_monotonic)
    try:
        root_names = sorted(os.listdir(root), key=lambda item: (item.casefold(), item))
    except OSError as exc:
        raise SourceFingerprintError(f"无法读取项目根目录: {exc}") from exc

    for name in root_names:
        path = os.path.join(root, name)
        if (os.path.isfile(path)
                and name.casefold().endswith(PROJECT_DESCRIPTOR_SUFFIX)):
            files.append((path, _relative(path, root)))

    for source_root in SOURCE_ROOTS:
        _check_deadline(deadline_monotonic)
        absolute_root = os.path.join(root, source_root)
        if not os.path.isdir(absolute_root):
            continue

        def _raise_walk_error(error: OSError) -> None:
            raise SourceFingerprintError(
                f"无法遍历 UE 编译输入目录 {source_root}: {error}") from error

        try:
            walker = os.walk(
                absolute_root,
                topdown=True,
                onerror=_raise_walk_error,
                followlinks=False,
            )
            for current, directories, names in walker:
                _check_deadline(deadline_monotonic)
                directories[:] = sorted(
                    (
                        name for name in directories
                        if name.casefold() not in EXCLUDED_DIRECTORY_NAMES
                    ),
                    key=lambda item: (item.casefold(), item),
                )
                for name in sorted(names, key=lambda item: (item.casefold(), item)):
                    _check_deadline(deadline_monotonic)
                    lowered = name.casefold()
                    is_source = (
                        lowered.endswith(SOURCE_SUFFIXES)
                        or lowered.endswith(BUILD_RULE_SUFFIXES)
                    )
                    is_plugin_descriptor = (
                        source_root == "Plugins"
                        and lowered.endswith(PLUGIN_DESCRIPTOR_SUFFIX)
                    )
                    if not is_source and not is_plugin_descriptor:
                        continue
                    path = os.path.join(current, name)
                    if os.path.isfile(path):
                        files.append((path, _relative(path, root)))
        except OSError as exc:
            raise SourceFingerprintError(
                f"无法遍历 UE 编译输入目录 {source_root}: {exc}") from exc

    files.sort(key=lambda item: (item[1].casefold(), item[1]))
    return files


def fingerprint_project_sources(
        repo_root: str, *,
        deadline_monotonic: float | None = None) -> dict:
    """返回可写入 JSON 的稳定指纹；路径与内容变化都会改变 digest。"""
    hasher = hashlib.sha256()
    hasher.update(FINGERPRINT_MODEL.encode("utf-8"))
    hasher.update(b"\0")
    files = _compile_input_files(
        repo_root, deadline_monotonic=deadline_monotonic)
    total_bytes = 0

    for absolute_path, relative_path in files:
        _check_deadline(deadline_monotonic)
        encoded_path = relative_path.encode("utf-8")
        hasher.update(len(encoded_path).to_bytes(8, "big"))
        hasher.update(encoded_path)
        try:
            with open(absolute_path, "rb") as handle:
                while True:
                    _check_deadline(deadline_monotonic)
                    chunk = handle.read(1024 * 1024)
                    if not chunk:
                        break
                    total_bytes += len(chunk)
                    hasher.update(len(chunk).to_bytes(8, "big"))
                    hasher.update(chunk)
        except OSError as exc:
            raise SourceFingerprintError(
                f"无法读取 UE 编译输入 {relative_path}: {exc}") from exc
        hasher.update(b"\0")

    return {
        "model": FINGERPRINT_MODEL,
        "digest": f"sha256:{hasher.hexdigest()}",
        "file_count": len(files),
        "total_bytes": total_bytes,
    }


def fingerprint_matches(expected, actual) -> bool:
    """只比较身份字段；统计字段用于观测，不参与兼容判断。"""
    if not isinstance(expected, dict) or not isinstance(actual, dict):
        return False
    return (
        expected.get("model") == FINGERPRINT_MODEL
        and actual.get("model") == FINGERPRINT_MODEL
        and isinstance(expected.get("digest"), str)
        and expected.get("digest") == actual.get("digest")
    )
