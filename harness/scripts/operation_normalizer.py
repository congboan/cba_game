#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""把 WorkBuddy 工具调用转换为 scope_guard 可求值的语义请求。

本模块只产生证据，不拥有 allow/deny 策略，也不包含任何项目规则。
第一版只覆盖现有 evaluator 需要的 pre_write / pre_command / pre_commit
上下文；新的真实门禁需求出现后再扩展，而不是枚举“所有 AI 行为”。
"""
from __future__ import annotations

import os
import shlex

from tool_governance import SEMANTIC_EVENTS


# 这些集合只缩小“现有 evaluator 所关心的副作用”范围，不是 allow 白名单。
COMMANDS_WITH_NO_MODELED_EFFECTS = {
    "cat",
    "dir",
    "echo",
    "findstr",
    "gci",
    "get-childitem",
    "get-content",
    "get-filehash",
    "get-item",
    "get-location",
    "ls",
    "pwd",
    "resolve-path",
    "select-string",
    "test-path",
    "tree",
    "type",
    "where",
    "where.exe",
    "write-output",
}

GIT_READ_SUBCOMMANDS = {
    "diff",
    "log",
    "ls-files",
    "rev-parse",
    "show",
    "status",
}

GIT_NON_WORKTREE_SUBCOMMANDS = {
    "add",
    "branch",
    "fetch",
    "init",
    "pull",
    "push",
    "remote",
    "reset",
    "rm",
    "stash",
}

PYTHON_PROGRAMS = {
    "py",
    "py.exe",
    "python",
    "python.exe",
    "python3",
    "python3.exe",
}

POWERSHELL_WRITE_COMMANDS = {
    "add-content": "write",
    "clear-content": "write",
    "new-item": "create",
    "set-content": "write",
}

POWERSHELL_DELETE_COMMANDS = {
    "del",
    "erase",
    "remove-item",
    "ri",
    "rm",
}

POWERSHELL_COPY_COMMANDS = {
    "copy",
    "copy-item",
    "cp",
    "cpi",
}

POWERSHELL_MOVE_COMMANDS = {
    "mi",
    "move",
    "move-item",
    "mv",
}

POWERSHELL_RENAME_COMMANDS = {
    "ren",
    "rename",
    "rename-item",
    "rni",
}

POSIX_DELETE_COMMANDS = {"del", "erase", "rm"}
POSIX_CREATE_COMMANDS = {"mkdir", "touch"}
POSIX_COPY_COMMANDS = {"copy", "cp"}
POSIX_MOVE_COMMANDS = {"move", "mv"}

POWERSHELL_SWITCHES = {
    "-append",
    "-confirm",
    "-force",
    "-nonewline",
    "-passthru",
    "-recurse",
    "-whatif",
}

# 官方说明这些 WorkBuddy 工具的当前调用不直接产生现有 evaluator 关心的
# repository pre_write / raw pre_command / git pre_commit 证据。它们仍产生
# pre_tool 请求，仍受动态 tool capability 约束和 WorkBuddy 原生权限控制。
TOOLS_WITH_NO_MODELED_EFFECTS = {
    "AskUserQuestion",
    "EnterPlanMode",
    "ExitPlanMode",
    "Glob",
    "Grep",
    "ListMcpResources",
    "LSP",
    "NotebookRead",
    "Read",
    "ReadMcpResource",
    "StructuredOutput",
    "TaskGet",
    "TaskList",
    "TaskOutput",
    "ToolSearch",
    "WaitForMcpServers",
    "WebFetch",
    "WebSearch",
}


class ToolAdapterError(RuntimeError):
    """已声明 provider 无法按其 schema 解释真实 tool_input。"""

    def __init__(self, code: str, message: str):
        super().__init__(message)
        self.code = code
        self.message = message


def normalization_contract() -> dict:
    return {
        "model": "semantic-evaluation-requests/v2",
        "authorization_policy": "effective_constraints_only",
        "semantic_events": list(SEMANTIC_EVENTS),
        "evidence_fields": [
            "path",
            "path_scope",
            "content",
            "command",
            "operation",
            "source_tool",
            "confidence",
        ],
        "unresolved_policy": (
            "deny_current_call_only_when_an_effective_deny_constraint_"
            "may_be_bypassed"
        ),
        "resolved_capability_policy": (
            "an unscoped require_tool_capability deny is not an unresolved "
            "blocker when the unique effective provider already proves it"
        ),
        "scope": (
            "Only behavior required by implemented evaluators is modeled; "
            "this is not an exhaustive AI behavior taxonomy."
        ),
        "unknown_tool_policy": (
            "valid generic envelope plus no effective provider => unresolved semantic events; "
            "never a hook protocol failure solely because tool_name is new"
        ),
    }


def _clean_token(token: str) -> str:
    value = str(token).strip()
    if (len(value) >= 2
            and value[0] == value[-1]
            and value[0] in {"'", '"'}):
        return value[1:-1]
    return value


def _command_tokens(command: str) -> tuple[list[str], str]:
    """只把能够静态证明为单条命令的文本交给具体 adapter。"""
    quote = ""
    for char in command:
        if char in {"%", "!", "^"}:
            return [], "environment expansion or shell escape"
        if quote == "'":
            if char == "'":
                quote = ""
            continue
        if quote == '"':
            if char == '"':
                quote = ""
            elif char in {"$", "`"}:
                return [], "dynamic expansion inside double quotes"
            continue
        if char in {"'", '"'}:
            quote = char
            continue
        if char in {"\r", "\n", ";", "|", "&", ">", "<", "$", "`",
                    "(", ")", "{", "}", "#"}:
            return [], "compound, redirected, or dynamically expanded command"
    if quote:
        return [], "unclosed quote"
    try:
        tokens = [_clean_token(item)
                  for item in shlex.split(command, posix=False)]
    except ValueError:
        return [], "command parse failure"
    if not tokens or not tokens[0]:
        return [], "empty command"
    if any(token.startswith("@") for token in tokens):
        return [], "response-file or splatting syntax"
    return tokens, ""


def _program_name(token: str) -> str:
    value = _clean_token(token)
    if not value:
        return ""
    return os.path.basename(value).lower()


def _relative_path(path: str, cwd: str, repo_root: str) -> str:
    raw = str(path).strip()
    for prefix in (
            "Microsoft.PowerShell.Core\\FileSystem::",
            "FileSystem::"):
        if raw.lower().startswith(prefix.lower()):
            raw = raw[len(prefix):]
            break
    expanded = os.path.expanduser(raw)
    absolute = (
        os.path.abspath(expanded)
        if os.path.isabs(expanded)
        else os.path.abspath(os.path.join(cwd, expanded))
    )
    try:
        relative = os.path.relpath(absolute, repo_root)
    except ValueError:
        relative = absolute
    return relative.replace("\\", "/")


def _static_path(path: str) -> bool:
    value = str(path).strip()
    return bool(
        value
        and not any(char in value for char in "*?[],$%!")
        and not value.startswith("@")
    )


def _request(event: str, *, tool_name: str, command: str = "",
             path: str = "", path_scope: str = "exact",
             content: str = "", operation: str = "",
             confidence: str = "exact") -> dict:
    if path_scope not in {"exact", "tree"}:
        raise ValueError(f"unsupported path_scope: {path_scope}")
    ctx = {
        "source_tool": tool_name,
        "confidence": confidence,
    }
    if command:
        ctx["command"] = command
    if path:
        ctx["path"] = path
        ctx["path_scope"] = path_scope
    if content or operation in {"write", "create"}:
        ctx["content"] = content
    if operation:
        ctx["operation"] = operation
    return {"event": event, "ctx": ctx}


def _is_link_like(path: str) -> bool:
    """目录链接/联结删除只影响入口本身，不把物理后代伪装成删除目标。"""
    if os.path.islink(path):
        return True
    isjunction = getattr(os.path, "isjunction", None)
    return bool(isjunction and isjunction(path))


def _delete_path_scope(path: str, cwd: str, *,
                       recursive: bool = False) -> str:
    """已声明递归或当前目标是普通目录时，删除影响范围为整棵逻辑子树。"""
    if recursive:
        return "tree"
    absolute = (
        os.path.abspath(path)
        if os.path.isabs(path)
        else os.path.abspath(os.path.join(cwd, path))
    )
    return (
        "tree"
        if os.path.isdir(absolute) and not _is_link_like(absolute)
        else "exact"
    )


def _analysis(tool_name: str, tool_kind: str, requests: list[dict],
              unresolved_events: list[str] | None = None,
              evidence: list[str] | None = None) -> dict:
    unresolved = list(dict.fromkeys(unresolved_events or []))
    return {
        "source_tool": tool_name,
        "tool_kind": tool_kind,
        "status": "unresolved" if unresolved else "exact",
        "evaluation_requests": requests,
        "unresolved_events": unresolved,
        "evidence": evidence or [],
    }


def _input_field(tool_input: dict, field_path: str) -> tuple[bool, object]:
    current: object = tool_input
    for part in str(field_path).split("."):
        if not isinstance(current, dict) or part not in current:
            return False, None
        current = current[part]
    return True, current


def _provider_effect_requests(provider: dict, ctx: dict,
                              repo_root: str) -> list[dict]:
    tool_name = str(ctx.get("tool_name") or "")
    tool_input = ctx.get("tool_input")
    if not isinstance(tool_input, dict):
        raise ToolAdapterError(
            "tool_adapter.tool_input_invalid",
            "动态 provider 要求 tool_input 为对象")
    cwd = str(ctx.get("cwd") or "")
    requests: list[dict] = []
    for ordinal, effect in enumerate(provider.get("effects") or [], start=1):
        event = str(effect.get("event") or "")
        values: dict[str, str] = {}
        for target, declaration in (
                ("path", "path_field"),
                ("content", "content_field"),
                ("command", "command_field")):
            field_path = effect.get(declaration)
            if not field_path:
                continue
            found, value = _input_field(tool_input, str(field_path))
            if not found:
                raise ToolAdapterError(
                    "tool_adapter.required_field_missing",
                    f"provider {provider.get('_provider_id', provider.get('id', ''))} "
                    f"的 effect #{ordinal} 无法读取 tool_input.{field_path}")
            if not isinstance(value, str):
                raise ToolAdapterError(
                    "tool_adapter.required_field_type_invalid",
                    f"provider {provider.get('_provider_id', provider.get('id', ''))} "
                    f"要求 tool_input.{field_path} 为字符串")
            if target in {"path", "command"} and not value.strip():
                raise ToolAdapterError(
                    "tool_adapter.required_field_empty",
                    f"provider {provider.get('_provider_id', provider.get('id', ''))} "
                    f"要求 tool_input.{field_path} 为非空字符串")
            values[target] = value

        path = values.get("path", "")
        if path:
            if not os.path.isabs(path) and not cwd:
                raise ToolAdapterError(
                    "tool_adapter.cwd_missing",
                    f"provider {provider.get('_provider_id', provider.get('id', ''))} "
                    "提取到相对路径，但 hook payload 缺少绝对 cwd")
            path = _relative_path(path, cwd or repo_root, repo_root)

        operation = str(effect.get("operation") or "")
        requests.append(_request(
            event,
            tool_name=tool_name,
            command=values.get("command", ""),
            path=path,
            path_scope=(
                _delete_path_scope(path, repo_root)
                if path and operation == "delete" else "exact"),
            content=values.get("content", ""),
            operation=operation,
            confidence=str(effect.get("confidence") or "exact")))
    return requests


def _finalize_analysis(analysis: dict, ctx: dict, repo_root: str,
                       provider: dict | None) -> dict:
    tool_name = str(ctx.get("tool_name") or "")
    capabilities = list(dict.fromkeys(
        str(item) for item in ((provider or {}).get("capabilities") or [])
        if str(item)))
    provider_ids = (
        [str(provider.get("_provider_id") or provider.get("id") or "")]
        if provider else [])
    provider_ids = [item for item in provider_ids if item]

    pre_tool = _request("pre_tool", tool_name=tool_name)
    provider_requests = (
        _provider_effect_requests(provider, ctx, repo_root)
        if provider else [])
    requests = [pre_tool] + list(analysis.get("evaluation_requests") or [])
    requests.extend(provider_requests)
    for request in requests:
        request_ctx = request.setdefault("ctx", {})
        request_ctx["tool_capabilities"] = capabilities
        request_ctx["tool_provider_ids"] = provider_ids

    unresolved = list(analysis.get("unresolved_events") or [])
    if provider:
        unresolved.extend(provider.get("unresolved_events") or [])
    unresolved = list(dict.fromkeys(str(item) for item in unresolved))

    evidence = list(analysis.get("evidence") or [])
    if provider:
        evidence.append(
            f"dynamic_tool_provider:{provider_ids[0] if provider_ids else '?'}")
    analysis["evaluation_requests"] = requests
    analysis["unresolved_events"] = unresolved
    analysis["evidence"] = evidence
    analysis["tool_capabilities"] = capabilities
    analysis["tool_provider_ids"] = provider_ids
    analysis["adapter_kind"] = (
        "dynamic_provider" if provider and analysis.get("tool_kind") == "generic"
        else "builtin_plus_dynamic_provider" if provider
        else "builtin" if analysis.get("status") != "unresolved"
        else "unresolved")
    analysis["status"] = "unresolved" if unresolved else "exact"
    return analysis


def _parse_powershell_args(args: list[str]) -> tuple[dict[str, str], list[str]]:
    named: dict[str, str] = {}
    positional: list[str] = []
    i = 0
    while i < len(args):
        current = args[i]
        lowered = current.lower()
        if current.startswith("-"):
            if lowered in POWERSHELL_SWITCHES:
                i += 1
                continue
            if i + 1 < len(args):
                named[lowered] = args[i + 1]
                i += 2
                continue
            named[lowered] = ""
            i += 1
            continue
        positional.append(current)
        i += 1
    return named, positional


def _named_path(named: dict[str, str], positional: list[str],
                *names: str) -> str:
    for name in names:
        value = named.get(name.lower())
        if value:
            return value
    return positional[0] if positional else ""


def _target_for_transfer(source: str, destination: str,
                         cwd: str, repo_root: str) -> str:
    dest_absolute = (
        os.path.abspath(destination)
        if os.path.isabs(destination)
        else os.path.abspath(os.path.join(cwd, destination))
    )
    if (destination.endswith(("/", "\\"))
            or os.path.isdir(dest_absolute)):
        dest_absolute = os.path.join(
            dest_absolute, os.path.basename(source.rstrip("/\\")))
    return _relative_path(dest_absolute, cwd, repo_root)


def _powershell_file_analysis(tokens: list[str], *, tool_name: str,
                              command: str, cwd: str,
                              repo_root: str) -> dict | None:
    program = _program_name(tokens[0])
    args = tokens[1:]
    named, positional = _parse_powershell_args(args)
    requests = [_request(
        "pre_command", tool_name=tool_name, command=command)]

    if program in POWERSHELL_WRITE_COMMANDS:
        path = _named_path(
            named, positional, "-literalpath", "-path")
        if not _static_path(path):
            return _analysis(
                tool_name, "command", requests,
                ["pre_write"], ["powershell_file_write_path_unresolved"])
        value = named.get("-value")
        if value is None:
            remaining = list(positional)
            if remaining and remaining[0] == path:
                remaining = remaining[1:]
            value = " ".join(remaining)
        requests.append(_request(
            "pre_write", tool_name=tool_name, command=command,
            path=_relative_path(path, cwd, repo_root),
            content=value or "",
            operation=POWERSHELL_WRITE_COMMANDS[program]))
        return _analysis(
            tool_name, "command", requests,
            evidence=["powershell_static_file_write"])

    if program == "out-file":
        path = _named_path(
            named, positional, "-literalpath", "-filepath")
        if not _static_path(path):
            return _analysis(
                tool_name, "command", requests,
                ["pre_write"], ["powershell_out_file_path_unresolved"])
        requests.append(_request(
            "pre_write", tool_name=tool_name, command=command,
            path=_relative_path(path, cwd, repo_root),
            operation="write", confidence="partial"))
        return _analysis(
            tool_name, "command", requests, ["pre_write"],
            ["powershell_out_file_content_unresolved"])

    if program in POWERSHELL_DELETE_COMMANDS:
        recursive = any(
            str(arg).lower() == "-recurse" for arg in args)
        paths = []
        explicit = (
            named.get("-literalpath")
            or named.get("-path")
        )
        if explicit:
            paths.append(explicit)
        paths.extend(positional)
        if not paths or any(not _static_path(path) for path in paths):
            return _analysis(
                tool_name, "command", requests,
                ["pre_write"], ["powershell_delete_path_unresolved"])
        for path in paths:
            requests.append(_request(
                "pre_write", tool_name=tool_name, command=command,
                path=_relative_path(path, cwd, repo_root),
                path_scope=_delete_path_scope(
                    path, cwd, recursive=recursive),
                operation="delete"))
        return _analysis(
            tool_name, "command", requests,
            evidence=["powershell_static_file_delete"])

    if program in POWERSHELL_COPY_COMMANDS | POWERSHELL_MOVE_COMMANDS:
        source = (
            named.get("-literalpath")
            or named.get("-path")
            or (positional[0] if positional else "")
        )
        destination = (
            named.get("-destination")
            or (positional[1] if len(positional) > 1 else "")
        )
        if not _static_path(source) or not _static_path(destination):
            return _analysis(
                tool_name, "command", requests,
                ["pre_write"], ["powershell_transfer_path_unresolved"])
        if program in POWERSHELL_MOVE_COMMANDS:
            requests.append(_request(
                "pre_write", tool_name=tool_name, command=command,
                path=_relative_path(source, cwd, repo_root),
                path_scope=_delete_path_scope(source, cwd),
                operation="delete"))
        requests.append(_request(
            "pre_write", tool_name=tool_name, command=command,
            path=_target_for_transfer(
                source, destination, cwd, repo_root),
            operation=("move" if program in POWERSHELL_MOVE_COMMANDS
                       else "copy"),
            confidence="partial"))
        return _analysis(
            tool_name, "command", requests, ["pre_write"],
            ["powershell_transfer_content_unresolved"])

    if program in POWERSHELL_RENAME_COMMANDS:
        source = (
            named.get("-literalpath")
            or named.get("-path")
            or (positional[0] if positional else "")
        )
        new_name = (
            named.get("-newname")
            or (positional[1] if len(positional) > 1 else "")
        )
        if not _static_path(source) or not _static_path(new_name):
            return _analysis(
                tool_name, "command", requests,
                ["pre_write"], ["powershell_rename_path_unresolved"])
        source_absolute = (
            os.path.abspath(source)
            if os.path.isabs(source)
            else os.path.abspath(os.path.join(cwd, source))
        )
        target = (
            new_name
            if os.path.isabs(new_name)
            else os.path.join(os.path.dirname(source_absolute), new_name)
        )
        requests.append(_request(
            "pre_write", tool_name=tool_name, command=command,
            path=_relative_path(source, cwd, repo_root),
            path_scope=_delete_path_scope(source, cwd),
            operation="delete"))
        requests.append(_request(
            "pre_write", tool_name=tool_name, command=command,
            path=_relative_path(target, cwd, repo_root),
            operation="move", confidence="partial"))
        return _analysis(
            tool_name, "command", requests, ["pre_write"],
            ["powershell_rename_content_unresolved"])

    return None


def _posix_file_analysis(tokens: list[str], *, tool_name: str,
                         command: str, cwd: str,
                         repo_root: str) -> dict | None:
    program = _program_name(tokens[0])
    raw_args = tokens[1:]
    args = [arg for arg in tokens[1:] if not arg.startswith("-")]
    requests = [_request(
        "pre_command", tool_name=tool_name, command=command)]

    if program in POSIX_DELETE_COMMANDS | POSIX_CREATE_COMMANDS:
        if not args or any(not _static_path(path) for path in args):
            return _analysis(
                tool_name, "command", requests,
                ["pre_write"], ["static_file_path_unresolved"])
        operation = (
            "delete" if program in POSIX_DELETE_COMMANDS else "create")
        recursive = (
            operation == "delete"
            and any(
                str(arg).lower() == "--recursive"
                or (
                    str(arg).startswith("-")
                    and not str(arg).startswith("--")
                    and "r" in str(arg)[1:].lower()
                )
                for arg in raw_args
            )
        )
        for path in args:
            requests.append(_request(
                "pre_write", tool_name=tool_name, command=command,
                path=_relative_path(path, cwd, repo_root),
                path_scope=(
                    _delete_path_scope(path, cwd, recursive=recursive)
                    if operation == "delete" else "exact"),
                operation=operation))
        return _analysis(
            tool_name, "command", requests,
            evidence=["static_file_mutation"])

    if program in POSIX_COPY_COMMANDS | POSIX_MOVE_COMMANDS:
        if len(args) < 2 or any(not _static_path(path) for path in args):
            return _analysis(
                tool_name, "command", requests,
                ["pre_write"], ["static_transfer_path_unresolved"])
        destination = args[-1]
        sources = args[:-1]
        for source in sources:
            if program in POSIX_MOVE_COMMANDS:
                requests.append(_request(
                    "pre_write", tool_name=tool_name, command=command,
                    path=_relative_path(source, cwd, repo_root),
                    path_scope=_delete_path_scope(source, cwd),
                    operation="delete"))
            requests.append(_request(
                "pre_write", tool_name=tool_name, command=command,
                path=_target_for_transfer(
                    source, destination, cwd, repo_root),
                operation=("move" if program in POSIX_MOVE_COMMANDS
                           else "copy"),
                confidence="partial"))
        return _analysis(
            tool_name, "command", requests, ["pre_write"],
            ["static_transfer_content_unresolved"])
    return None


def _python_wrapper_analysis(tokens: list[str], *, tool_name: str,
                             command: str, cwd: str,
                             repo_root: str) -> dict | None:
    if _program_name(tokens[0]) not in PYTHON_PROGRAMS:
        return None
    requests = [_request(
        "pre_command", tool_name=tool_name, command=command)]
    if len(tokens) < 2 or tokens[1].startswith("-"):
        return _analysis(
            tool_name, "command", requests,
            ["pre_write", "pre_commit"],
            ["python_effects_unresolved"])
    script = tokens[1]
    resolved = (
        os.path.abspath(script)
        if os.path.isabs(script)
        else os.path.abspath(os.path.join(cwd, script))
    )
    scripts_dir = os.path.join(repo_root, "harness", "scripts")
    known = {
        os.path.normcase(os.path.realpath(
            os.path.join(scripts_dir, name))): name
        for name in (
            "build_editor.py",
            "init_root_skill.py",
            "resolve_engine.py",
            "scope_guard.py",
        )
    }
    wrapper = known.get(os.path.normcase(os.path.realpath(resolved)))
    if not wrapper:
        return _analysis(
            tool_name, "command", requests,
            ["pre_write", "pre_commit"],
            ["python_script_effects_unresolved"])
    if wrapper == "init_root_skill.py":
        projects = sorted(
            name for name in os.listdir(repo_root)
            if name.lower().endswith(".uproject")
            and os.path.isfile(os.path.join(repo_root, name))
        )
        if len(projects) != 1:
            return _analysis(
                tool_name, "command", requests,
                ["pre_write"],
                ["root_skill_target_unresolved"])
        project_name = os.path.splitext(projects[0])[0]
        target = os.path.join(
            repo_root, ".workbuddy", "skills",
            project_name, "SKILL.md")
        if not os.path.lexists(target):
            requests.append(_request(
                "pre_write", tool_name=tool_name, command=command,
                path=_relative_path(target, cwd, repo_root),
                operation="create"))
    return _analysis(
        tool_name, "command", requests,
        evidence=[f"trusted_harness_wrapper:{wrapper}"])


def _git_analysis(tokens: list[str], *, tool_name: str,
                  command: str) -> dict | None:
    if _program_name(tokens[0]) not in {"git", "git.exe"}:
        return None
    requests = [_request(
        "pre_command", tool_name=tool_name, command=command)]
    args = tokens[1:]
    if args and args[0].lower() == "--no-pager":
        args = args[1:]
    if not args or args[0].startswith("-"):
        return _analysis(
            tool_name, "command", requests,
            ["pre_write", "pre_commit"],
            ["git_operation_unresolved"])
    subcommand = args[0].lower()
    if subcommand == "commit":
        requests.append(_request(
            "pre_commit", tool_name=tool_name, command=command))
        return _analysis(
            tool_name, "command", requests,
            evidence=["git_commit"])
    if subcommand in GIT_READ_SUBCOMMANDS:
        if any(
                arg.lower() in {"--ext-diff", "--output", "--textconv"}
                or arg.lower().startswith("--output=")
                for arg in args[1:]):
            return _analysis(
                tool_name, "command", requests,
                ["pre_write"], ["git_external_program_effects_unresolved"])
        return _analysis(
            tool_name, "command", requests,
            evidence=[f"git_read:{subcommand}"])
    if subcommand in GIT_NON_WORKTREE_SUBCOMMANDS:
        return _analysis(
            tool_name, "command", requests,
            evidence=[f"git_non_worktree:{subcommand}"])
    return _analysis(
        tool_name, "command", requests,
        ["pre_write"], [f"git_worktree_effects_unresolved:{subcommand}"])


def _command_analysis(ctx: dict, repo_root: str) -> dict:
    tool_name = str(ctx.get("tool_name") or "")
    command = str(ctx.get("command") or "")
    cwd = str(ctx.get("cwd") or repo_root)
    raw_request = _request(
        "pre_command", tool_name=tool_name, command=command)
    tokens, parse_error = _command_tokens(command)
    if not tokens:
        return _analysis(
            tool_name, "command", [raw_request],
            ["pre_write", "pre_commit"],
            [f"command_effects_unresolved:{parse_error}"])

    python_analysis = _python_wrapper_analysis(
        tokens, tool_name=tool_name, command=command,
        cwd=cwd, repo_root=repo_root)
    if python_analysis:
        return python_analysis

    git_analysis = _git_analysis(
        tokens, tool_name=tool_name, command=command)
    if git_analysis:
        return git_analysis

    if tool_name.lower() != "bash":
        powershell_analysis = _powershell_file_analysis(
            tokens, tool_name=tool_name, command=command,
            cwd=cwd, repo_root=repo_root)
        if powershell_analysis:
            return powershell_analysis

    posix_analysis = _posix_file_analysis(
        tokens, tool_name=tool_name, command=command,
        cwd=cwd, repo_root=repo_root)
    if posix_analysis:
        return posix_analysis

    program = _program_name(tokens[0])
    if program in {"rg", "rg.exe"}:
        if any(
                arg.lower() in {"--pre", "--hostname-bin"}
                or arg.lower().startswith(("--pre=", "--hostname-bin="))
                for arg in tokens[1:]):
            return _analysis(
                tool_name, "command", [raw_request],
                ["pre_write"], ["rg_external_program_effects_unresolved"])
        return _analysis(
            tool_name, "command", [raw_request],
            evidence=["no_modeled_effects:rg"])
    if program in COMMANDS_WITH_NO_MODELED_EFFECTS:
        return _analysis(
            tool_name, "command", [raw_request],
            evidence=[f"no_modeled_effects:{program}"])

    return _analysis(
        tool_name, "command", [raw_request],
        ["pre_write", "pre_commit"],
        [f"command_effects_unresolved:{program or 'unknown'}"])


def normalize_tool_call(ctx: dict, repo_root: str,
                        provider: dict | None = None) -> dict:
    """返回 evaluation_requests + unresolved_events，不做授权决策。"""
    tool_kind = str(ctx.get("tool_kind") or "")
    tool_name = str(ctx.get("tool_name") or "")
    if tool_kind in {"write", "edit", "delete"}:
        operation = {
            "write": "write",
            "edit": "write",
            "delete": "delete",
        }[tool_kind]
        request = _request(
            "pre_write",
            tool_name=tool_name,
            path=str(ctx.get("path") or ""),
            path_scope=(
                _delete_path_scope(
                    str(ctx.get("path") or ""), repo_root)
                if operation == "delete" else "exact"),
            content=str(ctx.get("content") or ""),
            operation=operation)
        analysis = _analysis(
            tool_name, tool_kind, [request],
            evidence=["structured_file_tool"])
    elif tool_kind == "command":
        analysis = _command_analysis(ctx, repo_root)
    elif tool_name in TOOLS_WITH_NO_MODELED_EFFECTS:
        analysis = _analysis(
            tool_name, tool_kind, [],
            evidence=[f"no_modeled_effects_tool:{tool_name}"])
    elif provider:
        # 动态 provider 的 effects / unresolved_events 是该工具的语义真相源。
        analysis = _analysis(
            tool_name, tool_kind, [],
            evidence=["dynamic_provider_semantics"])
    else:
        analysis = _analysis(
            tool_name, tool_kind, [],
            list(SEMANTIC_EVENTS),
            ["tool_effects_unresolved:no_effective_provider"])
    return _finalize_analysis(analysis, ctx, repo_root, provider)
