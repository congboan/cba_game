#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Reject Framework Plugin .uplugin that depends on a GameFeature Plugin.

Constraint data fields:
    gf_root: list of path segments for GF root (default: ["Plugins", "GameFeatures"])

Protocol:
    stdin JSON (path, content, data, ...); exit 0 = not hit, exit 1 = hit,
    exit 2 = gate failure (task abort).
"""
from __future__ import annotations

import json
import os
import sys

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.normpath(os.path.join(SCRIPT_DIR, "..", "..", "..", ".."))

_DEFAULT_GF_ROOT = ("Plugins", "GameFeatures")


def normalize_path(path):
    normalized = path.replace("\\", "/")
    while normalized.startswith("./"):
        normalized = normalized[2:]
    return normalized


def read_existing_descriptor(norm):
    file_path = os.path.realpath(os.path.join(REPO_ROOT, norm))
    try:
        if os.path.commonpath((REPO_ROOT, file_path)) != REPO_ROOT:
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
                "cannot prove the resulting .uplugin from partial JSON; "
                f"use a full-descriptor write ({error})")
    elif not ctx.get("source_tool"):
        data, error = read_existing_descriptor(norm)
        if error:
            return None, error
    else:
        return None, (
            "cannot prove the resulting .uplugin because this mutation "
            "did not provide complete descriptor content")
    if not isinstance(data, dict):
        return None, ".uplugin descriptor root must be an object"
    return data, ""


def game_feature_plugin_names(gf_dir):
    names = set()
    try:
        for entry in os.scandir(gf_dir):
            if not entry.is_dir():
                continue
            try:
                children = list(os.scandir(entry.path))
            except OSError as error:
                raise RuntimeError(
                    f"cannot inspect GameFeature directory {entry.path}: {error}"
                ) from error
            for child in children:
                if child.is_file() and child.name.lower().endswith(".uplugin"):
                    names.add(os.path.splitext(child.name)[0].casefold())
    except OSError as error:
        raise RuntimeError(
            f"cannot inspect GF root: {error}") from error
    return names


def _parts_match(parts, root_segments):
    """Check if path segments start with the configured root segments."""
    if len(parts) < len(root_segments):
        return False
    return all(
        parts[i].casefold() == root_segments[i].casefold()
        for i in range(len(root_segments))
    )


def main():
    try:
        payload = json.loads(sys.stdin.read() or "{}")
    except (TypeError, ValueError) as error:
        print("gate input not valid JSON: " + str(error), file=sys.stderr)
        return 2
    if not isinstance(payload, dict):
        print("gate input root must be object", file=sys.stderr)
        return 2

    path = payload.get("path", "")
    if not path.lower().endswith(".uplugin"):
        return 0

    constraint_data = payload.get("data")
    if not isinstance(constraint_data, dict):
        constraint_data = {}
    gf_root = tuple(str(s) for s in constraint_data.get("gf_root", _DEFAULT_GF_ROOT))

    norm = normalize_path(path)
    parts = norm.split("/")

    # Framework = under the plugins root but NOT under GF root
    plugins_root = gf_root[:1]  # first segment, e.g. ("Plugins",)
    is_framework = _parts_match(parts, plugins_root) and not _parts_match(parts, gf_root)

    if not is_framework or payload.get("operation") == "delete":
        return 0

    gf_dir = os.path.join(REPO_ROOT, *gf_root)
    if not os.path.isdir(gf_dir):
        return 0

    data, error = candidate_descriptor(payload, norm)
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
        print(str(error), file=sys.stderr)
        return 2

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
