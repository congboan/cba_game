#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""scope_guard.py — WorkBuddy 动态治理求值引擎（机制封闭 / 数据开放 / skill 门禁随宿主生灭）。

统一入口：约定定位根 skill（.workbuddy/skills/<项目名>/SKILL.md），收集其 frontmatter
的项目级约束 + 各做法 skill 的约束。PreToolUse 载荷先由显式工具 adapter 转换为求值器需要的
语义证据，再按当前任务实际生效的约束求值；settings 只负责静态 hook 路由。
运行时状态（stage/active_spec/active_skills）读 harness/state/harness_state.json。

求值器是封闭枚举，每个对应一个独立函数；新增类型=改机制加函数+登记 EVALUATORS，
数据里禁止发明逻辑/组合表达式。复杂项目逻辑通过 owner-bound `script` 扩展点加载当前有效
root/workflow skill 自有的受信 Python 门禁。

用法：
  python scope_guard.py --context                      输出当前治理状态 + 已加载实例数
  python scope_guard.py --explain                      逐条输出约束来源、宿主与生命周期
  python scope_guard.py --preview-skills [<host_id> ...] 只读预检候选 workflow skill 组合
  python scope_guard.py --hook-stdin                   从 stdin 读 PreToolUse payload 并自动判定
  python scope_guard.py --event stop                   判定 stop 时机（Stop hook 用）
  python scope_guard.py <file_path>                    便捷：按 pre_write 判定某路径

退出码：0 = harness abstain / 2 = 拒绝当前调用或终止任务；治理失败不得 fail-open。
PreToolUse 输出有四种结果：abstain（不输出 permissionDecision）、deny_current_call、
human_confirmation（permissionDecision:ask）、abort_task。
"""
from __future__ import annotations

import io
import json
import os
import subprocess
import sys
import time

from operation_normalizer import (
    ToolAdapterError,
    normalization_contract,
    normalize_tool_call,
)
from source_fingerprint import (
    SourceFingerprintError,
    fingerprint_matches,
    fingerprint_project_sources,
)
from tool_governance import (
    SEMANTIC_EVENTS,
    matching_providers,
    normalize_required_capabilities,
    tool_governance_contract,
    validate_tool_provider,
)

# Windows 下强制 UTF-8 输出，避免中文 reason 乱码（GBK）。
try:
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding="utf-8", errors="replace")
except Exception:
    pass

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
SKILLS_DIR = os.path.join(REPO_ROOT, ".workbuddy", "skills")
STATE_PATH = os.path.join(REPO_ROOT, "harness", "state", "harness_state.json")
ROOT_SKILL_TEMPLATE_PATH = os.path.join(REPO_ROOT, "harness", "templates", "root-skill.md")

WHEN_ENUM = ("pre_tool", "pre_write", "pre_command", "pre_commit", "stop")
ACTION_ENUM = ("deny", "warn")
GOVERNANCE_MODEL_VERSION = "dynamic-hosted-constraints/v2"
WORKBUDDY_GATE_TIMEOUT_SECONDS = 60
HARNESS_EVALUATION_BUDGET_SECONDS = 45
HARNESS_REPORT_RESERVE_SECONDS = (
    WORKBUDDY_GATE_TIMEOUT_SECONDS - HARNESS_EVALUATION_BUDGET_SECONDS)
SCRIPT_TIMEOUT_DEFAULT_SECONDS = 10
SCRIPT_TIMEOUT_MAX_SECONDS = 30
SCRIPT_OUTPUT_LIMIT = 4096

HARNESS_CONTROL_PLANE_TARGETS = (
    (".codebuddy/settings.json", "file", "WorkBuddy 项目 hook 与权限路由"),
    (".codebuddy/settings.local.json", "file", "可覆盖项目配置的本地 WorkBuddy 设置"),
    ("harness/scripts", "tree", "Harness 可执行机制与其 Python 依赖"),
)

HARNESS_POLICY_PLANE_TARGETS = (
    ("harness/state/harness_state.json", "file", "任务阶段与 skill/spec 激活选择"),
    (".workbuddy/skills", "tree", "root/workflow skill 治理源"),
)

SOURCE_ROLES = {
    "root_skill": "可选的项目知识、配表、项目生命周期约束与自有门禁脚本；存在时始终生效",
    "workflow_skill": "特定做法、流程、生命周期约束与自有门禁脚本",
    "spec": "单个需求的范围、验收与爆炸半径约束；不直接执行脚本",
}

LIFECYCLES = {
    "root_skill": "project",
    "workflow_skill": "workflow",
    "spec": "request",
}

SPEC_FORBIDDEN_FRONTMATTER_FIELDS = {
    "spec_id": (
        "spec.identity_duplicate",
        "spec_id 不得出现在 spec frontmatter；spec 身份只由规范文件名 specs/<id>.md 决定",
    ),
    "stage": (
        "spec.runtime_stage_duplicate",
        "stage 不得出现在 spec frontmatter；当前运行阶段只由 harness state 决定",
    ),
}

EVALUATOR_DATA_SCHEMA = {
    "path_glob": ({"pattern"}, {"pattern", "invert"}),
    "command_write": (
        {"asset_patterns", "write_hints", "read_hints"},
        {"asset_patterns", "write_hints", "read_hints"}),
    "state_field": ({"file", "field", "equals"}, {"file", "field", "equals", "missing_equals"}),
    "build_freshness": ({"file"}, {"file"}),
    "path_writable_stage": ({"stages", "code"}, {"stages", "code"}),
    "require_tool_capability": (
        {"capability"},
        {"capability", "path_pattern", "operation"}),
    "require_active_skill": (
        {"skill", "path_patterns"},
        {"skill", "path_patterns", "exempt_patterns"}),
    "script": ({"script"}, None),  # 允许约束声明任意自定义 data 字段透传给脚本
}

WORKBUDDY_TOOL_KINDS = {
    "Write": "write",
    "write_to_file": "write",
    "Edit": "edit",
    "replace_in_file": "edit",
    "delete_files": "delete",
    "Bash": "command",
    "execute_command": "command",
    "PowerShell": "command",
}

WORKBUDDY_ADAPTER_STATUS = {
    "Write": "verified",
    "write_to_file": "verified",
    "Edit": "verified",
    "replace_in_file": "verified",
    "delete_files": "verified",
    "Bash": "verified",
    "execute_command": "verified",
    "PowerShell": "configured_from_user_observation",
}

# 恢复态只接受已经从 WorkBuddy 真实会话观察到的 Read 载荷。
# 它不进入正常工具语义，也不授权其他 generic 工具。
WORKBUDDY_RECOVERY_READ_FIELDS = {
    "Read": ("file_path",),
    "Glob": ("path",),
}

def _uproject_files() -> list[str]:
    """返回仓库根目录下全部 .uproject；排序保证诊断稳定。"""
    try:
        return sorted(
            name for name in os.listdir(REPO_ROOT)
            if name.lower().endswith(".uproject")
        )
    except OSError as exc:
        print(
            f"[scope_guard] 无法枚举项目根目录 .uproject: {exc}",
            file=sys.stderr,
        )
        return []


def _project_name() -> str:
    """项目名 = 唯一 .uproject 文件名；零个或多个时拒绝猜测。"""
    projects = _uproject_files()
    return projects[0][: -len(".uproject")] if len(projects) == 1 else ""


def _project_diagnostic() -> dict | None:
    projects = _uproject_files()
    if not projects:
        return _diagnostic("error", "project.uproject_missing", "仓库根目录未找到 .uproject")
    if len(projects) > 1:
        return _diagnostic(
            "error", "project.uproject_ambiguous",
            f"仓库根目录存在多个 .uproject，无法确定项目: {projects}")
    return None


def _root_skill_path() -> str:
    """根 skill 路径：.workbuddy/skills/<项目名>/SKILL.md（约定位置）。"""
    name = _project_name()
    return os.path.join(SKILLS_DIR, name, "SKILL.md") if name else ""


# ── YAML 加载；解析失败后的阻断策略由具体宿主调用方决定 ────────────────
def _load_yaml_text(text: str):
    try:
        import yaml  # type: ignore
        return yaml.safe_load(text)
    except ModuleNotFoundError:
        if not getattr(_load_yaml_text, "_yaml_missing_reported", False):
            print(
                "[scope_guard] 缺少 PyYAML 依赖，无法解析 frontmatter；"
                "请安装后重试: python -m pip install pyyaml",
                file=sys.stderr,
            )
            _load_yaml_text._yaml_missing_reported = True
        return None
    except Exception as exc:
        print(
            f"[scope_guard] YAML parse error: {exc}",
            file=sys.stderr,
        )
        return None


def _parse_frontmatter(text: str) -> dict:
    """极简 frontmatter 解析（--- 之间的 YAML）。失败返回 {}。"""
    if not text.startswith("---"):
        return {}
    end = text.find("\n---", 3)
    if end == -1:
        return {}
    parsed = _load_yaml_text(text[3:end].strip("\n"))
    return parsed if isinstance(parsed, dict) else {}


def _state_schema_errors(state: dict) -> list[str]:
    """验证 state 的必需运行指针；具体 stage 名与 skill/spec 身份由后续协议验证。"""
    errors: list[str] = []
    required = ("stage", "active_spec", "active_skills")
    missing = [field for field in required if field not in state]
    if missing:
        errors.append(f"缺少必需字段: {missing}")

    if "stage" in state:
        stage = state["stage"]
        if not isinstance(stage, str) or not stage.strip():
            errors.append("stage 必须是非空字符串")
    if "active_spec" in state and not isinstance(state["active_spec"], str):
        errors.append("active_spec 必须是字符串")
    if "active_skills" in state and not isinstance(
            state["active_skills"], list):
        errors.append("active_skills 必须是列表")
    return errors


def _load_state(diagnostics: list[dict]) -> dict:
    """读取任务状态；缺失、不可读或格式错误都属于阻断性的激活协议错误。"""
    invalid = {"stage": None, "active_spec": None, "active_skills": None}
    try:
        with open(STATE_PATH, "r", encoding="utf-8") as f:
            state = json.load(f)
    except OSError as exc:
        diagnostics.append(_diagnostic(
            "error", "activation.state_unavailable",
            f"任务状态文件无法读取: {exc}", _relative_source_file(STATE_PATH)))
        return invalid
    except ValueError as exc:
        diagnostics.append(_diagnostic(
            "error", "activation.state_invalid",
            f"任务状态文件不是有效 JSON: {exc}", _relative_source_file(STATE_PATH)))
        return invalid
    if not isinstance(state, dict):
        diagnostics.append(_diagnostic(
            "error", "activation.state_invalid",
            "任务状态文件根节点必须是对象", _relative_source_file(STATE_PATH)))
        return invalid
    schema_errors = _state_schema_errors(state)
    if schema_errors:
        diagnostics.append(_diagnostic(
            "error", "activation.state_schema_invalid",
            "任务状态结构无效: " + "; ".join(schema_errors),
            _relative_source_file(STATE_PATH)))
    return state


def _load_root_skill() -> tuple[dict, str, str]:
    """返回 (frontmatter, loaded|missing|invalid, error)。缺失可继续，损坏须人工介入。"""
    path = _root_skill_path()
    if not path or not os.path.lexists(path):
        return {}, "missing", ""
    if not os.path.isfile(path):
        return {}, "invalid", "根 skill 路径存在但不是文件"
    try:
        with open(path, "r", encoding="utf-8") as f:
            frontmatter = _parse_frontmatter(f.read())
    except OSError as exc:
        return {}, "invalid", f"根 skill 读取失败: {exc}"
    if not frontmatter:
        return {}, "invalid", "根 skill frontmatter 缺失或无法解析"
    return frontmatter, "loaded", ""


def _root_skill_setup_info() -> dict:
    target = _root_skill_path()
    command = "python harness/scripts/init_root_skill.py"
    return {
        "recommended": True,
        "blocking": False,
        "target": (_relative_source_file(target) if target
                   else ".workbuddy/skills/<project-name>/SKILL.md"),
        "template": _relative_source_file(ROOT_SKILL_TEMPLATE_PATH),
        "command": command,
        "message": "项目尚未建立 root skill；请告知用户并建议初始化。初始化要求唯一 .uproject，未经确认不得自动创建或补造项目规则。",
    }


# ── glob 匹配（支持 ** 跨目录 与 * 段内） ──────────────────────────────
def _glob_match(pattern: str, path: str) -> bool:
    import re
    pattern = str(pattern).lower().replace("\\", "/")
    # 只去掉开头的 "./" 前缀（不去单独的 "."，否则误伤 .vs/.idea 这类点开头目录）
    if pattern.startswith("./"):
        pattern = pattern[2:]
    path = str(path).lower().replace("\\", "/")

    # `**/` 匹配零个或多个目录段；尾部 `/**` 同时匹配目录节点本身及全部后代。
    # 这样 `Intermediate/**` 能覆盖删除 `Intermediate` 目录，而不只覆盖其中的文件。
    trailing_tree = pattern.endswith("/**")
    if trailing_tree:
        pattern = pattern[:-3]

    rx_parts: list[str] = []
    i = 0
    while i < len(pattern):
        if pattern.startswith("**/", i):
            rx_parts.append("(?:.*/)?")
            i += 3
        elif pattern.startswith("**", i):
            rx_parts.append(".*")
            i += 2
        elif pattern[i] == "*":
            rx_parts.append("[^/]*")
            i += 1
        elif pattern[i] == "?":
            rx_parts.append("[^/]")
            i += 1
        else:
            rx_parts.append(re.escape(pattern[i]))
            i += 1
    if trailing_tree:
        rx_parts.append("(?:/.*)?")
    return re.fullmatch("".join(rx_parts), path) is not None


def _norm(path: str, base_dir: str = "") -> str:
    base = base_dir or REPO_ROOT
    p = (os.path.abspath(path) if os.path.isabs(path)
         else os.path.abspath(os.path.join(base, path)))
    try:
        rel = os.path.relpath(p, REPO_ROOT)
    except ValueError:
        rel = p
    return rel.replace("\\", "/")


def _evaluation_seconds_remaining(ctx: dict) -> float | None:
    deadline = ctx.get("_deadline_monotonic")
    if deadline is None:
        return None
    return float(deadline) - time.monotonic()


def _require_evaluation_budget(ctx: dict) -> None:
    remaining = _evaluation_seconds_remaining(ctx)
    if remaining is not None and remaining <= 0:
        raise RuntimeError("治理求值总预算已耗尽")


# ════════════════════════════════════════════════════════════════════
#  封闭求值器函数库（每个 evaluator 一个独立函数；逻辑写死，不在数据里）
#  约定：返回 None=不命中(放行) / 非空字符串=命中(reason 已被外层覆盖则用默认)
# ════════════════════════════════════════════════════════════════════

def _eval_path_glob(data: dict, ctx: dict) -> str | None:
    """文件路径命中 glob → 命中。data: {pattern, invert?}"""
    path = ctx.get("path", "")
    pattern = data.get("pattern", "")
    invert = bool(data.get("invert", False))
    hit = _glob_match(pattern, path)
    if invert:
        hit = not hit
    return "命中" if hit else None


def _string_list(data: dict, field: str, *, allow_empty: bool) -> tuple[str, ...]:
    value = data.get(field)
    if not isinstance(value, list) or (not allow_empty and not value):
        requirement = "列表" if allow_empty else "非空列表"
        raise RuntimeError(f"{field} 必须是{requirement}")
    invalid = [
        index for index, item in enumerate(value)
        if not isinstance(item, str) or not item.strip()
    ]
    if invalid:
        raise RuntimeError(f"{field} 的第 {invalid[0] + 1} 项必须是非空字符串")
    return tuple(value)


def _command_write_config(
        data: dict) -> tuple[tuple[str, ...], tuple[str, ...], tuple[str, ...]]:
    """验证并读取 command_write 的自包含声明数据。"""
    asset_patterns = tuple(
        value.lower() for value in _string_list(
            data, "asset_patterns", allow_empty=False))
    write_hints = tuple(
        value.lower() for value in _string_list(
            data, "write_hints", allow_empty=False))
    read_hints = tuple(
        value.lower() for value in _string_list(
            data, "read_hints", allow_empty=True))
    overlap = sorted(set(write_hints) & set(read_hints))
    if overlap:
        raise RuntimeError(
            f"write_hints 与 read_hints 不得包含相同项: {overlap}")
    return asset_patterns, write_hints, read_hints


def _eval_command_write(data: dict, ctx: dict) -> str | None:
    """shell 命令命中声明的资产片段和写特征 → 命中。
    data: {asset_patterns, write_hints, read_hints}，全部由当前 constraint 自有。"""
    cmd = (ctx.get("command") or "").lower()
    if not cmd:
        return None
    asset_patterns, write_hints, read_hints = _command_write_config(data)
    if not any(pattern in cmd for pattern in asset_patterns):
        return None
    if any(hint in cmd for hint in write_hints):
        return "命中"
    if any(hint in cmd for hint in read_hints):
        return None
    return None


def _state_field_path(data: dict) -> str:
    """解析 state_field 声明的仓库内相对状态路径。"""
    relative_state_file = data.get("file")
    if (not isinstance(relative_state_file, str)
            or not relative_state_file.strip()):
        raise RuntimeError("state_field.data.file 必须是非空相对路径")
    normalized = relative_state_file.strip().replace("\\", "/")
    parts = normalized.split("/")
    if (os.path.isabs(relative_state_file)
            or os.path.splitdrive(relative_state_file)[0]
            or any(part in {"", ".", ".."} for part in parts)):
        raise RuntimeError(
            "state_field.data.file 必须是无盘符、不含空段/./.. 的仓库相对路径")

    repo_root = os.path.realpath(REPO_ROOT)
    state_path = os.path.realpath(os.path.join(repo_root, normalized))
    try:
        inside_repo = os.path.commonpath([
            os.path.normcase(state_path),
            os.path.normcase(repo_root),
        ]) == os.path.normcase(repo_root)
    except ValueError as exc:
        raise RuntimeError(
            f"state_field 状态路径无效: {relative_state_file}") from exc
    if not inside_repo:
        raise RuntimeError(
            f"state_field 状态路径必须位于项目内: {relative_state_file}")
    return state_path


def _eval_state_field(data: dict, ctx: dict) -> str | None:
    """某状态文件字段等于某值 → 命中。data: {file, field, equals, missing_equals?}
    只有文件不存在时使用 missing_equals；已存在但损坏属于 evaluator 故障。"""
    path = _state_field_path(data)
    field = data.get("field")
    if not isinstance(field, str) or not field.strip():
        raise RuntimeError("state_field.data.field 必须是非空字符串")
    field = field.strip()
    expect = data.get("equals")
    try:
        with open(path, "r", encoding="utf-8") as f:
            state = json.load(f)
    except FileNotFoundError:
        if "missing_equals" in data:
            return "命中" if data["missing_equals"] == expect else None
        return None  # 无状态文件，未指定 missing_equals → 不命中（放行）
    except (OSError, ValueError) as exc:
        raise RuntimeError(
            f"state_field 状态文件无法读取或解析: {data.get('file')}: {exc}"
        ) from exc
    if not isinstance(state, dict):
        raise RuntimeError(
            f"state_field 状态文件根节点必须是对象: {data.get('file')}")
    if field not in state:
        raise RuntimeError(
            f"state_field 状态文件缺少字段 {field}: {data.get('file')}")
    actual = state.get(field)
    return "命中" if actual == expect else None


def _eval_build_freshness(data: dict, ctx: dict) -> str | None:
    """上次成功编译必须对应当前 UE 编译输入内容。data: {file}"""
    state_path = _build_freshness_state_path(data)
    relative_state_file = _norm(state_path)

    try:
        with open(state_path, "r", encoding="utf-8") as handle:
            build_state = json.load(handle)
    except FileNotFoundError:
        return "从未建立成功编译状态"
    except (OSError, ValueError) as exc:
        return f"编译状态无法读取: {exc}"
    if not isinstance(build_state, dict):
        return "编译状态根节点不是对象"
    if build_state.get("success") is not True:
        return "上次编译未成功"

    recorded = build_state.get("source_fingerprint")
    if not isinstance(recorded, dict):
        return "上次成功编译缺少源码指纹，请重新编译"
    try:
        current = fingerprint_project_sources(
            REPO_ROOT,
            deadline_monotonic=ctx.get("_deadline_monotonic"))
    except SourceFingerprintError as exc:
        raise RuntimeError(
            f"无法计算当前 UE 编译输入指纹: {exc}") from exc
    if not fingerprint_matches(recorded, current):
        return (
            "当前 UE 编译输入已不同于上次成功编译"
            f"（state={relative_state_file}, "
            f"recorded={recorded.get('digest', '?')}, "
            f"current={current.get('digest', '?')}）")
    return None


def _build_freshness_state_path(data: dict) -> str:
    """解析 build_freshness 声明的项目内证明文件；不绑定具体项目路径。"""
    relative_state_file = str(data.get("file") or "")
    if not relative_state_file.strip():
        raise RuntimeError("build freshness state 路径为空")
    state_path = os.path.abspath(os.path.join(REPO_ROOT, relative_state_file))
    try:
        inside_repo = os.path.commonpath(
            [os.path.normcase(state_path), os.path.normcase(REPO_ROOT)]
        ) == os.path.normcase(REPO_ROOT)
    except ValueError as exc:
        raise RuntimeError(
            f"build freshness state 路径无效: {relative_state_file}") from exc
    if not inside_repo:
        raise RuntimeError(
            f"build freshness state 必须位于项目内: {relative_state_file}")
    return state_path


def _path_writable_stage_config(
        data: dict, current_stage: str | None = None
        ) -> tuple[dict, tuple[str, ...], tuple[str, ...]]:
    """验证并读取 path_writable_stage 的自包含声明数据。"""
    stages = data.get("stages")
    if not isinstance(stages, dict) or not stages:
        raise RuntimeError("path_writable_stage.stages 必须是非空对象")
    for stage_name, stage_cfg in stages.items():
        if not isinstance(stage_name, str) or not stage_name.strip():
            raise RuntimeError("path_writable_stage.stages 的阶段名必须是非空字符串")
        if not isinstance(stage_cfg, dict):
            raise RuntimeError(f"阶段 {stage_name} 的配置必须是对象")
        extra = sorted(set(stage_cfg) - {"writable", "reason"})
        if extra:
            raise RuntimeError(f"阶段 {stage_name} 包含未定义字段: {extra}")
        if not isinstance(stage_cfg.get("writable"), bool):
            raise RuntimeError(f"阶段 {stage_name}.writable 必须是布尔值")
        if "reason" in stage_cfg and not isinstance(stage_cfg["reason"], str):
            raise RuntimeError(f"阶段 {stage_name}.reason 必须是字符串")

    code = data.get("code")
    if not isinstance(code, dict):
        raise RuntimeError("path_writable_stage.code 必须是对象")
    extra_code = sorted(set(code) - {"exts", "areas"})
    if extra_code:
        raise RuntimeError(
            f"path_writable_stage.code 包含未定义字段: {extra_code}")
    exts = tuple(
        value.lower() for value in _string_list(
            code, "exts", allow_empty=False))
    invalid_exts = [value for value in exts if not value.startswith(".")]
    if invalid_exts:
        raise RuntimeError(
            f"path_writable_stage.code.exts 必须使用点开头的扩展名: {invalid_exts}")

    raw_areas = _string_list(code, "areas", allow_empty=False)
    areas: list[str] = []
    for area in raw_areas:
        normalized = area.replace("\\", "/")
        parts = normalized[:-1].split("/") if normalized.endswith("/") else []
        if (os.path.isabs(area)
                or os.path.splitdrive(area)[0]
                or not normalized.endswith("/")
                or any(part in {"", ".", ".."} for part in parts)):
            raise RuntimeError(
                "path_writable_stage.code.areas 必须是以 / 结尾、"
                f"无盘符且不含空段/./.. 的相对目录前缀: {area}")
        areas.append(normalized.lower())

    if current_stage is not None:
        if not isinstance(current_stage, str) or not current_stage.strip():
            raise RuntimeError("当前 stage 必须是非空字符串")
        if current_stage not in stages:
            raise RuntimeError(
                f"当前 stage 未在 constraint.data.stages 中声明: {current_stage}")
    return stages, exts, tuple(areas)


def _eval_path_writable_stage(data: dict, ctx: dict) -> str | None:
    """当前 stage 禁写时代码区文件 → 命中。
    data: {stages, code} 由当前 constraint 自有；当前 stage 只读运行时 state。"""
    path = str(ctx.get("path", "")).replace("\\", "/")
    scope = ctx.get("scope", {})
    stage = (scope.get("current") or {}).get("stage")
    stages, exts, areas = _path_writable_stage_config(data, stage)
    stage_cfg = stages[stage]
    if stage_cfg["writable"]:
        return None
    path_lower = path.lower()
    base = os.path.basename(path).lower()
    is_code = base.endswith(exts)
    in_area = path_lower.startswith(areas)
    if is_code and in_area:
        # 命中：返回阶段表的 reason 供外层提示（外层 reason 优先）
        return str(stage_cfg.get("reason") or "当前阶段禁写代码")
    return None


def _eval_require_tool_capability(data: dict, ctx: dict) -> str | None:
    """在适用的语义请求上要求工具 capability。

    data: {capability, path_pattern?, operation?}
    path_pattern / operation 只用于缩小适用范围；不声明时作用于该 constraint 的整个 when。
    """
    path_pattern = str(data.get("path_pattern") or "")
    if path_pattern and not _glob_match(path_pattern, str(ctx.get("path") or "")):
        return None
    operation = str(data.get("operation") or "")
    if operation and operation != str(ctx.get("operation") or ""):
        return None
    capability = str(data.get("capability") or "")
    capabilities = {
        str(item) for item in (ctx.get("tool_capabilities") or [])
    }
    if capability not in capabilities:
        return f"当前工具缺少所需 capability: {capability}"
    return None


def _require_active_skill_config(
        data: dict) -> tuple[str, tuple[str, ...], tuple[str, ...]]:
    """验证并读取 require_active_skill 的声明数据。"""
    skill = data.get("skill")
    if not isinstance(skill, str) or not skill.strip():
        raise RuntimeError("require_active_skill.skill 必须是非空字符串")
    path_patterns = _string_list(
        data, "path_patterns", allow_empty=False)
    exempt_patterns = (
        _string_list(data, "exempt_patterns", allow_empty=True)
        if "exempt_patterns" in data
        else ()
    )
    return skill, path_patterns, exempt_patterns


def _resolve_workflow_skill_reference(skill: str) -> tuple[str, str]:
    """把 selector/evaluator 引用解析为唯一、规范的 workflow host_id 与 SKILL.md。"""
    if not isinstance(skill, str) or not skill:
        raise RuntimeError("workflow skill 引用必须是非空字符串")
    if skill != skill.strip():
        raise RuntimeError("workflow skill 引用不能包含首尾空白")
    normalized = skill
    if (normalized in {".", ".."}
            or "/" in normalized
            or "\\" in normalized
            or os.path.splitdrive(normalized)[0]):
        raise RuntimeError(
            "workflow skill 引用必须是无盘符和路径分隔符的单个宿主名")

    root_name = _project_name()
    if root_name and normalized.casefold() == root_name.casefold():
        raise RuntimeError(
            "workflow skill selector 不能引用项目 root skill；root 存在时始终激活")

    try:
        discovered_names = sorted(
            name for name in os.listdir(SKILLS_DIR)
            if (not root_name
                or name.casefold() != root_name.casefold())
            and os.path.isfile(os.path.join(
                SKILLS_DIR, name, "SKILL.md"))
        )
    except OSError as exc:
        print(
            f"[scope_guard] 无法枚举 workflow skill 发现目录: {exc}",
            file=sys.stderr,
        )
        discovered_names = []

    if normalized in discovered_names:
        return normalized, os.path.join(
            SKILLS_DIR, normalized, "SKILL.md")

    case_matches = [
        name for name in discovered_names
        if name.casefold() == normalized.casefold()
    ]
    if case_matches:
        raise RuntimeError(
            "workflow skill 引用必须与发现到的宿主名大小写完全一致: "
            f"{normalized} != {case_matches}")
    raise FileNotFoundError(
        f"workflow skill 不存在或不可发现: {normalized}")


def _eval_require_active_skill(data: dict, ctx: dict) -> str | None:
    """适用路径要求任务已由外部 selector 激活指定 workflow skill。"""
    referenced_skill, path_patterns, exempt_patterns = (
        _require_active_skill_config(data))
    skill, _ = _resolve_workflow_skill_reference(referenced_skill)
    path = str(ctx.get("path") or "")
    if not any(_glob_match(pattern, path) for pattern in path_patterns):
        return None
    if any(_glob_match(pattern, path) for pattern in exempt_patterns):
        return None
    active_skill_set = ctx.get("_active_skill_set")
    if not isinstance(active_skill_set, set):
        raise RuntimeError("require_active_skill 缺少任务 selector 上下文")
    if skill not in active_skill_set:
        return f"当前任务未激活所需 workflow skill: {skill}"
    return None


def _owned_script_path(inst: dict, data: dict,
                       *, require_exists: bool) -> tuple[str, str]:
    """解析 skill 自有 Python 门禁脚本；禁止 spec、绝对路径、越界和外部 symlink。"""
    host_type = str(inst.get("_host_type") or "")
    if host_type not in {"root_skill", "workflow_skill"}:
        raise RuntimeError("script evaluator 只允许由 root/workflow skill 声明")

    relative_script = data.get("script")
    if not isinstance(relative_script, str) or not relative_script.strip():
        raise RuntimeError("script.data.script 必须是非空相对路径")
    normalized_script = relative_script.strip().replace("\\", "/")
    parts = normalized_script.split("/")
    if (os.path.isabs(relative_script)
            or os.path.splitdrive(relative_script)[0]
            or any(part in {"", ".", ".."} for part in parts)):
        raise RuntimeError("script 必须是 skill 目录内不含 . 或 .. 的相对路径")
    if not normalized_script.lower().endswith(".py"):
        raise RuntimeError("script evaluator 只执行 skill 自有的 .py 文件")

    source_file = str(
        inst.get("_canonical_source_file")
        or inst.get("_source_file")
        or "")
    if not source_file:
        raise RuntimeError("script evaluator 无法确定约束宿主文件")
    source_path = (
        os.path.abspath(source_file)
        if os.path.isabs(source_file)
        else os.path.abspath(os.path.join(REPO_ROOT, source_file))
    )
    owner_dir = os.path.dirname(os.path.realpath(source_path))
    script_path = os.path.realpath(os.path.join(
        owner_dir, *parts))
    try:
        inside_owner = os.path.commonpath([
            os.path.normcase(script_path),
            os.path.normcase(owner_dir),
        ]) == os.path.normcase(owner_dir)
    except ValueError:
        inside_owner = False
    if not inside_owner:
        raise RuntimeError("script 解析结果越出所属 skill 物理目录")
    if require_exists and not os.path.isfile(script_path):
        raise RuntimeError(
            f"skill 门禁脚本不存在或不是文件: {normalized_script}")
    return script_path, owner_dir


def _script_public_context(ctx: dict) -> dict:
    """只把稳定求值协议暴露给 skill 脚本，不泄露 Harness 内部字段。"""
    return {
        key: ctx.get(key)
        for key in (
            "path",
            "path_scope",
            "content",
            "command",
            "operation",
            "source_tool",
            "tool_capabilities",
            "tool_provider_ids",
            "confidence",
            "event",
            "data",
        )
    }


def _clip_script_output(value: str) -> str:
    text = str(value or "").strip()
    if len(text) <= SCRIPT_OUTPUT_LIMIT:
        return text
    return text[:SCRIPT_OUTPUT_LIMIT] + "…<truncated>"


def _eval_script(data: dict, ctx: dict) -> str | None:
    """执行当前 skill 自有的受信 Python 门禁脚本。0=未命中，1=命中，其他=故障。"""
    inst = ctx.get("_constraint_instance")
    if not isinstance(inst, dict):
        raise RuntimeError("script evaluator 缺少当前约束宿主上下文")
    script_path, owner_dir = _owned_script_path(
        inst, data, require_exists=True)

    timeout = data.get("timeout", SCRIPT_TIMEOUT_DEFAULT_SECONDS)
    if (not isinstance(timeout, int)
            or isinstance(timeout, bool)
            or not 1 <= timeout <= SCRIPT_TIMEOUT_MAX_SECONDS):
        raise RuntimeError(
            f"script.timeout 必须是 1..{SCRIPT_TIMEOUT_MAX_SECONDS} 的整数秒")
    remaining = _evaluation_seconds_remaining(ctx)
    if remaining is not None:
        if remaining <= 0:
            raise RuntimeError("执行 skill 门禁脚本前治理求值总预算已耗尽")
        timeout = min(float(timeout), remaining)

    ctx["data"] = data  # 透传约束 data 给 skill 脚本（机制封闭、数据开放）

    payload = json.dumps(
        _script_public_context(ctx), ensure_ascii=False)
    try:
        completed = subprocess.run(
            [sys.executable, "-I", "-B", script_path],
            input=payload,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            cwd=owner_dir,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(
            f"skill 门禁脚本超时（{timeout:.3g}s）: "
            f"{data.get('script', '')}") from error
    except OSError as error:
        raise RuntimeError(
            f"skill 门禁脚本无法启动: {error}") from error

    stdout = _clip_script_output(completed.stdout)
    stderr = _clip_script_output(completed.stderr)
    if completed.returncode == 0:
        return None
    if completed.returncode == 1:
        if "Traceback" in stderr or "Traceback" in stdout:
            raise RuntimeError("skill 门禁脚本崩溃（traceback）")
        return stdout or "skill 门禁脚本命中"
    detail = "; ".join(
        value for value in (
            f"stdout={stdout}" if stdout else "",
            f"stderr={stderr}" if stderr else "",
        ) if value)
    suffix = f"; {detail}" if detail else ""
    raise RuntimeError(
        f"skill 门禁脚本协议失败，exit={completed.returncode}{suffix}")


# 求值器注册表（封闭枚举；新增类型 = 加函数 + 在此登记）
EVALUATORS = {
    "build_freshness": _eval_build_freshness,
    "path_glob": _eval_path_glob,
    "command_write": _eval_command_write,
    "state_field": _eval_state_field,
    "path_writable_stage": _eval_path_writable_stage,
    "require_tool_capability": _eval_require_tool_capability,
    "require_active_skill": _eval_require_active_skill,
    "script": _eval_script,
}

# 这些 evaluator 的结果只依赖治理状态，不依赖尚未解析出的工具副作用细节。
# unresolved event 在决定 blocker 前可以安全地对它们做一次精确求值。
UNRESOLVED_STATE_EVALUATORS = frozenset({
    "build_freshness",
    "state_field",
})


# ════════════════════════════════════════════════════════════════════
#  实例发现（discover）：收集当前存在的所有约束实例
#  来源：根 skill + workflow skills + specs（三类，随宿主生灭）
# ════════════════════════════════════════════════════════════════════

def _relative_source_file(path: str) -> str:
    return os.path.relpath(path, REPO_ROOT).replace("\\", "/")


def _decorate_constraint(constraint: dict, *, source: str, source_file: str,
                         host_type: str, host_id: str, ordinal: int) -> dict:
    """附加由宿主位置推导的来源与生命周期元数据，不改变实例声明。"""
    item = dict(constraint)
    declared_id = str(item.get("id") or "").strip()
    item["_constraint_id"] = declared_id or f"{source}#{ordinal}"
    item["_declared_id"] = declared_id
    item["_source"] = source
    discovery_source_file = _relative_source_file(source_file)
    canonical_source_file = _relative_source_file(os.path.realpath(source_file))
    item["_source_file"] = discovery_source_file
    item["_canonical_source_file"] = canonical_source_file
    item["_host_type"] = host_type
    item["_host_id"] = host_id
    item["_lifecycle"] = LIFECYCLES[host_type]
    return item


def _diagnostic(severity: str, code: str, message: str, source_file: str = "",
                *, host_type: str = "", host_id: str = "") -> dict:
    item = {"severity": severity, "code": code, "message": message}
    if source_file:
        item["source_file"] = source_file
    if host_type:
        item["host_type"] = host_type
        item["host_id"] = host_id
        lifecycle = LIFECYCLES.get(host_type)
        if lifecycle:
            item["lifecycle"] = lifecycle
    return item


def _constraint_items(frontmatter: dict, diagnostics: list[dict] | None,
                      source_file: str, *, host_type: str,
                      host_id: str) -> list[dict]:
    """读取 constraints 列表并报告容器/条目结构错误，避免静默忽略坏声明。"""
    raw = frontmatter.get("constraints", [])
    if raw is None:
        return []
    if not isinstance(raw, list):
        if diagnostics is not None:
            diagnostics.append(_diagnostic(
                "error", "constraint.list_invalid",
                "constraints 必须是列表", source_file,
                host_type=host_type, host_id=host_id))
        return []
    items: list[dict] = []
    for ordinal, item in enumerate(raw, start=1):
        if isinstance(item, dict):
            items.append(item)
        elif diagnostics is not None:
            diagnostics.append(_diagnostic(
                "error", "constraint.item_invalid",
                f"constraints 第 {ordinal} 项必须是对象", source_file,
                host_type=host_type, host_id=host_id))
    return items


def _validate_spec_frontmatter_schema(
        frontmatter: dict, diagnostics: list[dict] | None,
        source_file: str, *, spec_id: str) -> None:
    """验证 spec 的通用必需字段与单一真相源边界。"""
    if diagnostics is None:
        return
    for field, (code, message) in SPEC_FORBIDDEN_FRONTMATTER_FIELDS.items():
        if field in frontmatter:
            diagnostics.append(_diagnostic(
                "error", code, message, source_file,
                host_type="spec", host_id=spec_id))
    if "required_skills" not in frontmatter:
        diagnostics.append(_diagnostic(
            "error", "spec.required_skills_missing",
            "spec 必须显式声明 required_skills；没有 workflow 依赖时使用 []",
            source_file, host_type="spec", host_id=spec_id))
    elif not isinstance(frontmatter["required_skills"], list):
        diagnostics.append(_diagnostic(
            "error", "spec.required_skills_invalid",
            "spec required_skills 必须是列表",
            source_file, host_type="spec", host_id=spec_id))
    else:
        for ordinal, item in enumerate(
                frontmatter["required_skills"], start=1):
            if not isinstance(item, str):
                diagnostics.append(_diagnostic(
                    "error", "spec.required_skill_reference_invalid",
                    f"spec required_skills 第 {ordinal} 项必须是字符串 host_id",
                    source_file, host_type="spec", host_id=spec_id))


def _discover_spec_constraints(diagnostics: list[dict] | None = None) -> list[dict]:
    """扫描 specs/ 目录，收集所有 .md 的 frontmatter constraints。"""
    instances: list[dict] = []
    specs_dir = os.path.join(REPO_ROOT, "specs")
    if not os.path.isdir(specs_dir):
        return instances
    for name in sorted(os.listdir(specs_dir)):
        if not name.endswith(".md"):
            continue
        spec_path = os.path.join(specs_dir, name)
        spec_id = os.path.splitext(name)[0]
        source_file = _relative_source_file(spec_path)
        try:
            with open(spec_path, "r", encoding="utf-8") as f:
                fm = _parse_frontmatter(f.read())
        except OSError:
            if diagnostics is not None:
                diagnostics.append(_diagnostic("error", "source.read_failed",
                                               "无法读取 spec", source_file,
                                               host_type="spec", host_id=spec_id))
            continue
        if not fm:
            if diagnostics is not None:
                diagnostics.append(_diagnostic("error", "source.frontmatter_invalid",
                                               "spec frontmatter 缺失或无法解析",
                                               source_file,
                                               host_type="spec", host_id=spec_id))
            continue
        _validate_spec_frontmatter_schema(
            fm, diagnostics, source_file, spec_id=spec_id)
        for ordinal, c in enumerate(_constraint_items(
                fm, diagnostics, source_file,
                host_type="spec", host_id=spec_id), start=1):
            instances.append(_decorate_constraint(
                c, source=f"spec:{name}", source_file=spec_path,
                host_type="spec", host_id=spec_id, ordinal=ordinal))
    return instances


def discover_constraints(root_skill: dict, diagnostics: list[dict] | None = None) -> list[dict]:
    """收集约束实例：根 skill（项目级）+ 各 skill frontmatter（做法级）+ specs（需求级）。"""
    instances: list[dict] = []
    root_name = _project_name()

    # 根 skill 的项目级约束
    root_path = _root_skill_path()
    root_source_file = _relative_source_file(root_path) if root_path else ""
    for ordinal, c in enumerate(
            _constraint_items(
                root_skill, diagnostics, root_source_file,
                host_type="root_skill", host_id=root_name), start=1):
        instances.append(_decorate_constraint(
            c, source="root", source_file=root_path,
            host_type="root_skill", host_id=root_name, ordinal=ordinal))

    # 其他 skill 的做法级约束（排除根 skill 自身，避免重复）
    if os.path.isdir(SKILLS_DIR):
        for name in sorted(os.listdir(SKILLS_DIR)):
            if name == root_name:
                continue
            skill_md = os.path.join(SKILLS_DIR, name, "SKILL.md")
            if not os.path.isfile(skill_md):
                continue
            source_file = _relative_source_file(skill_md)
            try:
                with open(skill_md, "r", encoding="utf-8") as f:
                    fm = _parse_frontmatter(f.read())
            except OSError:
                if diagnostics is not None:
                    diagnostics.append(_diagnostic("error", "source.read_failed",
                                                   "无法读取 workflow skill",
                                                   source_file,
                                                   host_type="workflow_skill",
                                                   host_id=name))
                continue
            if not fm:
                if diagnostics is not None:
                    diagnostics.append(_diagnostic("error", "source.frontmatter_invalid",
                                                   "workflow skill frontmatter 缺失或无法解析",
                                                   source_file,
                                                   host_type="workflow_skill",
                                                   host_id=name))
                continue
            for ordinal, c in enumerate(_constraint_items(
                    fm, diagnostics, source_file,
                    host_type="workflow_skill", host_id=name), start=1):
                instances.append(_decorate_constraint(
                    c, source=f"skill:{name}", source_file=skill_md,
                    host_type="workflow_skill", host_id=name, ordinal=ordinal))

    # spec 级约束（爆炸半径）
    instances.extend(_discover_spec_constraints(diagnostics))

    return instances


def _decorate_tool_provider(provider: dict, *, source: str,
                            source_file: str, host_type: str,
                            host_id: str, ordinal: int) -> dict:
    item = dict(provider)
    provider_id = str(item.get("id") or "").strip()
    item["_provider_id"] = provider_id or f"{source}:provider#{ordinal}"
    item["_source"] = source
    item["_source_file"] = _relative_source_file(source_file)
    item["_canonical_source_file"] = _relative_source_file(
        os.path.realpath(source_file))
    item["_host_type"] = host_type
    item["_host_id"] = host_id
    item["_lifecycle"] = LIFECYCLES[host_type]
    return item


def _decorate_tool_requirement(capability: str, *, source: str,
                               source_file: str, host_type: str,
                               host_id: str, ordinal: int) -> dict:
    discovery_source_file = _relative_source_file(source_file)
    return {
        "capability": capability,
        "_requirement_id": f"{source}:requirement#{ordinal}:{capability}",
        "_source": source,
        "_source_file": discovery_source_file,
        "_canonical_source_file": _relative_source_file(
            os.path.realpath(source_file)),
        "_host_type": host_type,
        "_host_id": host_id,
        "_lifecycle": LIFECYCLES[host_type],
    }


def _tool_governance_hosts(root_skill: dict) -> list[dict]:
    """复用三类既有宿主；这里只读取有效 frontmatter，不重复报告来源读取错误。"""
    hosts: list[dict] = []
    root_name = _project_name()
    root_path = _root_skill_path()
    if root_path and root_skill:
        hosts.append({
            "frontmatter": root_skill,
            "source": "root",
            "source_file": root_path,
            "host_type": "root_skill",
            "host_id": root_name,
        })

    if os.path.isdir(SKILLS_DIR):
        for name in sorted(os.listdir(SKILLS_DIR)):
            if name == root_name:
                continue
            skill_md = os.path.join(SKILLS_DIR, name, "SKILL.md")
            if not os.path.isfile(skill_md):
                continue
            try:
                with open(skill_md, "r", encoding="utf-8") as file:
                    frontmatter = _parse_frontmatter(file.read())
            except OSError as exc:
                print(
                    f"[scope_guard] 无法读取 workflow skill frontmatter "
                    f"{skill_md}: {exc}",
                    file=sys.stderr,
                )
                continue
            if frontmatter:
                hosts.append({
                    "frontmatter": frontmatter,
                    "source": f"skill:{name}",
                    "source_file": skill_md,
                    "host_type": "workflow_skill",
                    "host_id": name,
                })

    specs_dir = os.path.join(REPO_ROOT, "specs")
    if os.path.isdir(specs_dir):
        for name in sorted(os.listdir(specs_dir)):
            if not name.endswith(".md"):
                continue
            spec_path = os.path.join(specs_dir, name)
            try:
                with open(spec_path, "r", encoding="utf-8") as file:
                    frontmatter = _parse_frontmatter(file.read())
            except OSError as exc:
                print(
                    f"[scope_guard] 无法读取 spec frontmatter "
                    f"{spec_path}: {exc}",
                    file=sys.stderr,
                )
                continue
            if frontmatter:
                hosts.append({
                    "frontmatter": frontmatter,
                    "source": f"spec:{name}",
                    "source_file": spec_path,
                    "host_type": "spec",
                    "host_id": os.path.splitext(name)[0],
                })
    return hosts


def discover_tool_governance(
        root_skill: dict, diagnostics: list[dict]) -> tuple[list[dict], list[dict]]:
    """发现随 root/workflow/spec 生灭的 provider 与 capability requirement。"""
    providers: list[dict] = []
    requirements: list[dict] = []
    for host in _tool_governance_hosts(root_skill):
        frontmatter = host["frontmatter"]
        source_file = _relative_source_file(host["source_file"])
        host_type = host["host_type"]
        host_id = host["host_id"]

        capabilities, requirement_errors = normalize_required_capabilities(
            frontmatter.get("required_tool_capabilities"))
        for error in requirement_errors:
            diagnostics.append(_diagnostic(
                "error", error["code"], error["message"], source_file,
                host_type=host_type, host_id=host_id))
        for ordinal, capability in enumerate(capabilities, start=1):
            requirements.append(_decorate_tool_requirement(
                capability, source=host["source"],
                source_file=host["source_file"], host_type=host_type,
                host_id=host_id, ordinal=ordinal))

        raw_providers = frontmatter.get("tool_providers", [])
        if raw_providers is None:
            raw_providers = []
        if not isinstance(raw_providers, list):
            diagnostics.append(_diagnostic(
                "error", "tool_provider.list_invalid",
                "tool_providers 必须是列表", source_file,
                host_type=host_type, host_id=host_id))
            continue
        for ordinal, provider in enumerate(raw_providers, start=1):
            for error in validate_tool_provider(provider):
                diagnostics.append(_diagnostic(
                    "error", error["code"],
                    f"tool_providers 第 {ordinal} 项: {error['message']}",
                    source_file, host_type=host_type, host_id=host_id))
            if isinstance(provider, dict):
                providers.append(_decorate_tool_provider(
                    provider, source=host["source"],
                    source_file=host["source_file"], host_type=host_type,
                    host_id=host_id, ordinal=ordinal))

    by_id: dict[str, list[dict]] = {}
    for provider in providers:
        by_id.setdefault(str(provider.get("_provider_id") or ""), []).append(
            provider)
    for provider_id, duplicates in by_id.items():
        if not provider_id or len(duplicates) < 2:
            continue
        sources = sorted({
            str(item.get("_source_file") or "") for item in duplicates})
        for item in duplicates:
            diagnostics.append(_diagnostic(
                "error", "tool_provider.id_duplicate",
                f"tool provider id 重复: {provider_id}; sources={sources}",
                str(item.get("_source_file") or ""),
                host_type=str(item.get("_host_type") or ""),
                host_id=str(item.get("_host_id") or "")))
    return providers, requirements


def validate_effective_tool_requirements(
        providers: list[dict], requirements: list[dict],
        task_context: dict) -> list[dict]:
    """requirement 必须能解析到至少一个当前有效 provider；不证明客户端已安装该工具。"""
    diagnostics: list[dict] = []
    effective_providers = [
        item for item in providers if _is_active(item, task_context)]
    for requirement in requirements:
        if not _is_active(requirement, task_context):
            continue
        capability = str(requirement.get("capability") or "")
        candidates = [
            item for item in effective_providers
            if capability in {
                str(value) for value in (item.get("capabilities") or [])
            }
        ]
        if candidates:
            continue
        diagnostics.append(_diagnostic(
            "error", "tool_requirement.provider_missing",
            f"当前任务要求 capability {capability}，但没有当前有效 tool provider 声明",
            str(requirement.get("_source_file") or ""),
            host_type=str(requirement.get("_host_type") or ""),
            host_id=str(requirement.get("_host_id") or "")))
    return diagnostics


def _normalize_skill_references(raw, diagnostics: list[dict], *, source: str,
                                source_file: str = "") -> list[str]:
    """把 spec/state 的字符串引用解析为规范 workflow host_id。"""
    if raw is None:
        diagnostics.append(_diagnostic(
            "error", "activation.skills_invalid",
            f"{source} 的 skill 引用必须显式声明为列表", source_file))
        return []
    if not isinstance(raw, list):
        diagnostics.append(_diagnostic(
            "error", "activation.skills_invalid",
            f"{source} 的 skill 引用必须是列表", source_file))
        return []
    names: list[str] = []
    for ordinal, item in enumerate(raw, start=1):
        if isinstance(item, str):
            name = item
        else:
            name = None
        if not isinstance(name, str) or not name.strip():
            diagnostics.append(_diagnostic(
                "error", "activation.skill_reference_invalid",
                f"{source} 的第 {ordinal} 个 skill 引用必须是字符串 host_id",
                source_file))
            continue
        try:
            canonical_name, _ = _resolve_workflow_skill_reference(name)
        except FileNotFoundError as error:
            diagnostics.append(_diagnostic(
                "error", "activation.skill_missing",
                f"{source} 的第 {ordinal} 个引用无效: {error}", source_file))
            continue
        except RuntimeError as error:
            diagnostics.append(_diagnostic(
                "error", "activation.skill_reference_invalid",
                f"{source} 的第 {ordinal} 个引用无效: {error}", source_file))
            continue
        if canonical_name not in names:
            names.append(canonical_name)
    return names


def _resolve_active_spec(active_spec, diagnostics: list[dict]) -> dict | None:
    """解析规范 active_spec 指针并读取 required_skills；空字符串表示无 active spec。"""
    if not isinstance(active_spec, str):
        diagnostics.append(_diagnostic(
            "error", "activation.active_spec_reference_invalid",
            "active_spec 必须是空字符串或 specs/<id>.md"))
        return None
    if not active_spec:
        return None
    if active_spec != active_spec.strip():
        diagnostics.append(_diagnostic(
            "error", "activation.active_spec_reference_invalid",
            "active_spec 不能包含首尾空白"))
        return None

    prefix = "specs/"
    if not active_spec.startswith(prefix):
        diagnostics.append(_diagnostic(
            "error", "activation.active_spec_reference_invalid",
            "active_spec 必须使用规范格式 specs/<id>.md"))
        return None

    name = active_spec[len(prefix):]
    spec_id = name[:-len(".md")] if name.endswith(".md") else ""
    if (not spec_id
            or spec_id in {".", ".."}
            or spec_id != spec_id.strip()
            or "/" in name
            or "\\" in name
            or os.path.splitdrive(name)[0]
            or not name.endswith(".md")):
        diagnostics.append(_diagnostic(
            "error", "activation.active_spec_reference_invalid",
            "active_spec 必须是无盘符、无路径分隔符、无首尾空白的 specs/<id>.md"))
        return None

    specs_dir = os.path.abspath(os.path.join(REPO_ROOT, "specs"))
    try:
        discovered_names = sorted(
            entry for entry in os.listdir(specs_dir)
            if entry.endswith(".md")
            and os.path.isfile(os.path.join(specs_dir, entry))
        )
    except OSError:
        discovered_names = []

    if name not in discovered_names:
        case_matches = [
            entry for entry in discovered_names
            if entry.casefold() == name.casefold()
        ]
        if case_matches:
            diagnostics.append(_diagnostic(
                "error", "activation.active_spec_reference_invalid",
                "active_spec 大小写必须与发现到的 spec 文件完全一致: "
                f"{name} != {case_matches}"))
            return None
        diagnostics.append(_diagnostic(
            "error", "activation.active_spec_missing",
            f"active_spec 无法解析: {active_spec}"))
        return None

    candidate = os.path.join(specs_dir, name)
    try:
        with open(candidate, "r", encoding="utf-8") as f:
            frontmatter = _parse_frontmatter(f.read())
    except OSError:
        frontmatter = {}
    source_file = _relative_source_file(candidate)
    if not frontmatter:
        diagnostics.append(_diagnostic(
            "error", "activation.active_spec_invalid",
            "active spec frontmatter 缺失或无法解析", source_file))
        return None
    status = frontmatter.get("status")
    if status != "confirmed":
        diagnostics.append(_diagnostic(
            "error", "activation.active_spec_status_invalid",
            "active spec 必须具有精确状态 status: confirmed；"
            f"当前值: {status!r}",
            source_file))
        return None
    raw_required_skills = frontmatter.get("required_skills")
    required_skills = (
        _normalize_skill_references(
            raw_required_skills, diagnostics,
            source="active spec required_skills", source_file=source_file)
        if (isinstance(raw_required_skills, list)
            and all(isinstance(item, str)
                    for item in raw_required_skills))
        else []
    )
    return {
        "id": spec_id,
        "status": status,
        "source_file": source_file,
        "required_skills": required_skills,
    }


def resolve_task_context(state: dict, diagnostics: list[dict]) -> dict:
    """从既有 state/spec 指针解析求值所需的最小任务上下文。"""
    if _state_schema_errors(state):
        return {
            "active_skills": [],
            "active_skill_set": set(),
            "selected_by": {},
            "active_spec": None,
        }

    active_spec_ref = state["active_spec"]
    active_spec = _resolve_active_spec(active_spec_ref, diagnostics)

    active_skills: list[str] = []
    selected_by: dict[str, list[str]] = {}
    if active_spec_ref != "":
        # active spec 是 spec 任务的唯一 skill 选择源；无效指针不得回退到旧 state 值。
        spec_skills = active_spec["required_skills"] if active_spec else []
        for name in spec_skills:
            if name not in active_skills:
                active_skills.append(name)
            selected_by.setdefault(name, []).append(
                f"spec:{active_spec['source_file']}")
    else:
        state_skills = _normalize_skill_references(
            state["active_skills"], diagnostics, source="state.active_skills",
            source_file=_relative_source_file(STATE_PATH))
        for name in state_skills:
            if name not in active_skills:
                active_skills.append(name)
            selected_by.setdefault(name, []).append("state.active_skills")

    return {
        "active_skills": active_skills,
        "active_skill_set": set(active_skills),
        "selected_by": selected_by,
        "active_spec": active_spec,
    }


def _preview_task_context(raw_skills: list[str],
                          diagnostics: list[dict]) -> dict:
    """按正式 selector 身份协议解析候选 workflow，但不读取或修改 selector。"""
    active_skills = _normalize_skill_references(
        raw_skills, diagnostics, source="preview_skills")
    return {
        "active_skills": active_skills,
        "active_skill_set": set(active_skills),
        "selected_by": {
            name: ["preview_skills"] for name in active_skills
        },
        "active_spec": None,
    }


def _diagnostic_host_is_active(item: dict, task_context: dict) -> bool:
    """诊断复用约束实例的同一激活判定，避免另建一套生命周期语义。"""
    host_type = str(item.get("host_type") or "")
    if not host_type:
        return False
    return _is_active({
        "_host_type": host_type,
        "_host_id": str(item.get("host_id") or ""),
        "_source_file": str(item.get("source_file") or ""),
    }, task_context)


def _annotate_diagnostic_activity(diagnostics: list[dict],
                                  task_context: dict) -> None:
    """给宿主诊断附加当前任务下的有效性，供 context/report 直接解释。"""
    for item in diagnostics:
        if item.get("host_type"):
            item["host_effective"] = _diagnostic_host_is_active(
                item, task_context)


def _task_context_errors(diagnostics: list[dict],
                         task_context: dict) -> list[dict]:
    """返回会阻断给定 effective 宿主集合的声明/激活错误。"""
    return [
        item for item in diagnostics
        if item.get("severity") == "error"
        and (str(item.get("code") or "").startswith("activation.")
             or item.get("code") == "root_skill.invalid"
             or _diagnostic_host_is_active(item, task_context))
    ]


def _task_context_repair_targets(
        errors: list[dict], instances: list[dict],
        task_context: dict) -> tuple[list[dict], list[dict]]:
    """从阻断诊断派生最小修复文件与只读检查根，不维护项目专用白名单。"""
    targets: list[dict] = []
    seen_targets: set[tuple[str, str]] = set()

    def add_target(path: str, role: str, diagnostic_code: str,
                   *, host_type: str = "", host_id: str = "") -> None:
        if not path:
            return
        absolute = (
            os.path.abspath(path)
            if os.path.isabs(path)
            else os.path.abspath(os.path.join(REPO_ROOT, path))
        )
        repo_key = os.path.normcase(os.path.realpath(REPO_ROOT))
        canonical_key = os.path.normcase(os.path.realpath(absolute))
        if not _path_contains(repo_key, canonical_key):
            return
        logical_key = os.path.normcase(os.path.normpath(absolute))
        identity = (logical_key, canonical_key)
        if identity in seen_targets:
            for item in targets:
                if item["_identity"] == identity:
                    if diagnostic_code not in item["diagnostic_codes"]:
                        item["diagnostic_codes"].append(diagnostic_code)
                    break
            return
        seen_targets.add(identity)
        targets.append({
            "path": _norm(absolute),
            "kind": "file",
            "role": role,
            "host_type": host_type,
            "host_id": host_id,
            "diagnostic_codes": [diagnostic_code],
            "_identity": identity,
        })

    for error in errors:
        code = str(error.get("code") or "")
        source_file = str(error.get("source_file") or "")
        host_type = str(error.get("host_type") or "")
        host_id = str(error.get("host_id") or "")
        if source_file:
            add_target(
                source_file, "声明该阻断诊断的治理源", code,
                host_type=host_type, host_id=host_id)
        elif code == "root_skill.invalid":
            add_target(
                _root_skill_path(), "损坏的项目 root skill", code,
                host_type="root_skill", host_id=_project_name())
        elif code.startswith("activation."):
            add_target(
                STATE_PATH, "任务 selector 与运行状态", code)

        for conflict in error.get("conflicts") or []:
            if not isinstance(conflict, dict):
                continue
            add_target(
                str(conflict.get("source_file") or ""),
                "effective 声明冲突来源", code,
                host_type=str(conflict.get("host_type") or ""),
                host_id=str(conflict.get("host_id") or ""))

    invalid_script_owners = {
        (
            str(item.get("source_file") or ""),
            str(item.get("host_type") or ""),
            str(item.get("host_id") or ""),
        )
        for item in errors
        if item.get("code") == "constraint.script_owner_invalid"
    }
    for inst in instances:
        owner = (
            str(inst.get("_source_file") or ""),
            str(inst.get("_host_type") or ""),
            str(inst.get("_host_id") or ""),
        )
        if owner not in invalid_script_owners:
            continue
        if not _is_active(inst, task_context):
            continue
        if inst.get("evaluator") != "script":
            continue
        try:
            script_path, _ = _owned_script_path(
                inst, inst.get("data") or {}, require_exists=False)
        except RuntimeError as exc:
            print(
                f"[scope_guard] 无法解析 skill-owned 门禁脚本路径: {exc}",
                file=sys.stderr,
            )
            continue
        if os.path.isfile(script_path):
            continue
        add_target(
            script_path, "缺失或非文件的 skill-owned 门禁脚本",
            "constraint.script_owner_invalid",
            host_type=str(inst.get("_host_type") or ""),
            host_id=str(inst.get("_host_id") or ""))

    public_targets = [{
        key: value for key, value in item.items()
        if not key.startswith("_") and value is not None and value != ""
    } for item in targets]

    inspection_roots: list[dict] = []
    seen_roots: set[tuple[str, str]] = set()

    def add_inspection(path: str, kind: str, role: str) -> None:
        if not path:
            return
        absolute = (
            os.path.abspath(path)
            if os.path.isabs(path)
            else os.path.abspath(os.path.join(REPO_ROOT, path))
        )
        repo_key = os.path.normcase(os.path.realpath(REPO_ROOT))
        canonical_key = os.path.normcase(os.path.realpath(absolute))
        if not _path_contains(repo_key, canonical_key):
            return
        identity = (
            os.path.normcase(os.path.normpath(absolute)),
            canonical_key,
        )
        if identity in seen_roots:
            return
        seen_roots.add(identity)
        inspection_roots.append({
            "path": _norm(absolute),
            "kind": kind,
            "role": role,
        })

    for item in targets:
        absolute = os.path.abspath(os.path.join(REPO_ROOT, item["path"]))
        add_inspection(
            os.path.dirname(absolute), "tree",
            "损坏治理宿主的受限只读检查范围")

    for path, role in (
            ("AGENTS.md", "项目治理认知入口"),
            ("harness/README.md", "Harness 正式机制说明"),
            ("harness/decisions.md", "Harness 机制决策记录")):
        if os.path.isfile(os.path.join(REPO_ROOT, path)):
            add_inspection(path, "file", role)

    for skill_name in task_context.get("active_skills") or []:
        if not isinstance(skill_name, str):
            continue
        skill_dir = os.path.join(SKILLS_DIR, skill_name)
        if os.path.isdir(skill_dir):
            add_inspection(
                skill_dir, "tree",
                f"当前 selector 已选择的 workflow skill: {skill_name}")
    return public_targets, inspection_roots


def _task_context_failure_report(state: dict, diagnostics: list[dict],
                                 task_context: dict,
                                 instances: list[dict] | None = None) -> dict | None:
    """任务依赖、root 完整性或当前有效宿主声明错误必须交由人工处理。"""
    errors = _task_context_errors(diagnostics, task_context)
    if not errors:
        return None
    repair_targets, inspection_roots = _task_context_repair_targets(
        errors, instances or [], task_context)
    root_errors = [
        item for item in errors
        if (item.get("code") == "root_skill.invalid"
            or item.get("host_type") == "root_skill")
    ]
    activation_errors = [
        item for item in errors
        if str(item.get("code") or "").startswith("activation.")
    ]
    if root_errors:
        human_action = (
            "请人工修复报告中的 root skill 及其约束声明后，重新启动任务并运行 "
            "scope_guard.py --context。")
    elif activation_errors:
        human_action = (
            "请人工修复 harness state、task selector 或当前 effective 宿主的冲突声明后，重新启动任务并运行 "
            "scope_guard.py --context。")
    else:
        human_action = (
            "请人工修复报告中的当前有效 workflow skill 或 active spec 约束声明后，重新启动任务并运行 "
            "scope_guard.py --context。")
    recovery_available = bool(repair_targets)
    return {
        "status": "human_action_required",
        "code": "task_context.invalid",
        "message": (
            "治理上下文或当前有效约束源校验失败，正常 AI 任务必须立即结束；"
            "只有用户明确发起治理修复后，才能使用本报告给出的受限恢复通道。"),
        "state_file": _relative_source_file(STATE_PATH),
        "stage": state.get("stage"),
        "active_spec": state.get("active_spec"),
        "active_skills": state.get("active_skills"),
        "diagnostics": errors,
        "human_action": human_action,
        "recovery": {
            "protocol": "task-context-repair/v1",
            "status": (
                "restricted_human_confirmation_available"
                if recovery_available
                else "out_of_band_human_action_required"),
            "normal_task": "abort_task",
            "repair_targets": repair_targets,
            "inspection_roots": inspection_roots,
            "inspection": (
                "WorkBuddy Read 仅可读取 inspection_roots；"
                "scope_guard.py 受信只读自检可运行。"),
            "mutation": (
                "仅限已验证结构化 Write/Edit 精确写入单个 repair_target；"
                "每次返回 permissionDecision:ask，由用户逐次确认。"),
            "other_calls": "abort_task",
            "boundary": (
                "hook JSON 损坏、scope_guard 无法启动或没有可安全推导的目标时，"
                "必须在 hook 外由人工修复。"),
        },
    }


def _task_context_failure_result(report: dict) -> dict:
    lines = [
        "[HARNESS_TASK_ABORT][HUMAN_ACTION_REQUIRED] 治理上下文解析失败，当前 AI 任务已终止。",
        f"state_file: {report.get('state_file', '')}",
        f"stage: {report.get('stage', '')}",
        f"active_spec: {report.get('active_spec', '')}",
        f"active_skills: {json.dumps(report.get('active_skills', []), ensure_ascii=False)}",
    ]
    for item in report.get("diagnostics", []):
        source = f" ({item.get('source_file')})" if item.get("source_file") else ""
        host = (
            f" [{item.get('host_type')}:{item.get('host_id', '')}]"
            if item.get("host_type") else ""
        )
        lines.append(
            f"- [{item.get('code', 'activation.error')}]{host} "
            f"{item.get('message', '')}{source}")
    recovery = report.get("recovery") or {}
    lines.append(
        "recovery_status: "
        f"{recovery.get('status', 'out_of_band_human_action_required')}")
    for item in recovery.get("repair_targets") or []:
        lines.append(
            f"- repair_target={item.get('path', '')} "
            f"role={item.get('role', '')}")
    lines.append(str(report.get("human_action") or "请交由人工处理。"))
    return {
        "decision": "deny",
        "reason": "\n".join(lines),
        "abort_task": True,
        "error_type": "task_context_invalid",
        "task_context_report": report,
    }


def _task_context_recovery_abort_result(report: dict, detail: str) -> dict:
    """无效上下文中，非恢复调用继续终止任务而不是退化为普通 deny。"""
    result = _task_context_failure_result(report)
    result["reason"] += (
        "\n[HARNESS_RECOVERY_SCOPE_DENIED] "
        f"{detail}\n"
        "正常任务仍处于终止状态；请仅使用 task_context_report.recovery "
        "声明的精确恢复入口。")
    return result


def _recovery_path_matches(path: str, targets: list[dict],
                           *, allow_descendant: bool) -> dict | None:
    if not path:
        return None
    request_key = _canonical_file_key(path)
    repo_key = _canonical_file_key(REPO_ROOT)
    if not _path_contains(repo_key, request_key):
        return None
    for item in targets:
        target_path = str(item.get("path") or "")
        if not target_path:
            continue
        target_key = _canonical_file_key(target_path)
        if request_key == target_key:
            return item
        if allow_descendant and _path_contains(
                target_key, request_key):
            return item
    return None


def _task_context_recovery_approval_result(
        request: dict, target: dict, report: dict) -> dict:
    request_ctx = request.get("ctx") or {}
    recovery_report = {
        "status": "human_confirmation_required",
        "code": "task_context.repair_requires_approval",
        "protocol": "task-context-repair/v1",
        "message": (
            "当前正常任务已终止；该调用精确命中动态修复目标，"
            "必须由用户确认本次治理修复。"),
        "target": target,
        "requested_path": str(request_ctx.get("path") or ""),
        "operation": str(request_ctx.get("operation") or "write"),
        "source_tool": str(request_ctx.get("source_tool") or ""),
        "task_context_code": report.get("code", ""),
        "abort_task": False,
        "recovery": (
            "批准只作用于当前精确 Write/Edit；不会恢复正常任务、"
            "不会授权其他路径，也不会建立持久维护模式。"
            "修改后必须重新运行 scope_guard.py --context。"),
    }
    return {
        "decision": "ask",
        "outcome": "human_confirmation_required",
        "reason": (
            "[HARNESS_TASK_CONTEXT_REPAIR_APPROVAL_REQUIRED] "
            "当前调用只用于修复已确认损坏的治理源，需要人工逐次确认。\n"
            f"- target={json.dumps(target, ensure_ascii=False)}\n"
            f"- requested_path={recovery_report['requested_path']}\n"
            f"- operation={recovery_report['operation']}\n"
            f"{recovery_report['recovery']}"
        ),
        "abort_task": False,
        "error_type": "task_context_repair_approval_required",
        "recovery_report": recovery_report,
    }


def _task_context_recovery_result(
        hook_ctx: dict, report: dict) -> dict:
    """任务上下文损坏时，仅开放可证明的检查与精确人工确认修复。"""
    recovery = report.get("recovery") or {}
    repair_targets = recovery.get("repair_targets") or []
    inspection_roots = recovery.get("inspection_roots") or []
    if recovery.get("status") != "restricted_human_confirmation_available":
        return _task_context_recovery_abort_result(
            report, "当前诊断没有可在 hook 内安全推导的修复目标。")

    tool_name = str(hook_ctx.get("tool_name") or "")
    if tool_name in WORKBUDDY_RECOVERY_READ_FIELDS:
        tool_input = hook_ctx.get("tool_input")
        try:
            path = _workbuddy_alias_value(
                tool_input if isinstance(tool_input, dict) else {},
                WORKBUDDY_RECOVERY_READ_FIELDS[tool_name],
                "recovery_read_target", allow_empty=False)
        except _HookProtocolError as error:
            return _task_context_recovery_abort_result(
                report, error.message)
        cwd = hook_ctx.get("cwd")
        if not os.path.isabs(path):
            if not isinstance(cwd, str):
                return _task_context_recovery_abort_result(
                    report, "恢复态相对 Read 路径要求顶层绝对 cwd。")
            path = _norm(path, cwd)
        else:
            path = _norm(path)
        root = _recovery_path_matches(
            path, inspection_roots, allow_descendant=True)
        if not root:
            return _task_context_recovery_abort_result(
                report, f"Read 目标不在受限检查范围内: {path}")
        return {
            "decision": "allow",
            "outcome": "recovery_inspection",
            "reason": (
                "[HARNESS_RECOVERY_INSPECTION] 正常任务仍已终止；"
                f"仅允许读取治理修复范围: {path}"),
            "warnings": [
                "当前处于受限治理恢复态；该 Read 不代表正常任务已恢复。"
            ],
        }

    try:
        analysis = normalize_tool_call(
            hook_ctx, REPO_ROOT, provider=None)
    except ToolAdapterError as error:
        return _task_context_recovery_abort_result(
            report, f"恢复态工具无法可信归一化: {error.message}")

    if (hook_ctx.get("tool_kind") == "command"
            and not analysis.get("unresolved_events")
            and any(
                str(item) == "trusted_harness_wrapper:scope_guard.py"
                for item in analysis.get("evidence") or [])):
        return {
            "decision": "allow",
            "outcome": "recovery_context_check",
            "reason": (
                "[HARNESS_RECOVERY_CONTEXT_CHECK] 正常任务仍已终止；"
                "允许运行 scope_guard.py 只读自检。"),
            "warnings": [
                "当前处于受限治理恢复态；只有 context 恢复有效后才能继续正常任务。"
            ],
        }

    if hook_ctx.get("tool_kind") not in {"write", "edit"}:
        return _task_context_recovery_abort_result(
            report, "恢复态只接受已观察 Read、scope_guard 自检或结构化 Write/Edit。")
    if analysis.get("unresolved_events"):
        return _task_context_recovery_abort_result(
            report, "恢复写入仍含无法证明的副作用。")
    write_requests = [
        item for item in analysis.get("evaluation_requests") or []
        if item.get("event") == "pre_write"
    ]
    if len(write_requests) != 1:
        return _task_context_recovery_abort_result(
            report, "恢复写入必须精确归一化为单个 pre_write 目标。")
    request = write_requests[0]
    request_ctx = request.get("ctx") or {}
    if request_ctx.get("operation") != "write":
        return _task_context_recovery_abort_result(
            report, "恢复态不允许删除、移动、复制或范围不明的修改。")
    target = _recovery_path_matches(
        str(request_ctx.get("path") or ""),
        repair_targets, allow_descendant=False)
    if not target:
        return _task_context_recovery_abort_result(
            report, "结构化写入没有精确命中任何 repair_target。")
    return _task_context_recovery_approval_result(
        request, target, report)


def _is_active(inst: dict, task_context: dict) -> bool:
    """唯一激活判定：已存在的 root 常驻，workflow 随任务，spec 随 active spec。"""
    host_type = inst.get("_host_type")
    if host_type == "root_skill":
        return True
    if host_type == "workflow_skill":
        return inst.get("_host_id") in (task_context.get("active_skill_set") or set())
    active_spec = task_context.get("active_spec") or {}
    active_spec_file = str(active_spec.get("source_file") or "")
    if host_type == "spec":
        return bool(active_spec_file and inst.get("_source_file") == active_spec_file)
    return False


def _canonical_file_key(path: str) -> str:
    """用于证明文件身份比较；解析现有 junction/symlink 并遵循平台大小写语义。"""
    absolute = (
        os.path.abspath(path)
        if os.path.isabs(path)
        else os.path.abspath(os.path.join(REPO_ROOT, path))
    )
    return os.path.normcase(os.path.realpath(absolute))


def _effective_build_proof_files(
        instances: list[dict], task_context: dict
        ) -> tuple[dict[str, dict], dict | None]:
    """由当前有效的硬 build_freshness 声明派生受保护证明文件。"""
    protected: dict[str, dict] = {}
    for inst in instances:
        if not _is_active(inst, task_context):
            continue
        if inst.get("evaluator") != "build_freshness":
            continue
        if inst.get("action", "deny") != "deny":
            continue
        try:
            state_path = _build_freshness_state_path(inst.get("data") or {})
            canonical_key = _canonical_file_key(state_path)
        except Exception as error:
            return {}, _evaluator_failure_result(
                inst, "proof_state_protection", error)
        item = protected.setdefault(canonical_key, {
            "path": _norm(state_path),
            "constraint_ids": [],
            "sources": [],
        })
        constraint_id = str(inst.get("_constraint_id") or "")
        if constraint_id and constraint_id not in item["constraint_ids"]:
            item["constraint_ids"].append(constraint_id)
        source_file = str(inst.get("_source_file") or "")
        if source_file and source_file not in item["sources"]:
            item["sources"].append(source_file)
    return protected, None


def validate_constraints(
        instances: list[dict], scope: dict | None = None) -> list[dict]:
    """验证声明 schema 与稳定 ID；阻断与否由诊断宿主的当前生命周期决定。"""
    diagnostics: list[dict] = []
    seen_ids: set[tuple[str, str, str, str]] = set()

    for inst in instances:
        source_file = str(inst.get("_source_file") or "")
        host_type = str(inst.get("_host_type") or "")
        host_id = str(inst.get("_host_id") or "")
        constraint_id = str(inst.get("_constraint_id") or "")
        declared_id = str(inst.get("_declared_id") or "")
        evaluator = inst.get("evaluator")
        when = inst.get("when")
        action = inst.get("action", "deny")
        data = inst.get("data")

        owner_key = (host_type, host_id, source_file)
        if not declared_id:
            diagnostics.append(_diagnostic(
                "warning", "constraint.id_missing",
                f"约束 {constraint_id} 缺少稳定 id", source_file,
                host_type=host_type, host_id=host_id))
        elif (*owner_key, declared_id) in seen_ids:
            diagnostics.append(_diagnostic(
                "error", "constraint.id_duplicate",
                f"约束 id {declared_id} 在同一宿主内重复", source_file,
                host_type=host_type, host_id=host_id))
        else:
            seen_ids.add((*owner_key, declared_id))

        if evaluator not in EVALUATORS:
            diagnostics.append(_diagnostic(
                "error", "constraint.evaluator_unknown",
                f"未知 evaluator: {evaluator}", source_file,
                host_type=host_type, host_id=host_id))
            continue

        events = when if isinstance(when, list) else [when]
        invalid_events = [event for event in events if event not in WHEN_ENUM]
        if invalid_events:
            diagnostics.append(_diagnostic(
                "error", "constraint.when_invalid",
                f"非法 when: {invalid_events}", source_file,
                host_type=host_type, host_id=host_id))

        if action not in ACTION_ENUM:
            diagnostics.append(_diagnostic(
                "error", "constraint.action_invalid",
                f"非法 action: {action}", source_file,
                host_type=host_type, host_id=host_id))

        if not isinstance(data, dict):
            diagnostics.append(_diagnostic(
                "error", "constraint.data_invalid",
                "data 必须是对象", source_file,
                host_type=host_type, host_id=host_id))
            continue

        required, allowed = EVALUATOR_DATA_SCHEMA[evaluator]
        missing = sorted(required - set(data))
        extra = sorted(set(data) - allowed) if allowed is not None else []
        if missing:
            diagnostics.append(_diagnostic(
                "error", "constraint.data_missing",
                f"{evaluator} 缺少 data 字段: {missing}", source_file,
                host_type=host_type, host_id=host_id))
        if extra:
            diagnostics.append(_diagnostic(
                "error", "constraint.data_extra",
                f"{evaluator} 包含未定义 data 字段: {extra}", source_file,
                host_type=host_type, host_id=host_id))

        if evaluator == "script" and isinstance(data, dict):
            timeout = data.get(
                "timeout", SCRIPT_TIMEOUT_DEFAULT_SECONDS)
            if (not isinstance(timeout, int)
                    or isinstance(timeout, bool)
                    or not 1 <= timeout <= SCRIPT_TIMEOUT_MAX_SECONDS):
                diagnostics.append(_diagnostic(
                    "error", "constraint.script_timeout_invalid",
                    f"script.timeout 必须是 1..{SCRIPT_TIMEOUT_MAX_SECONDS} 的整数秒",
                    source_file,
                    host_type=host_type, host_id=host_id))
            try:
                _owned_script_path(inst, data, require_exists=True)
            except RuntimeError as error:
                diagnostics.append(_diagnostic(
                    "error", "constraint.script_owner_invalid",
                    str(error), source_file,
                    host_type=host_type, host_id=host_id))
        if evaluator == "build_freshness" and isinstance(data, dict):
            state_file = data.get("file")
            if not isinstance(state_file, str) or not state_file.strip():
                diagnostics.append(_diagnostic(
                    "error", "constraint.build_state_file_invalid",
                    "build_freshness.data.file 必须是非空字符串",
                    source_file,
                    host_type=host_type, host_id=host_id))
            else:
                try:
                    _build_freshness_state_path(data)
                except RuntimeError as error:
                    diagnostics.append(_diagnostic(
                        "error", "constraint.build_state_file_invalid",
                        str(error), source_file,
                        host_type=host_type, host_id=host_id))
        if evaluator == "state_field" and isinstance(data, dict):
            state_field = data.get("field")
            if not isinstance(state_field, str) or not state_field.strip():
                diagnostics.append(_diagnostic(
                    "error", "constraint.state_field_name_invalid",
                    "state_field.data.field 必须是非空字符串",
                    source_file,
                    host_type=host_type, host_id=host_id))
            try:
                _state_field_path(data)
            except RuntimeError as error:
                diagnostics.append(_diagnostic(
                    "error", "constraint.state_field_path_invalid",
                    str(error), source_file,
                    host_type=host_type, host_id=host_id))
        if evaluator == "command_write" and isinstance(data, dict):
            try:
                _command_write_config(data)
            except RuntimeError as error:
                diagnostics.append(_diagnostic(
                    "error", "constraint.command_write_data_invalid",
                    str(error), source_file,
                    host_type=host_type, host_id=host_id))
        if evaluator == "path_writable_stage" and isinstance(data, dict):
            current_stage = None
            if scope is not None:
                current_stage = (scope.get("current") or {}).get("stage")
            try:
                _path_writable_stage_config(data, current_stage)
            except RuntimeError as error:
                diagnostics.append(_diagnostic(
                    "error", "constraint.stage_data_invalid",
                    str(error), source_file,
                    host_type=host_type, host_id=host_id))
        if evaluator == "require_active_skill" and isinstance(data, dict):
            try:
                required_skill, _, _ = _require_active_skill_config(data)
            except RuntimeError as error:
                diagnostics.append(_diagnostic(
                    "error", "constraint.active_skill_data_invalid",
                    str(error), source_file,
                    host_type=host_type, host_id=host_id))
            else:
                try:
                    _resolve_workflow_skill_reference(required_skill)
                except FileNotFoundError as error:
                    diagnostics.append(_diagnostic(
                        "error", "constraint.active_skill_missing",
                        str(error), source_file,
                        host_type=host_type, host_id=host_id))
                except RuntimeError as error:
                    diagnostics.append(_diagnostic(
                        "error", "constraint.active_skill_reference_invalid",
                        str(error), source_file,
                        host_type=host_type, host_id=host_id))

    return diagnostics


def validate_effective_constraint_ids(
        instances: list[dict], task_context: dict) -> list[dict]:
    """同一 constraint id 只能在当前 effective 集合中由一个宿主声明。"""
    by_id: dict[str, dict[tuple[str, str, str], dict]] = {}
    for inst in instances:
        if not _is_active(inst, task_context):
            continue
        declared_id = str(inst.get("_declared_id") or "")
        if not declared_id:
            continue
        source_file = str(inst.get("_source_file") or "")
        host_type = str(inst.get("_host_type") or "")
        host_id = str(inst.get("_host_id") or "")
        owner_key = (host_type, host_id, source_file)
        by_id.setdefault(declared_id, {}).setdefault(owner_key, {
            "host_type": host_type,
            "host_id": host_id,
            "source_file": source_file,
        })

    diagnostics: list[dict] = []
    for constraint_id, owners_by_key in by_id.items():
        owners = sorted(
            owners_by_key.values(),
            key=lambda item: (
                item["host_type"], item["host_id"], item["source_file"]),
        )
        if len(owners) < 2:
            continue
        sources = [item["source_file"] for item in owners]
        diagnostic = _diagnostic(
            "error", "activation.effective_constraint_id_duplicate",
            "当前 effective 约束 id 重复: "
            f"{constraint_id}; sources={sources}",
            sources[0] if sources else "")
        diagnostic["constraint_id"] = constraint_id
        diagnostic["conflicts"] = owners
        diagnostics.append(diagnostic)
    return diagnostics


# ════════════════════════════════════════════════════════════════════
#  求值（eval）：按时机逐条求值，命中 deny 即拦
# ════════════════════════════════════════════════════════════════════

def _evaluator_failure_result(inst: dict, event: str,
                              error: Exception) -> dict:
    error_message = f"{type(error).__name__}: {error}"[:300]
    report = {
        "status": "human_action_required",
        "code": "evaluator.runtime_error",
        "message": "当前有效约束的 evaluator 运行失败，治理裁判已失效。",
        "event": event,
        "constraint_id": inst.get("_constraint_id", ""),
        "evaluator": inst.get("evaluator", ""),
        "host_type": inst.get("_host_type", ""),
        "host_id": inst.get("_host_id", ""),
        "source_file": inst.get("_source_file", ""),
        "exception": error_message,
        "human_action": (
            "请人工修复 evaluator、约束数据或运行环境后重新启动任务；"
            "禁止把异常当成未命中继续执行。"),
    }
    reason = (
        "[HARNESS_TASK_ABORT][HUMAN_ACTION_REQUIRED] "
        "有效约束求值失败，当前 AI 任务已终止。\n"
        f"- [evaluator.runtime_error] event={event}, "
        f"constraint={report['constraint_id']}, "
        f"evaluator={report['evaluator']}, error={error_message}\n"
        f"{report['human_action']}"
    )
    return {
        "decision": "deny",
        "reason": reason,
        "abort_task": True,
        "error_type": "evaluator_runtime_error",
        "evaluator_report": report,
    }


def _evaluation_budget_failure_result(phase: str) -> dict:
    report = {
        "status": "human_action_required",
        "code": "evaluation.time_budget_exhausted",
        "message": "scope_guard 未能在内部治理求值预算内完成，无法形成可信裁决。",
        "phase": phase,
        "client_timeout_seconds": WORKBUDDY_GATE_TIMEOUT_SECONDS,
        "evaluation_budget_seconds": HARNESS_EVALUATION_BUDGET_SECONDS,
        "report_reserve_seconds": HARNESS_REPORT_RESERVE_SECONDS,
        "human_action": (
            "请人工检查项目规模、存储性能、有效 evaluator 或异常阻塞，"
            "修复后重新启动任务；禁止把超时当成未命中或降级放行。"),
    }
    reason = (
        "[HARNESS_TASK_ABORT][HUMAN_ACTION_REQUIRED] "
        "治理求值总预算耗尽，当前 AI 任务已终止。\n"
        f"- [evaluation.time_budget_exhausted] phase={phase}, "
        f"budget={HARNESS_EVALUATION_BUDGET_SECONDS}s, "
        f"client_timeout={WORKBUDDY_GATE_TIMEOUT_SECONDS}s\n"
        f"{report['human_action']}"
    )
    return {
        "decision": "deny",
        "reason": reason,
        "abort_task": True,
        "error_type": "evaluation_time_budget_exhausted",
        "time_budget_report": report,
    }


def _when_matches(inst: dict, event: str) -> bool:
    when = inst.get("when")
    return event in when if isinstance(when, list) else when == event


def evaluate(event: str, ctx: dict, instances: list[dict], task_context: dict) -> dict:
    """对某个时机（pre_write/pre_command/pre_commit/stop）逐条求值。
    返回 {"decision": "allow"|"deny", "reason": ..., "source": ...}"""
    warns = []
    for inst in instances:
        if not _is_active(inst, task_context):
            continue
        if not _when_matches(inst, event):
            continue
        evaluator = inst.get("evaluator")
        action = inst.get("action", "deny")
        reason = str(inst.get("reason", ""))
        fn = EVALUATORS.get(evaluator)
        if fn is None:
            return _evaluator_failure_result(
                inst, event, RuntimeError(f"未知 evaluator: {evaluator}"))
        if action not in ACTION_ENUM:
            action = "deny"
        try:
            _require_evaluation_budget(ctx)
            evaluator_ctx = ctx
            if evaluator in {"script", "require_active_skill"}:
                evaluator_ctx = dict(ctx)
                if evaluator == "script":
                    evaluator_ctx["_constraint_instance"] = inst
                else:
                    evaluator_ctx["_active_skill_set"] = set(
                        task_context.get("active_skill_set", set()))
            hit = fn(inst.get("data") or {}, evaluator_ctx)
        except Exception as error:
            return _evaluator_failure_result(inst, event, error)
        if hit:
            source = inst.get("_source", "?")
            # reason 优先级：约束实例的 reason > evaluator 返回的具体提示 > 默认
            eval_msg = hit if isinstance(hit, str) else ""
            final_reason = reason or eval_msg or f"命中约束 {evaluator}"
            if action == "deny":
                return {"decision": "deny", "reason": final_reason,
                        "source": source,
                        "source_file": inst.get("_source_file", ""),
                        "constraint_id": inst.get("_constraint_id", ""),
                        "host_type": inst.get("_host_type", ""),
                        "evaluator": evaluator}
            warns.append(f"{final_reason}（{source}）")

    if warns:
        return {"decision": "allow", "reason": "警告: " + "; ".join(warns), "warnings": warns}
    return {"decision": "allow", "reason": "ok"}


# ════════════════════════════════════════════════════════════════════
#  hook 协议包装 & 入口
# ════════════════════════════════════════════════════════════════════

class _HookProtocolError(ValueError):
    def __init__(self, code: str, message: str, meta: dict | None = None):
        super().__init__(message)
        self.code = code
        self.message = message
        self.meta = meta or {}


def _workbuddy_alias_value(tool_input: dict, fields: tuple[str, ...],
                           label: str, *, allow_empty: bool) -> str:
    """从官方 IDE/CLI 别名字段提取同一值；缺失、错型或冲突均视为协议失效。"""
    present = [(field, tool_input[field]) for field in fields if field in tool_input]
    if not present:
        raise _HookProtocolError(
            f"hook_payload.{label}_missing",
            f"tool_input 缺少 {label}；支持字段: {list(fields)}")
    invalid = [field for field, value in present if not isinstance(value, str)]
    if invalid:
        raise _HookProtocolError(
            f"hook_payload.{label}_type_invalid",
            f"tool_input 的 {invalid} 必须是字符串")
    values = {value for _, value in present}
    if len(values) > 1:
        raise _HookProtocolError(
            f"hook_payload.{label}_ambiguous",
            f"tool_input 同时提供了值不一致的 {label} 别名字段")
    value = present[0][1]
    if not allow_empty and not value.strip():
        raise _HookProtocolError(
            f"hook_payload.{label}_empty",
            f"tool_input 的 {label} 不得为空")
    return value


def _hook_protocol_failure_report(error: _HookProtocolError, event: str,
                                  payload) -> dict:
    top_fields = sorted(payload.keys()) if isinstance(payload, dict) else []
    tool_input = payload.get("tool_input") if isinstance(payload, dict) else None
    tool_fields = sorted(tool_input.keys()) if isinstance(tool_input, dict) else []
    return {
        "status": "human_action_required",
        "code": "hook_protocol.invalid",
        "message": "WorkBuddy PreToolUse hook 载荷无法可信解析，当前 AI 任务必须立即结束。",
        "internal_event": event,
        "hook_event_name": (
            payload.get("hook_event_name", "")
            if isinstance(payload, dict) else ""),
        "tool_name": (
            payload.get("tool_name", "")
            if isinstance(payload, dict) else ""),
        "session_id": (
            payload.get("session_id", "")
            if isinstance(payload, dict) else ""),
        "generic_tool_names_accepted": True,
        "known_specialized_adapters": sorted(WORKBUDDY_TOOL_KINDS),
        "received_top_level_fields": top_fields,
        "received_tool_input_fields": tool_fields,
        "diagnostics": [{
            "severity": "error",
            "code": error.code,
            "message": error.message,
        }],
        "human_action": (
            "请人工核对 WorkBuddy 版本、PreToolUse 官方载荷与 .codebuddy/settings.json matcher，"
            "修复通用 envelope 或显式 adapter 后重新启动任务；"
            "新的合法 tool_name 不需要加入白名单，禁止猜测字段或降级放行。"),
    }


def _parse_workbuddy_hook_payload(
        raw: str, event: str = "pre_tool") -> tuple[dict, dict | None]:
    """解析通用 WorkBuddy PreToolUse envelope；已知工具再使用专用 adapter。"""
    payload = None
    try:
        if not raw.strip():
            raise _HookProtocolError(
                "hook_payload.empty", "hook stdin 为空")
        try:
            payload = json.loads(raw)
        except json.JSONDecodeError as exc:
            raise _HookProtocolError(
                "hook_payload.invalid_json",
                f"hook stdin 不是有效 JSON（line {exc.lineno}, column {exc.colno}）")
        if not isinstance(payload, dict):
            raise _HookProtocolError(
                "hook_payload.root_type_invalid", "hook payload 根节点必须是对象")
        if payload.get("hook_event_name") != "PreToolUse":
            raise _HookProtocolError(
                "hook_payload.event_invalid",
                "hook_event_name 必须是 PreToolUse")
        tool_name = payload.get("tool_name")
        if not isinstance(tool_name, str) or not tool_name:
            raise _HookProtocolError(
                "hook_payload.tool_name_invalid",
                "tool_name 必须是非空字符串")
        tool_input = payload.get("tool_input")
        if not isinstance(tool_input, dict):
            raise _HookProtocolError(
                "hook_payload.tool_input_invalid",
                "tool_input 必须是对象")
        tool_kind = WORKBUDDY_TOOL_KINDS.get(tool_name, "generic")

        ctx: dict = {
            "tool_name": tool_name,
            "tool_kind": tool_kind,
            "tool_input": tool_input,
        }
        cwd = payload.get("cwd")
        if cwd is not None:
            if (not isinstance(cwd, str)
                    or not cwd.strip()
                    or not os.path.isabs(cwd)):
                raise _HookProtocolError(
                    "hook_payload.cwd_invalid",
                    "payload.cwd 存在时必须是非空绝对路径")
            ctx["cwd"] = os.path.abspath(cwd)

        if tool_kind in {"write", "edit", "delete"}:
            path_fields = (
                ("target_file", "file_path", "filePath")
                if tool_kind == "delete"
                else ("file_path", "filePath")
            )
            file_path = _workbuddy_alias_value(
                tool_input, path_fields, "write_target", allow_empty=False)
            if not os.path.isabs(file_path):
                if not isinstance(cwd, str):
                    raise _HookProtocolError(
                        "hook_payload.cwd_invalid",
                        "相对写入路径要求 payload.cwd 为非空绝对路径")
                ctx["path"] = _norm(file_path, cwd)
            else:
                ctx["path"] = _norm(file_path)
            if tool_kind != "delete":
                content_fields = (
                    ("content",)
                    if tool_kind == "write"
                    else ("new_string", "newString", "content")
                )
                ctx["content"] = _workbuddy_alias_value(
                    tool_input, content_fields, "write_content",
                    allow_empty=True)
        elif tool_kind == "command":
            ctx["command"] = _workbuddy_alias_value(
                tool_input, ("command",), "command", allow_empty=False)
            if not isinstance(cwd, str):
                raise _HookProtocolError(
                    "hook_payload.cwd_invalid",
                    "shell 命令要求 payload.cwd 为非空绝对路径")
        return ctx, None
    except _HookProtocolError as error:
        return {}, _hook_protocol_failure_report(error, event, payload)


def _hook_protocol_failure_result(report: dict) -> dict:
    lines = [
        "[HARNESS_TASK_ABORT][HUMAN_ACTION_REQUIRED] WorkBuddy hook 协议错误，当前 AI 任务已终止。",
        f"internal_event: {report.get('internal_event', '')}",
        f"hook_event_name: {report.get('hook_event_name', '')}",
        f"tool_name: {report.get('tool_name', '')}",
        f"known_specialized_adapters: "
        f"{json.dumps(report.get('known_specialized_adapters', []), ensure_ascii=False)}",
        f"received_top_level_fields: {json.dumps(report.get('received_top_level_fields', []), ensure_ascii=False)}",
        f"received_tool_input_fields: {json.dumps(report.get('received_tool_input_fields', []), ensure_ascii=False)}",
    ]
    for item in report.get("diagnostics", []):
        lines.append(
            f"- [{item.get('code', 'hook_payload.invalid')}] "
            f"{item.get('message', '')}")
    lines.append(str(report.get("human_action") or "请交由人工处理。"))
    return {
        "decision": "deny",
        "reason": "\n".join(lines),
        "abort_task": True,
        "error_type": "hook_protocol_invalid",
        "hook_protocol_report": report,
    }


def _tool_adapter_failure_result(
        code: str, message: str, hook_ctx: dict,
        providers: list[dict]) -> dict:
    """当前有效 provider 冲突或无法解释真实载荷时终止任务，禁止猜测。"""
    provider_views = [{
        "id": item.get("_provider_id", item.get("id", "")),
        "tool_glob": item.get("tool_glob", ""),
        "source_file": item.get("_source_file", ""),
        "host_type": item.get("_host_type", ""),
        "host_id": item.get("_host_id", ""),
    } for item in providers]
    tool_input = hook_ctx.get("tool_input")
    report = {
        "status": "human_action_required",
        "code": code,
        "message": message,
        "tool_name": hook_ctx.get("tool_name", ""),
        "tool_kind": hook_ctx.get("tool_kind", ""),
        "tool_input_fields": (
            sorted(tool_input) if isinstance(tool_input, dict) else []),
        "matching_providers": provider_views,
        "abort_task": True,
        "human_action": (
            "请人工核对当前有效 root/workflow/spec 中的 tool_providers，"
            "依据真实 WorkBuddy payload 修复重叠匹配或字段映射后重新启动任务；"
            "禁止猜测字段或改用未声明工具绕过。"),
    }
    lines = [
        "[HARNESS_TASK_ABORT][HUMAN_ACTION_REQUIRED] "
        "动态工具治理声明无法可信应用，当前 AI 任务已终止。",
        f"tool_name: {report['tool_name']}",
        f"code: {code}",
        f"message: {message}",
        f"matching_providers: {json.dumps(provider_views, ensure_ascii=False)}",
        report["human_action"],
    ]
    return {
        "decision": "deny",
        "reason": "\n".join(lines),
        "abort_task": True,
        "error_type": "tool_adapter_invalid",
        "tool_adapter_report": report,
    }


def _operation_unresolved_result(analysis: dict,
                                 blockers: list[dict]) -> dict:
    unresolved_events = analysis.get("unresolved_events", [])
    blocker_ids = [
        str(item.get("constraint_id") or "")
        for item in blockers
        if item.get("constraint_id")
    ]
    return {
        "decision": "deny",
        "reason": (
            "[HARNESS_OPERATION_UNRESOLVED] 当前工具调用的潜在副作用无法被可信归一化，"
            "并可能绕过当前有效硬约束；已拒绝本次调用，AI 任务可继续。\n"
            f"- unresolved_events={json.dumps(unresolved_events, ensure_ascii=False)}\n"
            f"- blocking_constraints={json.dumps(blocker_ids, ensure_ascii=False)}\n"
            "请改用具有明确专用 adapter、有效动态 provider 的工具，"
            "或可静态分析的受控命令。"
        ),
        "abort_task": False,
        "error_type": "operation_unresolved",
        "operation_report": {
            "status": "current_tool_call_denied",
            "code": "operation.effects_unresolved",
            "message": (
                "潜在副作用无法可信归一化，且存在可能被绕过的当前有效硬约束。"),
            "source_tool": analysis.get("source_tool", ""),
            "tool_kind": analysis.get("tool_kind", ""),
            "analysis_status": analysis.get("status", "unresolved"),
            "evidence": analysis.get("evidence", []),
            "unresolved_events": unresolved_events,
            "blocking_constraints": blockers,
            "abort_task": False,
            "recovery": (
                "改用具有明确专用 adapter、有效动态 provider 的工具，"
                "或可静态分析的受控命令；"
                "不得换其他未适配入口绕过。"),
        },
    }


def _proof_state_write_result(request: dict, proof: dict) -> dict:
    request_ctx = request.get("ctx") or {}
    path = str(proof.get("path") or request_ctx.get("path") or "")
    constraint_ids = list(proof.get("constraint_ids") or [])
    report = {
        "status": "current_tool_call_denied",
        "code": "proof_state.direct_write_denied",
        "message": "当前工具调用试图直接修改由有效 build_freshness 门禁使用的证明文件。",
        "path": path,
        "operation": str(request_ctx.get("operation") or "write"),
        "source_tool": str(request_ctx.get("source_tool") or ""),
        "blocking_constraints": constraint_ids,
        "sources": list(proof.get("sources") or []),
        "abort_task": False,
        "recovery": (
            "请运行负责生成该证明的受信 Harness 包装器；"
            "不得通过文件工具、Shell、MCP 或其他客户端工具直接改写证明。"),
    }
    return {
        "decision": "deny",
        "reason": (
            "[HARNESS_PROOF_STATE_PROTECTED] 当前有效编译门禁的证明文件由 Harness 保留，"
            "已拒绝本次直接修改；AI 任务可继续。\n"
            f"- path={path}\n"
            f"- operation={report['operation']}\n"
            f"- blocking_constraints={json.dumps(constraint_ids, ensure_ascii=False)}\n"
            f"{report['recovery']}"
        ),
        "abort_task": False,
        "error_type": "proof_state_direct_write",
        "proof_state_report": report,
    }


def _protected_proof_for_request(
        request: dict, protected: dict[str, dict]) -> dict | None:
    """精确修改证明文件，或删除其祖先目录，都会破坏证明。"""
    request_ctx = request.get("ctx") or {}
    request_path = str(request_ctx.get("path") or "")
    if not request_path:
        return None
    request_key = _canonical_file_key(request_path)
    exact = protected.get(request_key)
    if exact:
        return exact
    if str(request_ctx.get("operation") or "") != "delete":
        return None
    for proof_key, proof in protected.items():
        try:
            if os.path.commonpath([request_key, proof_key]) == request_key:
                return proof
        except ValueError:
            continue
    return None


def _proof_state_unresolved_blockers(
        protected: dict[str, dict]) -> list[dict]:
    blockers: list[dict] = []
    for proof in protected.values():
        for constraint_id in proof.get("constraint_ids") or [""]:
            blockers.append({
                "constraint_id": constraint_id,
                "evaluator": "build_freshness",
                "source_file": (
                    (proof.get("sources") or [""])[0]),
                "event": "pre_write",
                "protected_path": proof.get("path", ""),
                "mechanism": "proof_state_protection",
            })
    return blockers


def _path_contains(parent_key: str, child_key: str) -> bool:
    try:
        return os.path.commonpath([parent_key, child_key]) == parent_key
    except ValueError:
        return False


def _logical_file_key(path: str) -> str:
    """不解析 junction/symlink 的路径身份，用于保护 skill 的发现入口。"""
    absolute = (
        os.path.abspath(path)
        if os.path.isabs(path)
        else os.path.abspath(os.path.join(REPO_ROOT, path))
    )
    return os.path.normcase(os.path.normpath(absolute))


def _target_view(target: dict) -> dict:
    view = {
        "path": target.get("path", ""),
        "kind": target.get("kind", ""),
        "role": target.get("role", ""),
    }
    for field in ("lifecycle", "constraint_ids", "physical_path"):
        if target.get(field):
            view[field] = target[field]
    return view


def _decorate_protected_target(
        path: str, kind: str, role: str, **metadata) -> dict:
    target = {
        "path": _norm(path),
        "kind": kind,
        "role": role,
        "_logical_key": _logical_file_key(path),
        "_canonical_key": _canonical_file_key(path),
    }
    target.update(metadata)
    return target


def _control_plane_targets() -> list[dict]:
    targets: list[dict] = []
    for path, kind, role in HARNESS_CONTROL_PLANE_TARGETS:
        targets.append(_decorate_protected_target(
            path, kind, role, lifecycle="always"))
    return targets


def _protected_target_for_request(
        request: dict, targets: list[dict]) -> dict | None:
    """识别目标内部写入，以及会删除受保护目标的祖先路径。"""
    request_ctx = request.get("ctx") or {}
    request_path = str(request_ctx.get("path") or "")
    if not request_path:
        return None
    request_keys = {
        "logical": _logical_file_key(request_path),
        "canonical": _canonical_file_key(request_path),
    }
    operation = str(request_ctx.get("operation") or "")
    for target in targets:
        for mode, field in (
                ("logical", "_logical_key"),
                ("canonical", "_canonical_key")):
            target_key = str(target[field])
            request_key = request_keys[mode]
            if target["kind"] == "tree" and _path_contains(
                    target_key, request_key):
                return target
            if target["kind"] == "file" and request_key == target_key:
                return target
            if operation == "delete" and _path_contains(
                    request_key, target_key):
                return target
    return None


def _control_plane_target_for_request(request: dict) -> dict | None:
    """识别精确控制面写入，以及会删除控制面的祖先路径。"""
    return _protected_target_for_request(
        request, _control_plane_targets())


def _governance_plane_targets(task_context: dict) -> list[dict]:
    """固定策略入口 + 当前 active spec + junction 物理 skill。"""
    targets = [
        _decorate_protected_target(
            path, kind, role, lifecycle="always")
        for path, kind, role in HARNESS_POLICY_PLANE_TARGETS
    ]

    active_spec = task_context.get("active_spec") or {}
    active_spec_file = str(active_spec.get("source_file") or "")
    if active_spec_file:
        targets.append(_decorate_protected_target(
            active_spec_file, "file", "当前有效 spec 治理源",
            lifecycle="active_spec"))

    skills_root_key = _canonical_file_key(SKILLS_DIR)
    if os.path.isdir(SKILLS_DIR):
        for name in sorted(os.listdir(SKILLS_DIR)):
            skill_dir = os.path.join(SKILLS_DIR, name)
            if not os.path.isdir(skill_dir):
                continue
            physical_key = _canonical_file_key(skill_dir)
            if _path_contains(skills_root_key, physical_key):
                continue
            physical_path = os.path.realpath(skill_dir)
            targets.append(_decorate_protected_target(
                physical_path, "tree",
                f"junction skill {name} 的物理治理源",
                lifecycle="skill",
                physical_path=_norm(physical_path)))
    return targets


def _governance_plane_target_for_request(
        request: dict, targets: list[dict]) -> dict | None:
    return _protected_target_for_request(request, targets)


def _control_plane_approval_result(request: dict, target: dict) -> dict:
    request_ctx = request.get("ctx") or {}
    report = {
        "status": "human_confirmation_required",
        "code": "control_plane.write_requires_approval",
        "message": "当前工具调用将修改 Harness 信任基，必须由用户确认本次调用。",
        "target": target.get("path", ""),
        "target_kind": target.get("kind", ""),
        "target_role": target.get("role", ""),
        "requested_path": str(request_ctx.get("path") or ""),
        "operation": str(request_ctx.get("operation") or "write"),
        "source_tool": str(request_ctx.get("source_tool") or ""),
        "abort_task": False,
        "recovery": (
            "仅在用户明确要求维护 Harness 且确认当前工具参数无误时批准；"
            "本次批准不会建立持久维护模式，后续控制面修改仍需逐次确认。"),
    }
    return {
        "decision": "ask",
        "outcome": "human_confirmation_required",
        "reason": (
            "[HARNESS_CONTROL_PLANE_APPROVAL_REQUIRED] "
            "当前调用将修改 Harness 信任基，需要人工逐次确认。\n"
            f"- target={report['target']}\n"
            f"- requested_path={report['requested_path']}\n"
            f"- operation={report['operation']}\n"
            f"- role={report['target_role']}\n"
            f"{report['recovery']}"
        ),
        "abort_task": False,
        "error_type": "control_plane_approval_required",
        "control_plane_report": report,
    }


def _governance_plane_approval_result(request: dict, target: dict) -> dict:
    request_ctx = request.get("ctx") or {}
    report = {
        "status": "human_confirmation_required",
        "code": "governance_plane.write_requires_approval",
        "message": "当前工具调用将修改动态治理策略或激活状态，必须由用户确认本次调用。",
        "target": _target_view(target),
        "requested_path": str(request_ctx.get("path") or ""),
        "operation": str(request_ctx.get("operation") or "write"),
        "source_tool": str(request_ctx.get("source_tool") or ""),
        "abort_task": False,
        "recovery": (
            "仅在用户已确认这次 spec、skill、evaluator 或任务选择变更时批准；"
            "本次批准不会授权后续治理修改。"),
    }
    return {
        "decision": "ask",
        "outcome": "human_confirmation_required",
        "reason": (
            "[HARNESS_GOVERNANCE_PLANE_APPROVAL_REQUIRED] "
            "当前调用将修改动态治理策略面，需要人工逐次确认。\n"
            f"- target={json.dumps(report['target'], ensure_ascii=False)}\n"
            f"- requested_path={report['requested_path']}\n"
            f"- operation={report['operation']}\n"
            f"{report['recovery']}"
        ),
        "abort_task": False,
        "error_type": "governance_plane_approval_required",
        "governance_plane_report": report,
    }


def _control_plane_unresolved_blockers() -> list[dict]:
    return [{
        "constraint_id": f"harness.control-plane:{item['path']}",
        "evaluator": "harness_control_plane_integrity",
        "event": "pre_write",
        "protected_path": item["path"],
        "protected_kind": item["kind"],
        "mechanism": "control_plane_protection",
    } for item in _control_plane_targets()]


def _governance_plane_unresolved_blockers(
        targets: list[dict]) -> list[dict]:
    return [{
        "constraint_id": f"harness.governance-plane:{item['path']}",
        "evaluator": "harness_governance_plane_integrity",
        "event": "pre_write",
        "protected_path": item["path"],
        "protected_kind": item["kind"],
        "mechanism": "governance_plane_protection",
    } for item in targets]


def _unresolved_event_blockers(
        event: str, scope: dict, instances: list[dict],
        task_context: dict, analysis: dict,
        deadline_monotonic: float | None = None
        ) -> tuple[list[dict], dict | None]:
    """找出未知副作用可能绕过的当前有效 deny；状态型 evaluator 先精确求值。"""
    blockers: list[dict] = []
    for inst in instances:
        if not _is_active(inst, task_context):
            continue
        if not _when_matches(inst, event):
            continue
        if inst.get("action", "deny") != "deny":
            continue
        if inst.get("evaluator") == "require_active_skill":
            required_skill = str(
                (inst.get("data") or {}).get("skill") or "")
            if required_skill in task_context.get(
                    "active_skill_set", set()):
                continue
        if inst.get("evaluator") == "require_tool_capability":
            data = inst.get("data") or {}
            # capability 已由唯一 provider 精确给出。没有路径/操作限定时，
            # 即使该事件还有其他 unresolved 副作用，也已能证明本约束不会被绕过。
            # 带限定条件时，未知副作用的实际路径/操作仍不确定，必须保留 blocker。
            if (not data.get("path_pattern")
                    and not data.get("operation")
                    and str(data.get("capability") or "") in {
                        str(item)
                        for item in (analysis.get("tool_capabilities") or [])
                    }):
                continue
        if inst.get("evaluator") in UNRESOLVED_STATE_EVALUATORS:
            result = evaluate(
                event, {
                    "scope": scope,
                    "event": event,
                    "_deadline_monotonic": deadline_monotonic,
                },
                [inst], task_context)
            if result.get("abort_task"):
                return [], result
            if result.get("decision") != "deny":
                continue
        blockers.append({
            "constraint_id": inst.get("_constraint_id", ""),
            "evaluator": inst.get("evaluator", ""),
            "host_type": inst.get("_host_type", ""),
            "host_id": inst.get("_host_id", ""),
            "source_file": inst.get("_source_file", ""),
            "event": event,
        })
    return blockers, None


def _tree_descendant_requests(
        request: dict, deadline_monotonic: float | None
        ) -> tuple[list[dict], str, dict | None]:
    """把精确目录树删除展开为现有 evaluator 可复用的后代删除请求。

    不跟随 symlink/junction；遍历失败表示后代副作用无法证明，由现有
    unresolved pre_write 策略决定是否拒绝当前调用。
    """
    request_ctx = request.get("ctx") or {}
    if (str(request.get("event") or "") != "pre_write"
            or str(request_ctx.get("operation") or "") != "delete"
            or str(request_ctx.get("path_scope") or "exact") != "tree"):
        return [], "", None

    raw_path = str(request_ctx.get("path") or "")
    if not raw_path:
        return [], "tree_delete_path_missing", None
    absolute = (
        os.path.abspath(raw_path)
        if os.path.isabs(raw_path)
        else os.path.abspath(os.path.join(REPO_ROOT, raw_path))
    )
    repo_key = os.path.normcase(os.path.normpath(REPO_ROOT))
    absolute_key = os.path.normcase(os.path.normpath(absolute))
    try:
        inside_repo = os.path.commonpath(
            [repo_key, absolute_key]) == repo_key
    except ValueError:
        inside_repo = False
    if not inside_repo or not os.path.isdir(absolute):
        return [], "", None

    isjunction = getattr(os.path, "isjunction", None)
    if os.path.islink(absolute) or (isjunction and isjunction(absolute)):
        return [], "", None

    descendants: list[dict] = []
    pending = [absolute]
    while pending:
        if (deadline_monotonic is not None
                and time.monotonic() >= deadline_monotonic):
            return [], "", _evaluation_budget_failure_result(
                "tree_delete_descendant_enumeration")
        current = pending.pop()
        try:
            with os.scandir(current) as iterator:
                entries = sorted(
                    list(iterator),
                    key=lambda item: item.name.casefold(),
                    reverse=True)
        except OSError as error:
            return (
                descendants,
                f"tree_delete_enumeration_failed:"
                f"{type(error).__name__}:{_norm(current)}",
                None,
            )

        for entry in entries:
            child_ctx = dict(request_ctx)
            child_ctx["path"] = _norm(entry.path)
            child_ctx["path_scope"] = "exact"
            child_ctx["derived_from_tree"] = raw_path
            descendants.append({
                "event": "pre_write",
                "ctx": child_ctx,
            })
            try:
                is_directory = entry.is_dir(follow_symlinks=False)
                is_link = entry.is_symlink()
                is_junction = bool(isjunction and isjunction(entry.path))
            except OSError as error:
                return (
                    descendants,
                    f"tree_delete_entry_inspection_failed:"
                    f"{type(error).__name__}:{_norm(entry.path)}",
                    None,
                )
            if is_directory and not is_link and not is_junction:
                pending.append(entry.path)
    return descendants, "", None


def _evaluate_normalized_call(
        hook_ctx: dict, scope: dict, instances: list[dict],
        task_context: dict, providers: list[dict],
        deadline_monotonic: float | None = None) -> dict:
    if (deadline_monotonic is not None
            and time.monotonic() >= deadline_monotonic):
        return _evaluation_budget_failure_result(
            "before_tool_normalization")
    effective_providers = [
        item for item in providers if _is_active(item, task_context)]
    matched = matching_providers(
        str(hook_ctx.get("tool_name") or ""), effective_providers)
    if len(matched) > 1:
        return _tool_adapter_failure_result(
            "tool_adapter.provider_conflict",
            "当前工具同时匹配多个有效 provider，无法确定唯一语义真相源",
            hook_ctx, matched)
    provider = matched[0] if matched else None
    try:
        analysis = normalize_tool_call(
            hook_ctx, REPO_ROOT, provider=provider)
    except ToolAdapterError as error:
        return _tool_adapter_failure_result(
            error.code, error.message, hook_ctx, matched)
    protected_proofs, protection_failure = _effective_build_proof_files(
        instances, task_context)
    if protection_failure:
        return protection_failure
    governance_targets = _governance_plane_targets(task_context)
    control_plane_match: tuple[dict, dict] | None = None
    governance_plane_match: tuple[dict, dict] | None = None
    warnings: list[str] = []
    tree_descendants_evaluated = 0

    def evaluate_request(request: dict) -> dict | None:
        nonlocal control_plane_match
        nonlocal governance_plane_match
        nonlocal tree_descendants_evaluated
        event = str(request.get("event") or "")
        if event == "pre_write":
            proof = _protected_proof_for_request(
                request, protected_proofs)
            if proof:
                return _proof_state_write_result(request, proof)
            target = _control_plane_target_for_request(request)
            if target and control_plane_match is None:
                control_plane_match = (request, target)
            governance_target = _governance_plane_target_for_request(
                request, governance_targets)
            if governance_target and governance_plane_match is None:
                governance_plane_match = (request, governance_target)

        event = str(request.get("event") or "")
        request_ctx = dict(request.get("ctx") or {})
        request_ctx["scope"] = scope
        request_ctx["event"] = event
        request_ctx["_deadline_monotonic"] = deadline_monotonic
        result = evaluate(event, request_ctx, instances, task_context)
        if result.get("decision") == "deny":
            return result
        warnings.extend(result.get("warnings") or [])
        if request_ctx.get("derived_from_tree"):
            tree_descendants_evaluated += 1
        return None

    base_requests = list(analysis.get("evaluation_requests", []))
    for request in base_requests:
        failure = evaluate_request(request)
        if failure:
            return failure

    tree_unresolved: list[str] = []
    for request in base_requests:
        descendants, unresolved_reason, failure = (
            _tree_descendant_requests(
                request, deadline_monotonic))
        if failure:
            return failure
        for descendant in descendants:
            result = evaluate_request(descendant)
            if result:
                return result
        if unresolved_reason:
            tree_unresolved.append(unresolved_reason)

    if tree_descendants_evaluated:
        analysis.setdefault("evidence", []).append(
            f"tree_delete_descendants_evaluated:"
            f"{tree_descendants_evaluated}")
    if tree_unresolved:
        analysis["status"] = "unresolved"
        analysis["unresolved_events"] = list(dict.fromkeys(
            list(analysis.get("unresolved_events") or [])
            + ["pre_write"]))
        analysis.setdefault("evidence", []).extend(tree_unresolved)

    all_blockers: list[dict] = []
    for event in analysis.get("unresolved_events", []):
        blockers, failure = _unresolved_event_blockers(
            str(event), scope, instances, task_context, analysis,
            deadline_monotonic)
        if failure:
            return failure
        all_blockers.extend(blockers)
        if str(event) == "pre_write" and protected_proofs:
            all_blockers.extend(
                _proof_state_unresolved_blockers(protected_proofs))
        if str(event) == "pre_write" and control_plane_match is None:
            all_blockers.extend(_control_plane_unresolved_blockers())
        if str(event) == "pre_write" and governance_plane_match is None:
            all_blockers.extend(
                _governance_plane_unresolved_blockers(
                    governance_targets))
    if all_blockers:
        return _operation_unresolved_result(analysis, all_blockers)
    if (deadline_monotonic is not None
            and time.monotonic() >= deadline_monotonic):
        return _evaluation_budget_failure_result(
            "after_tool_evaluation")
    if control_plane_match:
        approval = _control_plane_approval_result(*control_plane_match)
        if warnings:
            approval["warnings"] = warnings
            approval["reason"] += (
                "\n- evaluator_warnings="
                + json.dumps(warnings, ensure_ascii=False))
        return approval
    if governance_plane_match:
        approval = _governance_plane_approval_result(
            *governance_plane_match)
        if warnings:
            approval["warnings"] = warnings
            approval["reason"] += (
                "\n- evaluator_warnings="
                + json.dumps(warnings, ensure_ascii=False))
        return approval

    result = {
        "decision": "allow",
        "outcome": "abstain",
        "reason": "Harness 未命中当前有效约束；交回 WorkBuddy 原生权限系统。",
        "analysis": analysis,
    }
    if warnings:
        result["warnings"] = warnings
        result["reason"] = "警告: " + "; ".join(warnings)
    return result


def _hook_response(result: dict, event: str = "pre_tool") -> dict:
    decision = result.get("decision", "allow")
    if decision == "allow":
        out: dict = {"continue": True}
        if result.get("warnings"):
            out["hookSpecificOutput"] = {
                "hookEventName": "PreToolUse",
                "additionalContext": result.get("reason", ""),
            }
        return out

    if decision == "ask":
        out = {
            "continue": True,
            "hookSpecificOutput": {
                "hookEventName": "PreToolUse",
                "permissionDecision": "ask",
                "permissionDecisionReason": result.get("reason", ""),
            },
        }
        report = (result.get("control_plane_report")
                  or result.get("governance_plane_report")
                  or result.get("recovery_report"))
        if isinstance(report, dict):
            out["harnessReport"] = report
        return out

    out = {
        "continue": False,
        "hookSpecificOutput": {
            "hookEventName": "PreToolUse",
            "permissionDecision": "deny",
            "permissionDecisionReason": result.get("reason", ""),
        }
    }
    report = (result.get("hook_protocol_report")
              or result.get("task_context_report")
              or result.get("tool_adapter_report")
              or result.get("evaluator_report")
              or result.get("time_budget_report")
              or result.get("proof_state_report")
              or result.get("operation_report"))
    if isinstance(report, dict):
        out["harnessReport"] = report
    if result.get("abort_task"):
        reason = str(result.get("reason") or "Harness 要求终止当前 AI 任务。")
        out["stopReason"] = reason
        out["systemMessage"] = reason
    return out


def _enforcement_contract() -> dict:
    contract = normalization_contract()
    contract.update({
        "client": "WorkBuddy",
        "hook_event_name": "PreToolUse",
        "capture": {
            "matcher": "*",
            "routing": "every PreToolUse call enters scope_guard exactly once",
        },
        "outcomes": {
            "abstain": (
                "continue:true without permissionDecision; "
                "native client permissions remain authoritative"),
            "deny_current_call": (
                "permissionDecision:deny without task-abort fields"),
            "human_confirmation": (
                "permissionDecision:ask; interactive user approval is required "
                "for this exact call and non-interactive clients deny"),
            "abort_task": (
                "permissionDecision:deny with HARNESS_TASK_ABORT, "
                "stopReason, and systemMessage"),
        },
        "unresolved_state_evaluators": sorted(
            UNRESOLVED_STATE_EVALUATORS),
        "script_evaluator": {
            "trust_model": "active skill-owned governance code",
            "allowed_hosts": ["root_skill", "workflow_skill"],
            "path_scope": (
                "owner-relative .py file whose real path remains inside "
                "the owning skill physical directory"),
            "protocol": "stdin JSON; exit 0=not hit, exit 1=hit, other=task abort",
            "python_mode": "-I -B",
            "default_timeout_seconds": SCRIPT_TIMEOUT_DEFAULT_SECONDS,
            "max_timeout_seconds": SCRIPT_TIMEOUT_MAX_SECONDS,
            "spec_policy": (
                "spec cannot execute code directly; reference a workflow "
                "skill through required_skills"),
        },
        "proof_state_protection": {
            "source": (
                "data.file of currently active action:deny "
                "build_freshness constraints"),
            "direct_write": (
                "deny the current call when normalized pre_write targets "
                "a proof file or deletes one of its ancestor directories"),
            "unresolved_write": (
                "deny the current call when unresolved pre_write may alter "
                "an active proof file"),
            "lifecycle": "derived from root/workflow/spec activation",
            "trusted_writer": (
                "the proof generator writes inside its trusted wrapper process; "
                "no client tool whitelist or bypass"),
        },
        "control_plane_protection": {
            "targets": [{
                "path": path,
                "kind": kind,
                "role": role,
            } for path, kind, role in HARNESS_CONTROL_PLANE_TARGETS],
            "precise_write": (
                "force permissionDecision:ask for each exact mutation"),
            "unresolved_write": (
                "deny the current call because approval cannot be scoped"),
            "maintenance_mode": (
                "none; approval is per call and cannot be enabled by "
                "repository state or a workflow skill"),
        },
        "governance_plane_protection": {
            "always_protected": [{
                "path": path,
                "kind": kind,
                "role": role,
            } for path, kind, role in HARNESS_POLICY_PLANE_TARGETS],
            "dynamic_targets": (
                "current active spec and physical targets of "
                "junction-backed skills"),
            "inactive_spec": "not protected until selected as active_spec",
            "precise_write": (
                "force permissionDecision:ask for each exact mutation"),
            "unresolved_write": (
                "deny the current call because approval cannot be scoped"),
            "activation_bypass": (
                "none; changing harness_state is itself human-confirmed"),
        },
        "task_context_recovery": {
            "trigger": (
                "task_context.invalid with dynamically derived "
                "repository-scoped repair_targets"),
            "normal_task": "abort_task",
            "inspection": (
                "observed WorkBuddy Read is limited to inspection_roots; "
                "trusted scope_guard.py context checks are allowed"),
            "read_adapters": [{
                "tool_name": name,
                "path_fields": list(WORKBUDDY_RECOVERY_READ_FIELDS[name]),
                "status": "verified_from_observed_workbuddy_session",
            } for name in sorted(WORKBUDDY_RECOVERY_READ_FIELDS)],
            "mutation": (
                "verified structured Write/Edit must target exactly one "
                "repair_target and returns permissionDecision:ask"),
            "other_calls": "abort_task",
            "persistent_maintenance_mode": False,
            "out_of_band_boundary": (
                "invalid hook JSON, scope_guard startup failure, or no "
                "safely derivable repair target"),
        },
        "time_budget": {
            "client_timeout_seconds": WORKBUDDY_GATE_TIMEOUT_SECONDS,
            "evaluation_budget_seconds": HARNESS_EVALUATION_BUDGET_SECONDS,
            "report_reserve_seconds": HARNESS_REPORT_RESERVE_SECONDS,
            "script_default_timeout_seconds": (
                SCRIPT_TIMEOUT_DEFAULT_SECONDS),
            "script_max_timeout_seconds": SCRIPT_TIMEOUT_MAX_SECONDS,
            "policy": (
                "all active evaluators share the internal budget; "
                "budget exhaustion aborts the task before the client timeout"),
        },
        "tool_adapters": [
            {
                "tool_name": name,
                "tool_kind": WORKBUDDY_TOOL_KINDS[name],
                "status": WORKBUDDY_ADAPTER_STATUS.get(name, "unverified"),
            }
            for name in sorted(WORKBUDDY_TOOL_KINDS)
        ],
        "tool_governance": tool_governance_contract(),
    })
    return contract


def _tool_governance_output(
        providers: list[dict], requirements: list[dict],
        task_context: dict, *, include_inactive: bool) -> dict:
    effective_providers = [
        item for item in providers if _is_active(item, task_context)]
    provider_views: list[dict] = []
    for item in providers:
        effective = _is_active(item, task_context)
        if not effective and not include_inactive:
            continue
        provider_views.append({
            "id": item.get("_provider_id", item.get("id", "")),
            "tool_glob": item.get("tool_glob", ""),
            "capabilities": item.get("capabilities") or [],
            "effects": item.get("effects") or [],
            "unresolved_events": item.get("unresolved_events") or [],
            "host_type": item.get("_host_type", ""),
            "host_id": item.get("_host_id", ""),
            "lifecycle": item.get("_lifecycle", ""),
            "source_file": item.get("_source_file", ""),
            "effective": effective,
            "runtime_availability": "unverified_use_toolsearch",
        })

    requirement_views: list[dict] = []
    for item in requirements:
        effective = _is_active(item, task_context)
        if not effective and not include_inactive:
            continue
        capability = str(item.get("capability") or "")
        candidates = [
            provider.get("_provider_id", provider.get("id", ""))
            for provider in effective_providers
            if capability in {
                str(value)
                for value in (provider.get("capabilities") or [])
            }
        ]
        requirement_views.append({
            "id": item.get("_requirement_id", ""),
            "capability": capability,
            "candidate_provider_ids": candidates,
            "host_type": item.get("_host_type", ""),
            "host_id": item.get("_host_id", ""),
            "lifecycle": item.get("_lifecycle", ""),
            "source_file": item.get("_source_file", ""),
            "effective": effective,
        })

    return {
        "contract": tool_governance_contract(),
        "providers_discovered": len(providers),
        "providers_effective": len(effective_providers),
        "requirements_discovered": len(requirements),
        "requirements_effective": sum(
            1 for item in requirements if _is_active(item, task_context)),
        "providers": provider_views,
        "requirements": requirement_views,
    }


def _preview_skills_output(
        requested_skills: list[str], root_skill_status: str,
        discovered: list[dict], diagnostics: list[dict],
        task_context: dict, providers: list[dict],
        requirements: list[dict]) -> dict:
    """输出候选 workflow 组合的只读 effective 集合预检。"""
    effective_constraints = [
        item for item in discovered if _is_active(item, task_context)
    ]
    blocking = _task_context_errors(diagnostics, task_context)
    effective_hosts: list[str] = []
    if root_skill_status == "loaded":
        effective_hosts.append(f"root_skill:{_project_name()}")
    effective_hosts.extend(
        f"workflow_skill:{name}"
        for name in task_context.get("active_skills", []))
    conflicts = [
        item for item in diagnostics
        if item.get("code") == "activation.effective_constraint_id_duplicate"
    ]
    ready = not blocking
    return {
        "mode": "candidate_task_preflight",
        "status": "ready" if ready else "invalid",
        "mutates_state_or_spec": False,
        "activates_skills": False,
        "authorizes_operations": False,
        "selection_source": "preview_only",
        "requested_skills": requested_skills,
        "resolved_skills": task_context.get("active_skills", []),
        "root_skill_status": root_skill_status,
        "candidate_effective_hosts": effective_hosts,
        "constraints_discovered": len(discovered),
        "constraints_effective": len(effective_constraints),
        "effective_constraints": [{
            "id": item.get("_constraint_id", ""),
            "declared_id": item.get("_declared_id", ""),
            "host_type": item.get("_host_type", ""),
            "host_id": item.get("_host_id", ""),
            "source_file": item.get("_source_file", ""),
        } for item in effective_constraints],
        "constraint_id_conflicts": conflicts,
        "tool_governance": _tool_governance_output(
            providers, requirements, task_context, include_inactive=False),
        "blocking_diagnostics": blocking,
        "diagnostics": diagnostics,
        "next_action": (
            "候选组合已通过预检；任务/spec 构建层可将 resolved_skills "
            "写入对应 selector，并在写入后重新运行 --context。"
            if ready else
            "不要写入 selector；先在意图对齐阶段处理 blocking_diagnostics，"
            "再重新预检。"
        ),
    }


def _preview_unavailable_output(
        requested_skills: list[str], diagnostics: list[dict],
        task_context_report: dict) -> dict:
    """当前真实任务上下文失效时，禁止用候选预检掩盖既有任务终止条件。"""
    return {
        "mode": "candidate_task_preflight",
        "status": "invalid_current_context_human_action_required",
        "evaluated": False,
        "mutates_state_or_spec": False,
        "activates_skills": False,
        "authorizes_operations": False,
        "requested_skills": requested_skills,
        "diagnostics": diagnostics,
        "task_context_report": task_context_report,
    }


def _context_output(scope: dict, discovered: list[dict], diagnostics: list[dict],
                    task_context: dict, providers: list[dict],
                    requirements: list[dict]) -> dict:
    current = scope.get("current", {}) or {}
    task_context_report = _task_context_failure_report(
        current, diagnostics, task_context, discovered)
    by_source: dict[str, int] = {}
    discovered_by_source: dict[str, int] = {}
    by_host_type: dict[str, int] = {}
    hosts: set[str] = set()
    effective_count = 0
    for inst in discovered:
        source = inst.get("_source", "?")
        discovered_by_source[source] = discovered_by_source.get(source, 0) + 1
        if not _is_active(inst, task_context):
            continue
        effective_count += 1
        s = inst.get("_source", "?")
        by_source[s] = by_source.get(s, 0) + 1
        host_type = str(inst.get("_host_type") or "unknown")
        by_host_type[host_type] = by_host_type.get(host_type, 0) + 1
        hosts.add(f"{host_type}:{inst.get('_host_id', '?')}")
    diagnostic_counts: dict[str, int] = {}
    for item in diagnostics:
        severity = str(item.get("severity") or "unknown")
        diagnostic_counts[severity] = diagnostic_counts.get(severity, 0) + 1
    protected_proofs, protection_failure = _effective_build_proof_files(
        discovered, task_context)
    governance_targets = _governance_plane_targets(task_context)
    proof_state_output = {
        "status": (
            "invalid_human_action_required" if protection_failure
            else "active" if protected_proofs
            else "inactive"
        ),
        "protected_files": list(protected_proofs.values()),
        "policy": "derived_from_active_deny_build_freshness",
    }
    if protection_failure:
        proof_state_output["failure"] = (
            protection_failure.get("evaluator_report")
            or protection_failure.get("reason", ""))
    output = {
        "governance_model": GOVERNANCE_MODEL_VERSION,
        "model_kind": "dynamic_hosted_constraints",
        "hook_protocol": {
            "client": "WorkBuddy",
            "hook_event_name": "PreToolUse",
            "capture_matcher": "*",
            "generic_tool_names_accepted": True,
            "specialized_tool_adapters": WORKBUDDY_TOOL_KINDS,
            "failure_policy": "task_abort_human_action_required",
        },
        "enforcement_contract": _enforcement_contract(),
        "control_plane_protection": {
            "status": "always_active",
            "targets": [{
                "path": path,
                "kind": kind,
                "role": role,
            } for path, kind, role in HARNESS_CONTROL_PLANE_TARGETS],
            "precise_mutation": "human_confirmation_required_per_call",
            "unresolved_mutation": "deny_current_call",
        },
        "governance_plane_protection": {
            "status": "active",
            "targets": [
                _target_view(item) for item in governance_targets
            ],
            "precise_mutation": "human_confirmation_required_per_call",
            "unresolved_mutation": "deny_current_call",
            "inactive_spec_mutation": "abstain_unless_other_constraint_hits",
        },
        "proof_state_protection": proof_state_output,
        "tool_governance": _tool_governance_output(
            providers, requirements, task_context, include_inactive=False),
        "activation_status": (
            "invalid_human_action_required" if task_context_report
            else "task_selector_active"
        ),
        "source_roles": SOURCE_ROLES,
        "project": _project_name(),
        "root_skill_status": scope.get("root_skill_status", "missing"),
        "stage": current.get("stage", ""),
        "active_spec": current.get("active_spec", ""),
        "active_spec_status": str(
            (task_context.get("active_spec") or {}).get("status") or ""),
        "root_skill": _root_skill_path().replace("\\", "/"),
        "state_file": STATE_PATH.replace("\\", "/"),
        "active_skills": [
            {"name": name, "selected_by": task_context.get("selected_by", {}).get(name, [])}
            for name in task_context.get("active_skills", [])
        ],
        "instances_loaded": effective_count,
        "constraints_discovered": len(discovered),
        "constraints_effective": effective_count,
        "constraints_evaluation_candidates": effective_count,
        "discovered_by_source": discovered_by_source,
        "instances_by_source": by_source,
        "instances_by_host_type": by_host_type,
        "hosts_loaded": sorted(hosts),
        "evaluators": sorted(EVALUATORS.keys()),
        "when_enum": list(WHEN_ENUM),
        "diagnostic_counts": diagnostic_counts,
        "diagnostics": diagnostics,
    }
    if task_context_report:
        output["task_context_report"] = task_context_report
    if scope.get("root_skill_status") == "missing":
        output["root_skill_setup"] = _root_skill_setup_info()
    return output


def _explain_output(scope: dict, discovered: list[dict], diagnostics: list[dict],
                    task_context: dict, providers: list[dict],
                    requirements: list[dict]) -> dict:
    active_spec_file = str((task_context.get("active_spec") or {}).get("source_file") or "")
    selected_by_skill = task_context.get("selected_by", {}) or {}
    explained = []
    for inst in discovered:
        source_file = str(inst.get("_source_file") or "")
        host_type = str(inst.get("_host_type") or "")
        host_id = str(inst.get("_host_id") or "")
        is_effective = _is_active(inst, task_context)
        selected_by_state = None
        activation_reason = "not_selected"
        if host_type == "root_skill":
            activation_reason = "root_always_active"
        elif host_type == "workflow_skill" and is_effective:
            activation_reason = selected_by_skill.get(host_id, ["task_selected"])
        if host_type == "spec":
            selected_by_state = bool(active_spec_file and source_file == active_spec_file)
            activation_reason = "active_spec" if selected_by_state else "inactive_spec"
        explained.append({
            "id": inst.get("_constraint_id", ""),
            "declared_id": inst.get("_declared_id", ""),
            "host_type": host_type,
            "host_id": host_id,
            "lifecycle": inst.get("_lifecycle", ""),
            "source": inst.get("_source", ""),
            "source_file": source_file,
            "canonical_source_file": inst.get("_canonical_source_file", source_file),
            "effective": is_effective,
            "activation_reason": activation_reason,
            "selected_by_state": selected_by_state,
            "evaluator": inst.get("evaluator"),
            "when": inst.get("when"),
            "action": inst.get("action", "deny"),
            "data": inst.get("data") or {},
            "reason": inst.get("reason", ""),
        })
    return {
        "context": _context_output(
            scope, discovered, diagnostics, task_context,
            providers, requirements),
        "instances": explained,
        "tool_governance_all_hosts": _tool_governance_output(
            providers, requirements, task_context, include_inactive=True),
    }


def _build_scope(state: dict, root_skill_status: str) -> dict:
    """组装 evaluator 读取的运行时 scope；约束数据由各 constraint 自有。"""
    return {
        "root_skill_status": root_skill_status,
        "current": {
            "stage": state.get("stage"),
            "active_spec": state.get("active_spec"),
            "active_skills": state.get("active_skills"),
        },
    }


def main() -> int:
    argv = sys.argv[1:]

    # 解析 --event（默认 pre_write）与 --hook-stdin / --context / 位置参数
    event = "pre_write"
    rest = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--event" and i + 1 < len(argv):
            event = argv[i + 1]
            i += 2
        else:
            rest.append(a)
            i += 1

    preview_requested = (
        list(rest[1:])
        if rest and rest[0] == "--preview-skills"
        else None
    )

    gate_deadline: float | None = None
    if ((rest and rest[0] == "--hook-stdin")
            or (not rest and event == "stop")):
        gate_deadline = (
            time.monotonic() + HARNESS_EVALUATION_BUDGET_SECONDS)

    diagnostics: list[dict] = []
    project_diagnostic = _project_diagnostic()
    if project_diagnostic:
        diagnostics.append(project_diagnostic)

    root_skill, root_skill_status, root_error = _load_root_skill()
    if root_skill_status == "missing":
        diagnostics.append(_diagnostic(
            "warning", "root_skill.missing",
            "项目尚未建立 root skill；harness 将继续运行 workflow/spec 机制，并建议用户初始化",
            _relative_source_file(_root_skill_path()) if _root_skill_path() else ""))
    elif root_skill_status == "invalid":
        diagnostics.append(_diagnostic(
            "error", "root_skill.invalid", root_error,
            _relative_source_file(_root_skill_path()) if _root_skill_path() else ""))

    state = _load_state(diagnostics)
    scope = _build_scope(state, root_skill_status)
    discovered = discover_constraints(root_skill, diagnostics)
    providers, requirements = discover_tool_governance(
        root_skill, diagnostics)

    current_diagnostics = [dict(item) for item in diagnostics]
    current_diagnostics.extend(validate_constraints(discovered, scope))
    current_task_context = resolve_task_context(
        state, current_diagnostics)
    current_diagnostics.extend(validate_effective_constraint_ids(
        discovered, current_task_context))
    current_diagnostics.extend(validate_effective_tool_requirements(
        providers, requirements, current_task_context))
    _annotate_diagnostic_activity(
        current_diagnostics, current_task_context)
    current_task_context_report = _task_context_failure_report(
        state, current_diagnostics, current_task_context, discovered)

    if preview_requested is not None:
        if current_task_context_report:
            print(json.dumps(
                _preview_unavailable_output(
                    preview_requested, current_diagnostics,
                    current_task_context_report),
                ensure_ascii=False, indent=2))
            return 2

        preview_diagnostics = [dict(item) for item in diagnostics]
        # 意图对齐预检不借用当前 stage 评价候选宿主；这里只校验声明本身。
        preview_diagnostics.extend(validate_constraints(discovered, None))
        preview_task_context = _preview_task_context(
            preview_requested, preview_diagnostics)
        preview_diagnostics.extend(validate_effective_constraint_ids(
            discovered, preview_task_context))
        preview_diagnostics.extend(validate_effective_tool_requirements(
            providers, requirements, preview_task_context))
        _annotate_diagnostic_activity(
            preview_diagnostics, preview_task_context)
        preview_output = _preview_skills_output(
            preview_requested, root_skill_status, discovered,
            preview_diagnostics, preview_task_context,
            providers, requirements)
        print(json.dumps(preview_output, ensure_ascii=False, indent=2))
        return 0 if preview_output["status"] == "ready" else 2

    diagnostics = current_diagnostics
    task_context = current_task_context
    task_context_report = current_task_context_report

    if rest and rest[0] == "--context":
        print(json.dumps(
            _context_output(
                scope, discovered, diagnostics, task_context,
                providers, requirements),
            ensure_ascii=False, indent=2))
        return 2 if task_context_report else 0

    if rest and rest[0] == "--explain":
        print(json.dumps(
            _explain_output(
                scope, discovered, diagnostics, task_context,
                providers, requirements),
            ensure_ascii=False, indent=2))
        return 2 if task_context_report else 0

    ctx: dict = {
        "scope": scope,
        "_deadline_monotonic": gate_deadline,
    }

    if rest and rest[0] == "--hook-stdin":
        if (gate_deadline is not None
                and time.monotonic() >= gate_deadline):
            result = _evaluation_budget_failure_result(
                "governance_context_loading")
        else:
            hook_ctx, hook_protocol_report = _parse_workbuddy_hook_payload(
                sys.stdin.read())
            if hook_protocol_report:
                result = _hook_protocol_failure_result(
                    hook_protocol_report)
            elif task_context_report:
                result = _task_context_recovery_result(
                    hook_ctx, task_context_report)
            else:
                result = _evaluate_normalized_call(
                    hook_ctx, scope, discovered, task_context, providers,
                    gate_deadline)
        print(json.dumps(_hook_response(result), ensure_ascii=False))
        if result["decision"] == "deny":
            sys.stderr.write(str(result.get("reason") or "Harness denied operation") + "\n")
            sys.stderr.flush()
            return 2
        return 0

    # 便捷：位置参数 = 文件路径，按当前 event 判定
    if rest and not rest[0].startswith("--"):
        ctx["path"] = _norm(rest[0])
        ctx["event"] = event
        result = (_task_context_failure_result(task_context_report)
                  if task_context_report
                  else evaluate(event, ctx, discovered, task_context))
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 2 if result["decision"] == "deny" else 0

    # 无参数命令模式（如 stop hook 直接跑 --event stop）
    ctx["event"] = event
    result = (_task_context_failure_result(task_context_report)
              if task_context_report
              else evaluate(event, ctx, discovered, task_context))
    if (result.get("decision") != "deny"
            and gate_deadline is not None
            and time.monotonic() >= gate_deadline):
        result = _evaluation_budget_failure_result(
            "after_event_evaluation")
    if event == "stop":
        if result["decision"] == "deny":
            print(json.dumps({"continue": False, "stopReason": "[HARNESS] " + result.get("reason", "")},
                             ensure_ascii=False))
            return 2
        print(json.dumps({"continue": True}, ensure_ascii=False))
        return 0

    print(json.dumps(_hook_response(result, event), ensure_ascii=False))
    return 2 if result["decision"] == "deny" else 0


if __name__ == "__main__":
    sys.exit(main())
