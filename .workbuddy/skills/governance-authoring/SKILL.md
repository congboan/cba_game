---
name: governance-authoring
description: WorkBuddy 治理编写工作流。用户要求新增、完善、拆分、移动或删除 skill、constraint、skill-owned gate script、spec、plugin-owned skill/junction、state、hook、tool provider/capability、payload adapter、evaluator 或治理测试时使用；在任何写入前重建动态治理模型，确定宿主、生命周期、回收条件和测试归属。
---

# Governance Authoring

## 认知前置

0. 生成/完善 spec 时，内容必须从需求意图反推（"要测什么/要做什么"），
   禁止用通用模板填充（如"插件 = Actor + Action + 命令"）；验收标准必须
   覆盖"是否真的实现了意图"，而不只是"能否编译运行"。

在修改任何治理文件前：

1. 完整读取项目 `AGENTS.md`、`harness/README.md`、相关最新 ADR、当前 state，以及存在时的项目根 skill。
2. 运行 `python harness/scripts/scope_guard.py --context`；需要逐条追踪时运行 `python harness/scripts/scope_guard.py --explain`。
3. 确认进入本 skill 前，任务/spec 构建层已经通过现有 selector 选择本 skill，并在 context 的
   `active_skills` 中报告正确来源。本 skill 只能验证选择结果，不能修改 state、spec 或其他 selector
   来决定自己的生效方式；未被选择时停止治理写入并返回任务构建层处理。
4. 区分治理模型与当前实例：harness 定义发现、解析、选择、调度、求值和验证机制；约束实例由 root skill、workflow skill 或 spec 动态提供，复杂门禁实现可由 root/workflow skill 自有脚本提供；state 只保存运行状态。
5. 必须分别识别 discovered、active 与 effective：文件存在只表示可发现，不表示当前任务已加载；root skill 存在时常驻，其他 workflow skill 必须来自当前任务、confirmed spec 或对话中的明确选择。
6. 诊断同样服从宿主生命周期：当前有效 root/workflow/spec 的 error 必须终止正常任务；未激活宿主的 error 只保留为诊断，不能误判为当前任务失败；warning 不阻断。用户随后明确发起治理修复且
   `task_context_report.recovery` 提供受限目标时，只能按该协议检查和逐次确认修复；skill 不能自行开启
   维护模式或扩大目标。
7. WorkBuddy PreToolUse 使用通用 envelope：合法新 `tool_name` 不因未知名称失败；公共结构损坏仍以 `hook_protocol_report` 终止任务。具体工具语义只能来自已验证专用 adapter 或当前有效 `tool_providers`，不得按名称或相似字段猜测。
8. 必须区分四种裁决：`abstain` 只返回 `continue:true` 且不输出 `permissionDecision:allow`，权限交回 WorkBuddy；普通 constraint deny、治理编写 workflow 未激活或 `HARNESS_OPERATION_UNRESOLVED` / `operation_report` 只拒绝当前调用；`permissionDecision:ask` 把精确的控制面或策略面修改交给人逐次确认；只有 `HARNESS_TASK_ABORT` 才结束整个任务。
9. 工具名本身既不授权也不禁止。settings 只做单一通配静态路由；scope_guard 负责协议、provider/capability、语义证据、effective constraint 和裁决。`pre_tool` / `pre_write` / `pre_command` / `pre_commit` 是 evaluator 的语义时机，不是具体工具的一对一别名，不得按工具复制同一约束。
10. provider 声明治理映射，不证明客户端已安装工具；ToolSearch 只发现延迟/MCP 工具，不激活 skill、不授予 capability、不改变门禁。required capability 无有效 provider 是治理上下文错误；provider 有声明但工具不可用时停止并向人报告，不得回退到未声明入口。
11. 声称“硬门禁”前必须逐项证明：真实工具被通配 matcher 捕获、公共 envelope 可信、所需专用 adapter 或唯一有效 provider 能提供 evaluator 证据或识别相关 unresolved effect、宿主当前 effective、skill-owned script 的所有权与退出协议有效、WorkBuddy 执行 deny。缺少任一项只能记录为认知约束或覆盖缺口。
12. `command_write` 的 `asset_patterns` / `write_hints` / `read_hints` 只属于声明它的 constraint 数据，
    不能作为通用命令授权白名单；normalizer 的只读/受控识别只表示本次没有发现现有 evaluator
    所关心的副作用。
13. 区分执行刷新与认知刷新：机器求值可在下一次运行重新发现磁盘，已经进入 AI 上下文的旧 AGENTS/skill 必须显式重载才能撤销。
14. memory 只作为历史背景，示例只说明 schema；两者都不是现行规范或授权来源。
15. 编译门禁必须证明“当前源码就是成功编译过的源码”；孤立 `success:true` 或 PostToolUse 工具覆盖不能证明新鲜度。内容指纹是正确性边界，自动编译只是反馈优化。

## 归属决策

按顺序确定唯一宿主：

1. 跨项目稳定的发现、解析、调度、求值或协议能力 → `harness/`。
2. 本项目整个生命周期成立的知识、配表或约束 → 项目根 skill。
3. 只在采用某种做法时成立的步骤或约束 → 对应 workflow skill。
4. 只服务于一个需求的范围、验收或爆炸半径 → active spec。
5. 只在开发某个 plugin 本身时成立的知识、步骤或约束 → 该 plugin 物理拥有的 workflow skill，存放在 `Plugins/<X>/.workbuddy/skills/<name>/`，通过项目 `.workbuddy/skills/<name>/` junction 暴露给 WorkBuddy。
6. 任务只是使用、依赖、调用或配置 plugin 时成立的约束 → 对应项目 workflow skill；不得激活 plugin 开发 skill。
7. 当前 stage、active spec 等指针 → state。
8. 无法唯一归属 → 停止写入并询问用户。

`tool_providers` 和 `required_tool_capabilities` 不建立新宿主：项目常驻需求归 root skill，某种做法所需
工具归 workflow skill，单个需求专用工具归 spec；它们复用对应宿主的激活和回收语义。

任务 skill 的选择遵守以下边界：

- root skill 存在时始终激活；缺失时提示用户初始化但不得自动补造，已存在却损坏时立即停止正常任务并交给人处理。若人明确启动修复且 Harness 给出受限恢复目标，才可逐次确认修改该目标；workflow skill 不得因目录存在而自动视为当前任务依赖。
- confirmed spec 通过 `required_skills` 引用正式任务 skill，不复制 skill 内容或约束。
- 无 spec 的任务可以由 AI 根据对话选择 skill，但必须明确说明名称与原因。
- 任务/spec 构建层在固化候选 workflow 前，必须运行
  `python harness/scripts/scope_guard.py --preview-skills <workflow...>`；显式空依赖使用不带名称的命令。
  只有 `status: ready` 才能写入 selector。该预检只模拟 root 与候选 workflow 的 effective 集合，
  不修改 selector、不激活 skill、不授权；`invalid` 应在意图对齐阶段处理，运行期校验仍保留为最后防线。
- confirmed spec 建立后，新增或移除正式 task skill 必须同步修改 spec。
- skill 只能增加知识、流程和限制，不能扩大 spec 授予的修改范围。
- junction 只是发现投影，不是独立宿主或第二真相源；plugin-owned skill 的正文、约束声明、门禁脚本和测试以 plugin 内物理目录为准，删除 plugin 时一并删除 junction。

优先使用内置原子 evaluator 和声明式 `data`。复杂且随项目或做法生命周期生灭的门禁逻辑放在所属
root/workflow skill 的 `checks/` 中，通过 `script` evaluator 调用；脚本是受信治理代码，不是普通任务
脚本。`data.script` 只能是所属 skill 物理目录内的相对 `.py` 路径，不能声明参数、工作目录、绝对路径
或 `..`。spec 不直接执行代码；需要复杂门禁时通过 `required_skills` 引用 workflow skill。

## 写入前声明

在内部明确以下内容后才能编辑：

- 变更类型；
- 目标宿主；
- 生命周期；
- 删除哪个宿主时回收；
- evaluator 语义、约束数据与门禁脚本归属；
- 测试归属；
- 是否已有重复真相源；
- 当前宿主是仅被发现，还是已由任务激活；激活来源是什么；
- context 是否在 `active_skills` 中明确报告本 workflow 及其外部选择来源；
- 工具 provider/capability 是否属于该宿主，真实 `tool_name` 与 payload 字段证据来自哪里；
- 为什么需要或不需要修改 harness。

约束声明、项目数据、skill-owned 门禁脚本和规则测试必须跟随同一宿主；evaluator 调度机制及其机制测试
属于 harness。不得把同一约束复制到 `AGENTS.md`、harness 和 skill 等多个位置。

## 实施与验证

1. 只修改已确定的宿主及必要机制。
2. 约束使用 harness 已定义的 schema；不要在 YAML 中发明表达式或组合逻辑。只有 `script` 可以引用
   所属 skill 内的受信 `.py` 门禁脚本，并严格遵守 0=未命中、1=命中、其他=任务终止的协议。
3. 新增约束时使用稳定 ID；同一宿主内及当前任务的 effective 集合内必须唯一。Spec 在意图对齐阶段
   选择会同时生效的 workflow skill，不能把同名 constraint 当作运行时合并或覆盖；共同长期规则应迁移到
   唯一宿主，不同规则使用不同 ID。
4. harness 机制测试使用临时 `TestProject`，不得固化真实项目规则。
5. 项目或工作流规则测试放在所属 skill 的 `tests/` 下；删除 skill 时一并回收。
6. 已存在机制测试套件时运行它；未建立持久化测试目录时，使用临时目录、stdin 或内存矩阵验证，不得仅为一次变更把真实项目规则固化进 harness。
7. 用 `scope_guard.py --context`/`--explain` 确认来源、生命周期和诊断结果。
8. context 未报告本 workflow 已由外部 selector 选择时不得尝试治理写入；返回对话/spec 构建层处理，
   本 skill 不得自行修改 selector。
9. 若 context/explain 无法建立可信治理上下文，停止治理写入并报告问题；不得用当前偶然加载结果补造模型。
10. 若诊断属于当前有效宿主，修复后必须重新运行 context；不得因为 evaluator 尚未执行就把声明错误视为可降级问题。
    上下文已损坏时，只能使用 `task_context_report.recovery` 声明的检查范围和精确修复目标；没有
    `restricted_human_confirmation_available` 就停止并要求 hook 外人工处理。
11. 修改 hook matcher 或通用 envelope 时验证每个 PreToolUse 只进入一次；通配捕获不按工具扩展。修改专用 adapter 或动态 provider schema 时，必须使用 WorkBuddy 官方字段或用户提供的真实 payload 样本；其他客户端需要独立协议决策。
12. 修改 PreToolUse 归一化或裁决时，必须分别验证：合法未知工具不是协议错误；无相关有效 deny 时 abstain 且不显式 allow；精确证据命中具体约束时该原因优先；unresolved effect 仅在可能绕过相关有效 deny 时拒绝当前调用；协议、任务上下文、有效宿主、provider 冲突/字段映射或 evaluator 失败仍终止任务；git commit 只由一次工具调用产生一次 `pre_commit` 求值。
13. 修改工具治理时还要验证 provider/requirement 的 discovered 与 effective 分离、required capability 缺失 provider 时 fail-closed、ToolSearch 不参与授权，以及 `--context` 不把声明存在写成运行时已安装。
14. 修改编译门禁时验证：成功状态必须带兼容指纹；源码内容/路径变化使其过期；生成目录变化不影响；编译期间源码变化不得记录成功；PostToolUse 覆盖范围不参与正确性判定。

## 禁止事项

- 不把当前加载后的行为误认为 harness 固有规则。
- 不把磁盘上存在的 workflow skill 误认为当前任务已经激活。
- 不把目标架构误认为已经落地的仓库事实。
- 不从 memory 恢复已被新 ADR 推翻的设计。
- 不把示例直接升级为项目规则。
- 不在归属不清时默认写入根 skill 或 harness。
- 不把 ToolSearch 结果、MCP 名称、plugin 存在或 provider 声明本身当作授权。
- 不让 spec 直接携带可执行门禁，也不让 skill 脚本越出其物理宿主目录。

## Skill-owned 门禁脚本编写规范

- 第三方 import 全部放函数内（模块级 import 崩溃→exit 1 被误判命中→全量锁死，2026-08-07 事故）
- main() 用 try/except 包住，未知异常打印 traceback 并 return 2
- 退出码协议：0=未命中、1=命中、2=故障；json.loads 失败 return 2（客户端载荷可能截断）
