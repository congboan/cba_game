---
name: agent-parallel-work
description: 多 agent 并行的切分原则、可并行判定、角色映射与批次纪律
constraints:
  - id: workflow.parallel.subagent-write-scope
    evaluator: script
    when: pre_write
    data: {script: "checks/check_subagent_write_scope.py"}
---

# Agent 并行工作

## 平台硬约束（先读：违反会 abort 或返工）

1. **prompt 极短**：hook stdin 约 430 列截断。prompt 用 ASCII、约 150 字符内；完整上下文
   由文件/spec 传递，禁止贴规范正文。
2. **批末编译**：PostToolUse 自动编译已移除（ADR 2026-09-10）。一批全部结束后，由主 agent
   显式跑一次 `build_editor.py`；编译责任不下放给 subagent。
3. **写集互斥**：并行组之间写集两两不交，含同一文件的不同部分。

## 并行单位

UE 模块粒度在本项目无空间（插件常仅 1 个 Module）→ 单位为**目录树 = 文件组**，并按
关注点二次切分：**接口基类文件上提到串行区**，并行组只含具体实现文件。
即 并行单位 = 「目录 − 接口文件」。

## 可并行判定

三条全中才可同批并行：

| 条件 | 判据 |
|---|---|
| N1 写集互斥 | 任意两组 writes(glob) 两两不交 |
| N2 无依赖 | B 下笔不需要 A 的产出 |
| N3 上下文自包含 | 契约可完整注入（走文件，不走 prompt） |
| 兜底 | 写集为空的纯读任务永远可并行 |

## 角色与类型映射

| 角色 | 类型 | 权限 | 产出 |
|---|---|---|---|
| Planner | Plan 或主 agent | 读+分析 | 任务清单、写集划分、依赖图 |
| Explorer | Explore | 读 | 搜索／依赖分析结论 |
| Coder | general-purpose | 读写 | 实现 diff |
| Reviewer | **Explore** | 读 | 审查结论 |

Reviewer 用 Explore 是唯一能机械保证只读的手段（Explore 无写工具）；Coder/Reviewer
分离由类型不同强制成立。Explore 深度参数：quick／medium／very thorough。

## 分组与批次

流程：归接缝（爆炸半径／目录／插槽／类型）→ 声明 writes → 建依赖图 → 有环则回退重切 →
写集相交或有依赖者同组 → 组间无依赖且写集互斥者同批。

批内上限由主 agent 汇总带宽决定（核实产出是串行的，耗时 ∝ 组数）：≤3 组用独立 Agent，
4+ 组用 Team 模式。

## 阶段适配

| Stage | 并行策略 |
|---|---|
| design | Explore × N 多维分析并行（只读，无门槛） |
| build | 并行实现须先冻结接口；批末编译 |
| review | Explore × N 多维度审查并行 |
| test | 搜索覆盖缺口 + 生成测试 |

## 串行独占区（永不下放／永不并行）

1. 唯一真相源写入：接口签名、Row 字段、tag 常量表
2. 跨任务裁决：接口／命名冲突
3. 治理控制面：state、spec、`.workbuddy/skills`、`harness/scripts`、build 指纹
4. 需全局视野的重构
5. 接口尚未冻结时的任何下游任务

## 前置条件：接口冻结

并行开始前，组间接口（类型名、函数签名、字段、tag 字面量）必须已写死；不满足则整批串行。
理由：两侧各自发明后合不回来，返工远超并行收益。

## 汇总

主 agent 逐组核实产出（读实际文件、跑门禁），不信任 agent 自述；结果落主回复。

## 反模式（禁止并行）

同文件修改／写集相交／紧耦合依赖／微小任务（spawn 开销>收益）／共享唯一真相源／
接口未冻结／顺序性约束

## 与门禁的关系

本 skill 声明 1 条 deny 约束 `workflow.parallel.subagent-write-scope`：subagent 的 `pre_write`
必须落在 spec 的 `parallel_write_sets` 内。Harness 门禁对主 agent 与 subagent 统一生效
（PreToolUse 与 PostToolUse 均已 2026-09-10 实证）；hook payload 带 `agent_type`／`agent_id`
可区分调用者。故「子 agent 越界写入」已是机械门禁；组间互斥仍靠主 agent 的分批计划
（认知约束）。
AI 遵守 root skill 全部硬约束。
