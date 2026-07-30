#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""动态工具 provider / capability 声明的封闭 schema 与匹配机制。

本模块不拥有项目工具声明，也不判断 allow/deny。具体 provider 与 requirement
跟随 root skill、workflow skill 或 spec；这里只验证结构、匹配工具名并公开机制契约。
"""
from __future__ import annotations

import fnmatch
import re


SEMANTIC_EVENTS = ("pre_tool", "pre_write", "pre_command", "pre_commit")
CONFIDENCE_ENUM = ("exact", "partial")

PROVIDER_REQUIRED_KEYS = {
    "id",
    "tool_glob",
    "capabilities",
    "effects",
    "unresolved_events",
}
PROVIDER_ALLOWED_KEYS = set(PROVIDER_REQUIRED_KEYS)
EFFECT_ALLOWED_KEYS = {
    "event",
    "operation",
    "path_field",
    "content_field",
    "command_field",
    "confidence",
}

_IDENTIFIER_RE = re.compile(r"^[a-z0-9][a-z0-9._-]*$")
_FIELD_PATH_RE = re.compile(
    r"^[A-Za-z0-9_-]+(?:\.[A-Za-z0-9_-]+)*$")


def tool_governance_contract() -> dict:
    return {
        "model": "dynamic-tool-capabilities/v1",
        "host_fields": [
            "tool_providers",
            "required_tool_capabilities",
        ],
        "provider_schema": {
            "required": sorted(PROVIDER_REQUIRED_KEYS),
            "effect_allowed": sorted(EFFECT_ALLOWED_KEYS),
            "semantic_events": list(SEMANTIC_EVENTS),
            "confidence": list(CONFIDENCE_ENUM),
        },
        "matching": "case-sensitive tool_glob; one effective provider per tool call",
        "availability": (
            "Provider declarations prove governance mapping, not client installation; "
            "resolve deferred/MCP availability with ToolSearch."
        ),
    }


def _error(code: str, message: str) -> dict:
    return {"code": code, "message": message}


def valid_identifier(value: str) -> bool:
    return bool(_IDENTIFIER_RE.fullmatch(str(value or "")))


def valid_field_path(value: str) -> bool:
    return bool(_FIELD_PATH_RE.fullmatch(str(value or "")))


def normalize_required_capabilities(raw) -> tuple[list[str], list[dict]]:
    if raw is None:
        return [], []
    if not isinstance(raw, list):
        return [], [_error(
            "tool_requirement.list_invalid",
            "required_tool_capabilities 必须是字符串列表")]

    capabilities: list[str] = []
    errors: list[dict] = []
    for ordinal, item in enumerate(raw, start=1):
        value = item.strip() if isinstance(item, str) else ""
        if not valid_identifier(value):
            errors.append(_error(
                "tool_requirement.capability_invalid",
                f"required_tool_capabilities 第 {ordinal} 项必须是小写 capability 标识"))
            continue
        if value not in capabilities:
            capabilities.append(value)
    return capabilities, errors


def validate_tool_provider(provider) -> list[dict]:
    if not isinstance(provider, dict):
        return [_error(
            "tool_provider.item_invalid",
            "tool_providers 条目必须是对象")]

    errors: list[dict] = []
    keys = set(provider)
    missing = sorted(PROVIDER_REQUIRED_KEYS - keys)
    extra = sorted(keys - PROVIDER_ALLOWED_KEYS)
    if missing:
        errors.append(_error(
            "tool_provider.fields_missing",
            f"tool provider 缺少字段: {missing}"))
    if extra:
        errors.append(_error(
            "tool_provider.fields_extra",
            f"tool provider 包含未定义字段: {extra}"))

    provider_id = provider.get("id")
    if not isinstance(provider_id, str) or not valid_identifier(provider_id):
        errors.append(_error(
            "tool_provider.id_invalid",
            "tool provider id 必须是小写稳定标识"))

    tool_glob = provider.get("tool_glob")
    if not isinstance(tool_glob, str) or not tool_glob.strip():
        errors.append(_error(
            "tool_provider.tool_glob_invalid",
            "tool_glob 必须是非空字符串"))
    elif not tool_glob.replace("*", "").replace("?", ""):
        errors.append(_error(
            "tool_provider.tool_glob_too_broad",
            "tool_glob 不得是无字面前缀的全局通配"))

    capabilities = provider.get("capabilities")
    if not isinstance(capabilities, list):
        errors.append(_error(
            "tool_provider.capabilities_invalid",
            "capabilities 必须是列表；不提供额外能力时显式写 []"))
    else:
        seen: set[str] = set()
        for ordinal, item in enumerate(capabilities, start=1):
            if not isinstance(item, str) or not valid_identifier(item):
                errors.append(_error(
                    "tool_provider.capability_invalid",
                    f"capabilities 第 {ordinal} 项必须是小写 capability 标识"))
            elif item in seen:
                errors.append(_error(
                    "tool_provider.capability_duplicate",
                    f"capability 重复: {item}"))
            else:
                seen.add(item)

    unresolved = provider.get("unresolved_events")
    unresolved_set: set[str] = set()
    if not isinstance(unresolved, list):
        errors.append(_error(
            "tool_provider.unresolved_events_invalid",
            "unresolved_events 必须是列表；无未知副作用时显式写 []"))
    else:
        for ordinal, item in enumerate(unresolved, start=1):
            if item not in SEMANTIC_EVENTS:
                errors.append(_error(
                    "tool_provider.unresolved_event_invalid",
                    f"unresolved_events 第 {ordinal} 项非法: {item}"))
            elif item in unresolved_set:
                errors.append(_error(
                    "tool_provider.unresolved_event_duplicate",
                    f"unresolved event 重复: {item}"))
            else:
                unresolved_set.add(item)

    effects = provider.get("effects")
    if not isinstance(effects, list):
        errors.append(_error(
            "tool_provider.effects_invalid",
            "effects 必须是列表；无相关语义效果时显式写 []"))
        return errors

    effect_keys: set[tuple[str, str]] = set()
    for ordinal, effect in enumerate(effects, start=1):
        if not isinstance(effect, dict):
            errors.append(_error(
                "tool_provider.effect_invalid",
                f"effects 第 {ordinal} 项必须是对象"))
            continue
        extra_effect = sorted(set(effect) - EFFECT_ALLOWED_KEYS)
        if extra_effect:
            errors.append(_error(
                "tool_provider.effect_fields_extra",
                f"effects 第 {ordinal} 项包含未定义字段: {extra_effect}"))

        event = effect.get("event")
        if event not in SEMANTIC_EVENTS:
            errors.append(_error(
                "tool_provider.effect_event_invalid",
                f"effects 第 {ordinal} 项 event 非法: {event}"))
            continue

        operation = effect.get("operation", "")
        if operation is not None and not isinstance(operation, str):
            errors.append(_error(
                "tool_provider.effect_operation_invalid",
                f"effects 第 {ordinal} 项 operation 必须是字符串"))
        operation_text = str(operation or "")
        identity = (event, operation_text)
        if identity in effect_keys:
            errors.append(_error(
                "tool_provider.effect_duplicate",
                f"effect 重复: event={event}, operation={operation_text}"))
        effect_keys.add(identity)

        confidence = effect.get("confidence", "exact")
        if confidence not in CONFIDENCE_ENUM:
            errors.append(_error(
                "tool_provider.effect_confidence_invalid",
                f"effects 第 {ordinal} 项 confidence 非法: {confidence}"))
        elif confidence == "partial" and event not in unresolved_set:
            errors.append(_error(
                "tool_provider.partial_without_unresolved",
                f"effects 第 {ordinal} 项为 partial 时必须把 {event} 写入 unresolved_events"))

        for field_name in ("path_field", "content_field", "command_field"):
            if field_name not in effect:
                continue
            value = effect.get(field_name)
            if not isinstance(value, str) or not valid_field_path(value):
                errors.append(_error(
                    "tool_provider.effect_field_path_invalid",
                    f"effects 第 {ordinal} 项 {field_name} 必须是点分字段路径"))

        if event == "pre_write" and "path_field" not in effect:
            errors.append(_error(
                "tool_provider.pre_write_path_missing",
                f"effects 第 {ordinal} 项 pre_write 必须声明 path_field"))
        if event == "pre_command" and "command_field" not in effect:
            errors.append(_error(
                "tool_provider.pre_command_command_missing",
                f"effects 第 {ordinal} 项 pre_command 必须声明 command_field"))
        if event != "pre_write" and any(
                name in effect for name in ("path_field", "content_field")):
            errors.append(_error(
                "tool_provider.effect_field_not_applicable",
                f"effects 第 {ordinal} 项只有 pre_write 可声明 path/content field"))
        if event != "pre_command" and "command_field" in effect:
            errors.append(_error(
                "tool_provider.effect_command_not_applicable",
                f"effects 第 {ordinal} 项只有 pre_command 可声明 command_field"))

    return errors


def tool_matches(tool_glob: str, tool_name: str) -> bool:
    return fnmatch.fnmatchcase(str(tool_name), str(tool_glob))


def matching_providers(tool_name: str, providers: list[dict]) -> list[dict]:
    return [
        provider for provider in providers
        if tool_matches(str(provider.get("tool_glob") or ""), tool_name)
    ]
