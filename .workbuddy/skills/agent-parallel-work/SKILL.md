---
name: agent-parallel-work
description: 指导 AI 识别可并行化任务，使用 subagent 并行执行
---

# Agent 并行工作

## 核心原则

AI 执行任务时必须主动识别并行机会。

三条硬规则：
1. 独立子任务可并行 — 不修改同一文件、不共享状态
2. 只读操作永远可并行 — 搜索、分析无风险
3. 有依赖链必须串行 — A 的输出是 B 的输入

决策流程：先判断是否可拆，再判断文件是否重叠。纯只读探索 → 永远并行。

## Agent 类型选型指南

| 类型 | 能力 | 适用 | 限制 |
|---|---|---|---|
| Explore | Read/Grep/Glob | 搜索、依赖分析 | 不能写文件 |
| general-purpose | 全部工具 | 实现、修bug、重构 | — |
| Plan | 全部(偏分析) | 架构、技术决策 | 不适合纯执行 |

Explore 深度：quick(简单找) / medium(跨文件) / very thorough(全量审计)

## 并行场景模板

场景1：多目录代码搜索 → Explore × N，按目录拆分独立搜索

场景2：多文件独立生成 → general-purpose × N，确认文件无交叉依赖后并行

场景3：编译+分析并行 → 主 agent 编译，同时 spawn Explore 分析代码规范/依赖

场景4：架构评审 → Explore × 2-3，分别分析依赖、命名、配置

场景5：多 Bug 修复 → general × N，确认 bug 文件不重叠后并行修复

## Team 模式 vs 独立 Agent

≤3 个独立子任务 → 独立 Agent（run_in_background=true）
4+ 个或需角色协调 → Team 模式（共享 task list）
有明确的设计→实现→验证流水线 → Team + 角色分工

## 阶段适配

| Stage | 主导 Agent | 并行策略 |
|---|---|---|
| design | Explore + Plan | 多维分析并行 |
| build | general + Explore | 编译后台 + 并行实现 |
| review | Explore × N | 多维度审查并行 |
| test | Explore + general | 搜索覆盖缺口 + 生成测试 |

## 反模式（禁止并行）

同文件修改 / 紧耦合依赖 / 微小任务(spawn开销>收益) / 共享上下文 / 顺序性约束

## 执行规范

并行前检查：路径不重叠、无数据依赖、prompt 自包含、明确 read/write 意图

Prompt 编写：含完整路径和上下文、说明任务目的和预期输出、不假设 subagent 知道主对话历史

结果汇总：核实每个 agent 产出（信任但验证）、Read 确认文件变更、汇总到主回复

## 与现有约束的关系

认知指导型 skill，不声明 deny 约束。AI 遵守 root skill 全部硬约束（path_glob、command_write、build_freshness 等），自主判断何时并行。Harness 门禁对主 agent 和 subagent 统一生效。
