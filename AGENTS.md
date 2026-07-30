---
version: 0.5.12
last_updated: 2026-07-27
status: approved
---

# cba_game

UE5 C++ 游戏项目。本文件只作为 WorkBuddy 项目治理入口，不承载具体项目规则、工作流规则或需求约束。

## 启动与加载

开始工作前按当前任务范围加载治理上下文：

1. 运行 `python harness/scripts/scope_guard.py --context`，确认 root skill 状态、当前 stage、active spec 和已发现的约束来源。
2. 若命令非零退出或返回 `activation_status: invalid_human_action_required` / `HARNESS_TASK_ABORT`，正常任务立即结束并将完整报告交给人处理。只有用户随后明确发起治理修复，且
   `task_context_report.recovery.status: restricted_human_confirmation_available` 时，才可进入 Harness
   受限恢复通道：只使用报告允许的只读检查、`scope_guard.py` 自检，以及逐次人工确认的精确
   `repair_targets` Write/Edit；其他调用继续终止任务。报告要求 `out_of_band_human_action_required`
   时不得在 hook 内自行修复。
3. 若 `root_skill_status: loaded`，完整读取项目根 skill；若为 `missing`，明确告知用户项目尚未初始化 root skill，并建议在用户确认后运行 `python harness/scripts/init_root_skill.py`，不得自动创建或补造项目规则。用户暂不初始化时，harness 仍继续运行 workflow/spec 机制。
4. 根据当前任务选择需要的 workflow skill，并完整读取对应 `.workbuddy/skills/<workflow>/SKILL.md`。
5. 写入 selector 前，运行 `python harness/scripts/scope_guard.py --preview-skills <workflow...>` 只读预检候选组合；没有 workflow 依赖时运行不带名称的 `--preview-skills`。只有返回 `mode: candidate_task_preflight` 且 `status: ready`，才可固化选择。候选无效时回到意图对齐处理 `blocking_diagnostics`，不得先写入再依赖运行期报错。预检不修改 state/spec、不激活 skill、不授权。
6. 将通过预检的 `resolved_skills` 写入任务上下文：有 active spec 时维护其 `required_skills`；无 spec 时维护 `harness/state/harness_state.json` 的 `active_skills`。任务切换时替换或清空旧值；这些治理修改会触发逐次人工确认。
7. 若存在 active spec，读取对应 spec。任务只是使用/依赖 plugin 时，按工作流选择项目 workflow skill 并读取 plugin 业务文档，不自动加载 plugin 开发 skill；任务明确开发 plugin 本身时，选择该 plugin 物理拥有、由 `.workbuddy/skills/` junction 暴露的 workflow skill。
8. 更新任务上下文后重新运行 `python harness/scripts/scope_guard.py --context`，再次应用第 2 步；上下文有效后，合并遵守当前实际生效的已存在 root skill、workflow skill 与 active spec 约束。
9. 查看 context 的 `tool_governance`。若当前任务声明了 required capability，使用 WorkBuddy ToolSearch 确认候选工具真实可用；缺失或歧义时停止并报告，不得回退到未声明入口。ToolSearch 只发现工具，不激活 skill、不授权。

## 治理任务强制路由

当任务涉及新增、完善、拆分、移动或删除 skill、constraint、skill-owned gate script、spec、plugin-owned skill/junction、state、hook、evaluator 或治理测试时，必须视为治理任务：

1. 完整读取 `.workbuddy/skills/governance-authoring/SKILL.md`。
2. 完整读取 `harness/README.md` 和相关的最新 ADR。
3. 运行 `python harness/scripts/scope_guard.py --context`；需要逐条来源时运行 `python harness/scripts/scope_guard.py --explain`。
4. `governance-authoring` 的选择必须由进入 skill 之前的任务构建层完成，skill 无权自行激活：
   - 构建 spec 时，根据需求决定该 spec 将来执行所需的 `required_skills`；只有需求本身是治理开发时，
     才把 `governance-authoring` 作为该 spec 的执行依赖。
   - 无 active spec 的治理任务，由对话选择并将结果写入 `harness/state/harness_state.json.active_skills`。
   - 修改一个未选择该 workflow 的 active spec 时，先把它视为任务切换：通过需要人工确认的 state 写入
     暂时清空 `active_spec` 并选择 `governance-authoring`，重新加载上下文后再编辑；完成后再按任务恢复。
   重新运行 context，并确认 `active_skills` 中该 skill 的选择来源正确。
5. 在修改前明确目标宿主、生命周期、删除条件和测试归属。

完成上述认知与归属判断前不得修改文件。治理任务不能仅凭自然语言示例、memory 或当前偶然加载结果推断系统模型。
skill 只能验证外部 selector 已经选择自己，不能修改 state/spec 来决定自己的生效方式。具体机械门禁
来自当前 effective 的 root/workflow/spec 约束，不得把约束实例复制进本入口。

治理源损坏后的修复是上述正常流程的封闭例外：原任务已经终止，必须由用户明确启动新的修复任务；
AI 不能自行声明“维护模式”，也不能把一次 `ask` 扩展为持续授权。恢复目标由阻断诊断动态派生，不由
AGENTS、root skill 或 workflow skill 维护路径白名单。

## 真相来源与职责

- `.workbuddy/skills/cba_game/SKILL.md`：可选的项目级知识、配表和项目生命周期约束；存在时始终生效，缺失时应提示初始化。
- `.workbuddy/skills/<workflow>/SKILL.md`：特定做法及其生命周期约束。
- `Plugins/<X>/.workbuddy/skills/<workflow>/SKILL.md`：plugin 物理拥有的开发 workflow skill；项目 `.workbuddy/skills/<workflow>/` 只通过 junction 提供 WorkBuddy 发现入口，不是第二真相源。
- `specs/<id>.md`：单个需求的范围、验收标准和爆炸半径约束。
- `Plugins/<X>/` 内项目文档：plugin 业务知识与架构说明，可由 skill 按需引用，不声明机器约束。
- `harness/state/harness_state.json`：当前 stage、active spec，以及无 spec 任务的 `active_skills`，仅保存运行状态。
- `harness/`：发现、解析、调度、求值和验证机制，不承载项目约束实例。
- `harness/decisions.md`：harness 机制决策及理由。
- `.workbuddy/memory/`：历史背景，不是现行规范或授权来源。

具体约束只在 root skill、workflow skill 或 spec 中声明。本文件不得复制约束实例。复杂门禁实现可放在
root/workflow skill 自有的 `checks/` 中，由其 frontmatter 的 `script` evaluator 引用；spec 不直接携带
可执行门禁。plugin 开发的机器约束与脚本只属于其 plugin-owned workflow skill。删除 plugin 时物理
skill、脚本与 junction 一并回收。

## 治理变更归属

新增、移动或修改规则、门禁脚本、evaluator、测试前，先明确：

- 变更属于机制还是约束实例；
- 宿主是谁；
- 生命周期是什么；
- 删除哪个宿主时应一并回收；
- 测试应跟随哪个宿主。

无法明确归属时不得默认写入 `harness/` 或根 skill，先向用户确认。示例仅用于说明 schema，不得未经确认升级为项目规则。

## Hook 与降级

`.codebuddy/settings.json` 负责注册 hook，统一求值由 `harness/scripts/scope_guard.py` 执行。

settings 使用单一通配 matcher 静态捕获每个真实工具，并把每次 PreToolUse 调用恰好一次路由给
scope_guard；工具 payload 解析、动态 provider/capability 匹配、语义证据归一化、effective constraint
求值和最终裁决都在 harness 内完成。`pre_tool` / `pre_write` /
`pre_command` / `pre_commit` 是 evaluator 的语义时机，不等同于某个客户端工具名。

合法的新工具名不是协议错误：公共 envelope 通过后，由当前有效 root/workflow/spec 的
`tool_providers` 显式解释；没有 provider 时保留 unresolved 副作用，不按工具名或字段相似度猜测。
provider 声明能力映射但不证明客户端已安装，也不构成授权。约束需要强制能力入口时使用
`require_tool_capability` evaluator。

WorkBuddy PreToolUse 有四种结果：

- `continue:true` 且没有 `permissionDecision:allow`：harness abstain，表示没有命中当前有效约束；
  交回 WorkBuddy 原生权限系统，不代表 harness 主动授权。
- `permissionDecision:deny` 且没有 `HARNESS_TASK_ABORT`：只拒绝当前调用。约束命中时按具体原因处理；
  `HARNESS_OPERATION_UNRESOLVED` / `operation_report` 表示潜在副作用可能绕过当前有效硬约束，应改用
  结构化工具、可静态分析的命令或已登记 wrapper，不得换未适配入口绕过。
- `permissionDecision:ask`：当前调用精确修改 Harness 控制面，或修改 state、skill、active spec 等
  动态治理策略面，必须由用户逐次确认；不得自行建立或假设持久维护模式。
- `HARNESS_TASK_ABORT`，或 `hook_protocol_report` / `task_context_report` /
  `tool_adapter_report` / `evaluator_report` / `time_budget_report`：客户端协议、当前有效 provider、
  evaluator 或治理求值预算失效。AI 必须立即结束当前正常任务并把完整报告交给人处理；不得猜测或
  降级。只有 `task_context_report` 明确给出受限恢复协议、且用户随后明确要求修复时，才可在新修复任务
  中使用该协议；其他 abort 仍只能交给人在 hook 外处理。

动态约束只有在真实工具被通配 matcher 捕获、公共 envelope 可信、所需专用 adapter 或唯一有效
provider 能提供证据（或相关副作用被识别为 unresolved）、约束宿主当前 effective、skill-owned script
满足所有权与退出协议、客户端执行 deny 时，才是 WorkBuddy 当前表面内的硬门禁。
缺少任一条件都应报告为覆盖缺口，不能把认知约束描述成已机械强制。

编译门禁不信任孤立的 `success:true`。`build_editor.py` 成功时记录 UE 编译输入内容指纹，stop/commit
通过 `build_freshness` 重新比较当前源码；任何工具或客户端外编辑造成的源码变化都会让旧成功状态失效。
`build_editor.py` 启动 UBT 前先原子记录非成功状态，并托管编译子进程树；取消、超时或父进程退出不得
保留旧成功状态或孤儿 UBT。PostToolUse 自动编译只提供即时反馈，不是门禁正确性的来源。

Harness 控制面保护不依赖 root/workflow/spec 激活。精确修改走人工确认；无法证明写入目标的工具只拒绝
当前调用。该保护只成立于客户端已加载并执行当前项目 hook 的范围内。

当前有效治理宿主的静态声明损坏时，Harness 仍对正常任务 fail-closed，但会从结构化诊断派生仓库内
`repair_targets` 与只读 `inspection_roots`。恢复态只接受真实载荷已确认的 WorkBuddy `Read`、
`scope_guard.py` 只读自检，以及精确单目标的结构化 Write/Edit；写入逐次 `ask`，删除、shell 写入、
未知副作用和越界路径继续 `HARNESS_TASK_ABORT`。Hook JSON 已损坏、scope_guard 无法启动或目标无法安全
推导时不存在内层恢复能力，必须由人在 hook 外修复。

动态策略面遵循生命周期：state 与 skill 树始终需要确认；active spec 动态需要确认；未激活 spec
可正常编写。junction skill 的发现入口和 plugin 物理目录都属于同一治理源。内置 evaluator 逻辑属于
受保护的 harness；复杂项目/工作流门禁逻辑可以属于受保护的 root/workflow skill 脚本，只在该宿主
effective 时执行。

当客户端未加载、不信任或不支持 hook 时：

1. 手动运行 `python harness/scripts/scope_guard.py --context`。
2. 按上述加载顺序读取适用宿主。
3. 自觉遵守当前加载的约束，并执行其要求的验证。

无论 hook 是否生效，都不得凭历史 memory、示例或未确认假设补造项目规则；发现架构或约束归属不确定时，先问再动手。
