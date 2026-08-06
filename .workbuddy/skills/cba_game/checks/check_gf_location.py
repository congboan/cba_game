#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Reject GameFeature .uplugin writes outside the configured GF directory.

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


def is_gf_uplugin(data):
    if "BuiltInInitialFeatureState" in data:
        return True
    return data.get("ExplicitlyLoaded") is True


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
    if payload.get("operation") == "delete":
        return 0

    constraint_data = payload.get("data")
    if not isinstance(constraint_data, dict):
        constraint_data = {}
    gf_root = tuple(str(s) for s in constraint_data.get("gf_root", _DEFAULT_GF_ROOT))

    norm = normalize_path(path)
    data, error = candidate_descriptor(payload, norm)
    if error:
        print(error)
        return 1

    if is_gf_uplugin(data):
        parts = norm.split("/")
        depth = len(gf_root)
        correct = (
            len(parts) >= depth + 1
            and all(
                parts[i].casefold() == gf_root[i].casefold()
                for i in range(depth)
            )
            and parts[depth] not in {"", ".", ".."}
        )
        if not correct:
            expected = "/".join(gf_root) + "/<name>/"
            print(f"GameFeature .uplugin must be under {expected}: {norm}")
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
