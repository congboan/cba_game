# cba_game 治理层（Harness）

让 AI agent 在受控、可重复、可观测环境下自助闭环工作的一层。
**定义"地形"（上下文）、"裁判"（验证）、"围墙"（护栏），不定义"路线"（具体需求方案）。**

> **规范状态**：本文件是治理模型、宿主归属、生命周期和扩展方式的唯一正式定义。
> 治理模型版本：`dynamic-hosted-constraints/v2`。

## 治理认知契约

任何治理编写任务都必须先遵循项目治理入口，由对话或 spec 构建层选择适用 workflow，完成治理模型重建
与宿主归属判断。Harness 不知道具体 authoring skill 的名称、用途或目标路径，也不替任何 skill 选择、
激活或修改 selector。

治理层由三类对象组成：

- **机制**：`harness/` 只负责发现、解析、调度、求值、验证与 hook 协议，不拥有项目约束实例。
- **实例**：可选 root skill、workflow skill、spec 分别提供不同生命周期的动态约束；root/workflow skill
  还可物理拥有复杂门禁脚本。plugin 开发约束也是 workflow skill，不建立第四种实例类型。
- **状态**：state 只保存当前 stage、active spec、显式 active skills 等运行指针，不承载规范。

动态治理必须区分三个阶段：

- **发现（discover）**：宿主存在，机制能够解析它；发现不代表当前任务采用它。
- **激活（activate）**：root skill 存在时常驻；其他 workflow skill 由当前任务、confirmed spec 或对话选择。
- **刷新（refresh）**：机器求值每次重新读取磁盘；AI 已加载的上下文需要显式重载才能撤销旧版本。

> **当前实现状态**：任务级过滤已落地。scope_guard 不选择或激活 skill，只读取既有任务状态；
> root skill 存在时始终生效；缺失时进入非阻断初始化提示，已存在但损坏时终止任务。workflow skill 仅在 active spec 的 `required_skills` 或 state 的
> `active_skills` 中出现时生效，spec 约束仅在该 spec 被 `active_spec` 指向时生效。plugin-owned
> development skill 通过 junction 暴露后仍按普通 workflow skill 激活，不因 plugin 存在或被使用而生效。

不可破坏的不变量：

1. 同一具体约束只能有一个宿主，不得复制到 AGENTS、harness 或其他宿主形成双重真相源；constraint ID
   在同一宿主及当前 effective 集合中必须唯一。
2. 约束声明、项目数据、skill-owned 门禁脚本和规则测试跟随同一宿主；删除宿主即完整回收。
3. 当前加载后的行为不等于 harness 固有规则，判断时必须追踪来源和生命周期。
4. memory 是历史记录，示例只解释 schema；两者都不是现行规范或授权来源。
5. 目标架构不自动等于当前实现，未确认内容不得升级为约束。
6. 只有跨项目稳定的发现、所有权校验、调度、超时和退出协议进入 harness；复杂项目/工作流门禁逻辑
   留在所属 root/workflow skill，spec 不直接执行代码。
7. 无法唯一确定宿主时先询问用户，不得默认写入 harness 或根 skill。
8. ToolSearch 只发现客户端工具；工具 provider 只声明语义和 capability 映射；两者都不是授权来源。

### 宿主归属

| 宿主 | 拥有内容 | 生命周期 |
|---|---|---|
| 项目根 skill（可选） | 项目知识、配表、项目级约束、门禁脚本及其测试 | 随项目；存在时始终生效 |
| workflow skill | 某种做法、流程、约束、门禁脚本与测试；包括通过 junction 暴露的 plugin-owned skill | 随工作流；物理 skill 可随 plugin 回收 |
| active spec | 单个需求的范围、验收和爆炸半径 | 随需求 |
| state | 当前 stage、active spec 等指针 | 运行时 |
| harness | 通用机制、schema、协议和机制测试 | 跨项目稳定 |

## ⭐ 统一元原则（所有扩展点共用，先读这条）

每个可扩展点都是一个受控治理节点，满足四性：

```
机制封闭   发现、激活、所有权校验、调度、超时和裁决协议写在 Harness
数据开放   内置 evaluator 的规则值由约束宿主开放填写
受信扩展   复杂门禁只能由 root/workflow skill 自有脚本实现，不能越出物理宿主
实例生灭   声明、脚本和测试跟随宿主，宿主删除即完整回收
```

**边界纪律（命门，不可破坏）**：
- `evaluator` 只能是机制已实现的枚举值（见 `scripts/scope_guard.py` 的 `EVALUATORS`）。
- `data` 字段由该 evaluator 固定；禁止 AND/OR/嵌套/自由表达式。
- 内置 evaluator 保持原子；复杂项目逻辑使用 `script`，不得为每条项目规则扩充 Harness。
- `script` 不是任意命令入口：只接受所属 root/workflow skill 内的相对 `.py` 路径和受限 timeout；
  spec、绝对路径、`..`、自定义参数与 cwd 均不允许。
- 脚本是受信治理代码，随 skill 一起进入策略面保护；退出协议固定为 0=未命中、1=命中、其他=任务终止。

## 结构

```
harness/                     ← 机制（跨项目复用，零项目数据）
├── README.md            ← 本文件（元原则 + 扩展方法）
├── decisions.md         ← 治理层决策记录（只记 harness 底座决策，不记项目技术选型）
├── state/harness_state.json ← 运行时状态（stage / active_spec / active_skills，动态读写）
├── templates/spec.md    ← 需求 spec 模板（可带 constraints）
├── templates/root-skill.md ← 不含项目规则的 root skill 空骨架
└── scripts/             ← 机制（永久；客户端协议必须显式适配）
    ├── scope_guard.py       ★ 统一求值引擎（discover 实例 → 过滤有效宿主 → 求值/决策）
    ├── operation_normalizer.py 工具载荷 → evaluator 语义证据（不做授权）
    ├── tool_governance.py   tool provider/capability 的封闭 schema 与名称匹配（不做授权）
    ├── init_root_skill.py   root skill 非覆盖式初始化器
    ├── build_editor.py      编译执行器（调 UBT 编译，记录成功源码指纹）
    ├── source_fingerprint.py UE 编译输入内容指纹（工具无关）
    └── resolve_engine.py    引擎路径解析（环境变量 / EngineAssociation / 唯一已注册源码引擎）

.workbuddy/skills/<项目名>/    ← 根 skill（项目知识，随项目生灭）
├── SKILL.md               ← 项目级配表 + 项目级约束 + 项目架构（frontmatter + 正文）
├── checks/                ← 项目级复杂门禁脚本
└── tests/                 ← 项目规则测试

Plugins/<X>/.workbuddy/skills/<name>/  ← plugin-owned workflow skill 的物理真相源
└── SKILL.md / checks/ / tests/

.workbuddy/skills/<name>/   ← 指向上述物理目录的 junction，仅作为 WorkBuddy 发现入口
```

**三层分离**：机制（harness/，引擎+schema）／项目知识（根 skill，配表+约束+架构）／
运行时状态（state/，动态）。机制约定定位根 skill（`.workbuddy/skills/<项目名>/SKILL.md`，
项目名从 `.uproject` 推出），读其 frontmatter；代码里不含任何具体规则/参数内容。

### Root skill 三态与初始化

| 状态 | 行为 |
|---|---|
| `loaded` | 完整加载，root constraints 始终参与求值 |
| `missing` | 非阻断；继续发现和求值 workflow/spec，同时通过 context 要求 AI 告知用户并建议初始化 |
| `invalid` | 文件已存在但不可读或 frontmatter 无法解析；正常任务 `HARNESS_TASK_ABORT`；能安全定位该文件时可进入逐次人工确认的受限恢复 |

缺失 root skill 时，`--context` 返回 `root_skill_setup`，其中包含目标位置、空模板和初始化命令。
初始化器只在目标不存在时创建空骨架：

```bash
python harness/scripts/init_root_skill.py
```

它要求仓库根目录恰有一个 `.uproject` 并据此推导项目名；无法唯一确定时停止并交由人工解决项目身份，
不得引入第二个隐式项目名来源。初始化器不覆盖现有文件，也不生成任何项目事实或约束。AI 必须先告知
用户并取得确认，再执行初始化；之后继续与用户完善内容。

### 根 skill 的 frontmatter（约束数据自包含）

| 段 | 内容 | 被谁读 |
|---|---|---|
| `tool_providers` | 项目常驻的工具语义与 capability 映射；通常应优先放入按需 workflow/spec | tool governance |
| `required_tool_capabilities` | 项目始终需要可解析 provider 的 capability；通常应优先放入按需 workflow/spec | task context validation |
| `constraints` | 项目级约束（物理底线等） | discover 加载求值 |

> 每个内置 evaluator 的规则值都放在声明它的 constraint `data` 中。evaluator 不固定读取 root，也不从
> Harness 默认值补造项目知识；因此同一约束移动到 root、workflow 或 spec 后语义保持不变。当前 stage
> 等运行指针仍来自 state，但可用阶段、代码范围和命令特征都属于具体 constraint。

## 实例层（随宿主生灭）

约束声明在哪，决定它的生命周期：

| 实例类型 | 声明位置 | 生灭 |
|---|---|---|
| 项目级约束（物理底线） | 根 skill `SKILL.md` 的 frontmatter `constraints` | 随项目（根 skill） |
| 做法约束（工作流） | `.workbuddy/skills/<name>/SKILL.md` 的 frontmatter `constraints` | **删 skill 即删约束** |
| 需求约束（爆炸半径） | `specs/<id>.md` 的 frontmatter `constraints` | 随需求生灭 |

scope_guard 启动时 discover 根 skill、各 workflow skill 和 spec 中的约束候选。plugin-owned skill
通过 `.workbuddy/skills/` junction 进入同一个 workflow 发现入口，不建立 plugin 专用扫描器。发现集合负责来源
与生命周期追踪；求值循环再根据 active spec 与显式 active skills 跳过未激活实例，不能从“文件存在”
推导“当前任务已加载”。

Spec 不重复保存自身身份或当前运行阶段：`specs/<id>.md` 的 `<id>` 是唯一 spec 身份，
`harness/state/harness_state.json.stage` 是唯一当前阶段。spec frontmatter 中出现 `spec_id` 或 `stage`
属于宿主声明错误；未激活 spec 只保留诊断，被选为 active spec 后按有效宿主完整性规则 fail-closed。
`status` 仍只表示 spec 生命周期及激活资格，`required_skills` 仍表示该需求的 workflow 依赖，两者不与
上述真相源重叠。每个 spec 必须显式声明列表 `required_skills`；没有 workflow 依赖时写 `[]`。字段缺失
或容器错型同样属于 spec 宿主声明错误，服从相同生命周期。

### Plugin-owned workflow skill

- plugin 开发 skill 的唯一真相源位于 `Plugins/<X>/.workbuddy/skills/<name>/`；项目
  `.workbuddy/skills/<name>/` 必须是指向该目录的 junction，不得复制 `SKILL.md`。
- 任务明确开发、维护或重构 plugin 本身时，才通过 `required_skills` / `active_skills` 选择该 skill。
- 任务只是使用、依赖、调用或配置 plugin 时，不选择 plugin 开发 skill；相关约束由实际工作流对应的项目
  workflow skill 提供。
- plugin 业务说明保留在 plugin 自己的项目文档中，skill 按需引用；机器约束只在 skill frontmatter
  声明，复杂门禁脚本放在同一物理 skill 的 `checks/` 中。
- junction 只是可发现性投影。harness 不解析 `Plugins/<X>` 或建立特殊生命周期；`--explain` 只额外报告
  `canonical_source_file` 供人确认真实物理文件。plugin 删除时物理 skill 与 junction 一并回收。

### 任务级 skill selector

当前单 active task 模型的有效 workflow skill 集合为：

```text
active workflow skills = active_spec.required_skills  （有 active spec）
                      或 state.active_skills          （无 active spec）
```

`harness_state.json` 的 `stage`、`active_spec`、`active_skills` 都是必需字段：`stage` 必须是非空字符串，
`active_spec` 必须是字符串，`active_skills` 必须是列表。字段缺失或错型产生
`activation.state_schema_invalid` 并终止任务；Harness 可以在报告路径中使用 `null` 占位，但不得补造
`design`、空 spec 或空 skill 集合后继续运行。Harness 不固定 stage 枚举，当前 stage 是否被某条约束
识别仍由该约束自己的 `data.stages` 决定。

- `required_skills` 与 `active_skills` 的每一项只能是字符串 workflow `host_id`。不接受
  `{name: ...}` 对象或其他兼容表示，也不读取对象中的未知字段；对象项在 state 中产生 activation error，
  在 spec 中产生服从该 spec 生命周期的宿主声明错误。
- `state.active_spec` 只能是空字符串或规范引用 `specs/<id>.md`。`<id>` 必须是无首尾空白的单段文件名，
  不接受 basename 简写、盘符、额外目录、`.` / `..`、反斜杠或大小写漂移；解析成功后使用发现到的精确
  spec 文件身份。spec 不得再用 frontmatter `spec_id` 声明第二个身份。
- `state.active_spec` 是唯一选择源，spec 不能通过自身字段激活自己；但被选择的 spec 必须具有精确的
  `status: confirmed` 才具备激活资格。`draft`、`done`、缺失、非字符串或未知状态产生
  `activation.active_spec_status_invalid` 并终止任务。重新开发 `done` spec 时，先由人明确恢复为
  `confirmed`，再通过受保护 state 选择。
- 当前 stage 只来自 `harness_state.json.stage`；spec frontmatter 不得声明 `stage`。某条约束可在自身
  `data` 中定义它识别的阶段及对应语义，但不能建立第二个当前阶段指针。
- `active_spec.required_skills`：spec 驱动任务的权威依赖，必须显式存在且为列表；`[]` 表示经 spec
  构建阶段明确确认本需求没有 workflow 依赖。存在 `active_spec` 时不再合并 state 中的旧选择。
- `state.active_skills`：仅用于无 spec、由对话明确选择 workflow skill 的任务，由任务执行者维护，只保存宿主名。
- 两种 selector 来源都必须填写发现入口的规范 workflow `host_id`：只允许无首尾空白的单个目录名，禁止
  盘符、路径分隔符、`.` / `..`，并且大小写必须与 `.workbuddy/skills/<host_id>/` 的实际入口完全一致。
  Harness 使用同一解析器处理 spec、state 和 `require_active_skill`，不得依赖操作系统的路径大小写行为
  建立第二套身份。
- 未被选中的 workflow skill 仍会被 discover 以便诊断和解释，但其 constraints 不参与求值。
- state 文件不可读、JSON/根结构损坏、必需字段缺失或错型，以及 active spec、`required_skills` 或
  `active_skills` 无法解析时会产生 `activation.*` error；这是任务上下文失效，不是普通 evaluator
  故障。非规范 active spec 引用产生 `activation.active_spec_reference_invalid`，规范引用对应文件缺失才
  产生 `activation.active_spec_missing`。scope_guard 必须 fail-closed，返回 `HARNESS_TASK_ABORT` 与
  `task_context_report`，AI 必须立即结束当前任务并交由人工处理。
- 引用不存在的 skill 会产生 `activation.skill_missing`，不会把其他 skill 错当替代品，也不会继续执行剩余约束。
- 非规范名称和 root skill 引用产生 `activation.skill_reference_invalid` 并终止任务；root 存在时始终激活，
  缺失时应走初始化提示，而不是伪装成 task skill。

### 有效治理宿主完整性

约束发现和 schema 校验产生的诊断必须携带 `host_type`、`host_id`、`lifecycle` 和
`source_file`。scope_guard 使用与约束求值相同的激活判定计算 `host_effective`：

- 已加载 root skill、当前选中的 workflow skill、active spec 是当前有效宿主；这些宿主的
  `source.*` / `constraint.*` error 会 fail-closed，返回 `HARNESS_TASK_ABORT` 与
  `task_context_report`，不得进入正常 evaluator 求值。报告能从结构化诊断安全定位仓库内修复文件时，
  同时提供受限恢复目标；这不恢复原任务。
- 未激活 workflow skill 和非 active spec 的 error 只保留为可观测诊断，不影响当前任务；之后激活该
  宿主时，同一错误立即成为阻断错误。
- warning（包括兼容旧实例的 `constraint.id_missing`）不阻断，无论宿主是否有效。
- `activation.*` error 与 `root_skill.invalid` 不依赖宿主过滤，始终阻断当前任务。

因此，文件“可发现”不代表声明“可执行”；只有当前有效且通过完整性校验的宿主才能进入约束求值。

scope_guard 不提供 activate/deactivate 命令，也不写入任务状态。AI 根据任务选择并读取 skill 后，
在有 spec 时维护 `required_skills`，无 spec 时维护 `state.active_skills`；scope_guard 只做确定性过滤和求值。
`--context` / `--explain` 遇到任务上下文或有效宿主完整性失效时仍输出完整诊断 JSON，但退出码为 2；
所有 hook 求值直接 deny。

skill 不能在自身正文中决定或修改自己的生效方式。spec 的 `required_skills` 由 spec 构建阶段根据需求
确定；无 spec 的选择由对话写入 state。修改一个当前 active spec 本身属于任务切换：先通过受保护 state
退出该 spec，再由新的任务选择 authoring workflow，不能让被修改的 skill/spec 自行授权。

宿主需要机械要求“某类路径只能在指定 workflow 已激活时修改”，可声明通用
`require_active_skill` constraint。具体 skill 名、路径范围和例外都属于声明宿主的数据；Harness 只读取
当前 selector 并求值，不认识任何具体 workflow。required skill 已激活时，未解析副作用不会被该
constraint 阻断；未激活且副作用路径无法证明时，仍作为潜在绕过只拒绝当前调用。

`require_active_skill.skill` 使用与 task selector 相同的规范身份解析，必须精确解析到
`.workbuddy/skills/<host_id>/SKILL.md` 下真实可发现的 workflow skill；不能引用项目 root skill，不能
携带盘符或路径分隔符，大小写也不能漂移。缺失或非法引用产生属于声明 constraint 宿主的 error：当前
effective root/workflow/spec 立即通过 `task_context_report` 终止任务；未激活宿主只保留诊断，待其
激活时再升级为阻断。

### 意图对齐阶段的候选 skill 预检

任务/spec 构建层在写入 `required_skills` 或 `state.active_skills` 前，使用正式 selector 身份协议只读预检
候选 workflow 组合：

```bash
python harness/scripts/scope_guard.py --preview-skills workflow-a workflow-b
python harness/scripts/scope_guard.py --preview-skills
```

第二种形式表示已经明确选择空 workflow 集合。预检模拟 `root skill + 候选 workflow skills` 的 effective
集合，校验候选引用、当前会生效的宿主声明、constraint ID 冲突和 required tool capability；它不借用
当前 stage 判断候选 workflow 的阶段适配，因为当前 stage 仍属于既有运行任务，而不是候选 selector。

输出 `mode: candidate_task_preflight`。只有 `status: ready` 时，构建层才可把 `resolved_skills` 固化到
spec/state；`status: invalid` 时应在意图对齐阶段根据 `blocking_diagnostics` 处理宿主选择、ID 或能力
冲突后重新预检。命令不写 state/spec、不激活 skill、不授权任何操作。当前真实任务上下文已经失效时，
预检不会掩盖该问题，而是返回 `invalid_current_context_human_action_required` 和原始
`task_context_report`。

预检改善的是错误发现时机，不替代运行期完整性校验。人工编辑、跳过构建层或预检后宿主内容漂移时，
实际 selector 产生的同一冲突仍必须 fail-closed 并终止任务。

### 动态工具 provider 与 capability

root skill、workflow skill 和 spec 都可以在 frontmatter 声明：

```yaml
tool_providers:
  - id: workspace-file-mcp
    tool_glob: "mcp__workspace__*"
    capabilities: [workspace.file.write]
    effects:
      - event: pre_write
        operation: write
        path_field: target_file
        confidence: exact
    unresolved_events: []

required_tool_capabilities: [workspace.file.write]
```

这些声明沿用约束实例的同一宿主和生命周期：root 存在时常驻，workflow 随任务选择，spec 随
`active_spec`。provider 不属于 plugin 第四宿主；plugin 开发所需 provider 应写在对应 plugin-owned
development skill 中，普通使用 plugin 时写在实际业务 workflow skill 或 spec 中。

`tool_providers` 是封闭 schema：

- `id` 是全局稳定小写标识；`tool_glob` 对真实 `tool_name` 做区分大小写匹配；
- `capabilities` 可为空列表，只描述该工具能够向约束证明的能力；
- `effects` 把明确的 `tool_input` 点分字段映射为 `pre_tool` / `pre_write` / `pre_command` /
  `pre_commit` 语义请求；`pre_write.path_field` 必须指向仓库文件路径；不相关时显式写 `[]`；
- `unresolved_events` 声明仍无法证明的相关副作用；没有时显式写 `[]`；
- 同一真实调用只能匹配一个当前有效 provider。重叠匹配、字段缺失或错型属于有效治理源损坏，
  返回 `tool_adapter_report` 并终止任务，禁止按名称或相似字段猜测。

`required_tool_capabilities` 只校验当前任务是否至少有一个有效 provider 声明该能力，不证明客户端已经
安装或加载工具。AI 必须使用 WorkBuddy ToolSearch 确认延迟/MCP 工具的真实可用性；缺失或歧义时停止并
报告，不能回退到未受控工具。ToolSearch 只发现工具，不激活 skill、不授予 capability、也不覆盖约束。

约束需要限制“某类行为必须经指定能力入口”时，使用 `require_tool_capability` evaluator；工具名称本身
永远不构成授权。例如在 `when: pre_write` 上要求 `workspace.file.write`，则只有当前 provider 明确提供该
capability 的写语义请求才能通过该约束。

## WorkBuddy PreToolUse 协议

当前 hook 适配只面向 WorkBuddy。`.codebuddy/settings.json` 使用单一 `matcher: "*"` 捕获每个
`PreToolUse` 调用，并恰好一次路由到 `scope_guard.py --hook-stdin`。matcher 只建立静态覆盖，不判断
allow/deny，也不枚举未来工具。

所有工具先进入通用 envelope：

- payload 根节点是对象，`hook_event_name` 必须是 `PreToolUse`；
- `tool_name` 必须是非空字符串；新的合法工具名本身不是协议错误；
- `tool_input` 必须是对象；
- 顶层 `cwd` 可省略；一旦提供就必须是非空绝对路径；
- JSON、根节点、事件、工具名或 `tool_input` 结构损坏时，返回 `hook_protocol.invalid`、
  `HARNESS_TASK_ABORT` 与完整报告，终止任务交给人处理。

已验证的 WorkBuddy 文件/命令工具继续使用严格专用 adapter：

| 工具类别 | `tool_name` | 必需输入 |
|---|---|---|
| 写文件 | `Write` / `write_to_file` | `file_path` / `filePath` + `content` |
| 编辑文件 | `Edit` / `replace_in_file` | `file_path` / `filePath` + `new_string` / `newString` / `content` |
| 删除文件 | `delete_files` | `target_file` / `file_path` / `filePath` |
| 命令 | `Bash` / `execute_command` / `PowerShell` | `tool_input.command` + 顶层绝对 `cwd` |

专用 adapter 的必需字段必须存在、类型正确且别名值不冲突；相对文件路径需要绝对 `cwd`；空文件内容
合法，但“缺少内容字段”不是空内容。未知或尚未建模的工具仍保留原始 `tool_input`，再由当前有效动态
provider 显式解释；没有 provider 时标记相关语义副作用 unresolved，不按字段名猜测。无相关有效 deny
时 harness abstain；可能绕过当前有效 deny 时只拒绝本次调用。`ToolSearch` 等明确不产生当前 evaluator
所关心副作用的查询/发现工具仍产生 `pre_tool`，但不因此获得授权。

`DeferExecuteTool`、`MultiEdit`、`NotebookEdit` 等未取得真实 payload 的工具不在机制中猜测字段。
它们可以被通配 hook 捕获，但在明确 provider 或专用 adapter 落地前，其潜在相关副作用保持 unresolved。
unresolved event 不会推翻已经精确证明的无范围 `require_tool_capability`：provider 已提供所需
capability，且约束没有 `path_pattern` / `operation` 时，该约束不再作为 unresolved blocker；带范围
条件时因实际路径/操作仍可能未知，继续保守阻断。

PreToolUse 与 Stop gate 使用两级时间预算：WorkBuddy 外层 timeout 为 60 秒，scope_guard 内部所有当前
有效 evaluator 共享 45 秒求值预算，并预留 15 秒生成结构化 deny/abort 报告。内部预算耗尽返回
`HARNESS_TASK_ABORT`：发生在具体 evaluator 内时附 `evaluator_report`，发生在上下文加载或调度阶段时
附 `time_budget_report`；不得等外层强制杀死后猜测客户端会放行还是拒绝。单个 `script` 默认最多
10 秒，可声明 1..30 秒，但仍受剩余共享预算约束；超时属于 evaluator 失效并终止任务。

## 统一门禁流水线

`.codebuddy/settings.json` 只提供一个通配静态入口；后续职责全部属于 harness：

1. `scope_guard.py` 解析通用 WorkBuddy envelope；协议损坏直接终止任务，合法新工具名继续。
2. 按当前任务生命周期加载 root/workflow/spec 的 provider 与 capability requirement；匹配必须唯一。
3. `operation_normalizer.py` 只产生当前 evaluator 需要的证据：
   - 文件写/编辑/删除产生 `pre_write`；
   - 所有命令产生原始 `pre_command`；
   - 可静态识别的文件变更补充 `pre_write` 路径、`path_scope: exact|tree`、内容与操作类型；
   - 结构化删除命中当前普通目录，或 shell 明确使用递归删除时，normalizer 将目标标记为精确
     `tree`；scope_guard 在共享求值预算内、不跟随 symlink/junction 地展开当前后代，并把每个后代作为
     `operation: delete` 的精确 `pre_write` 复用现有 evaluator。后代遍历无法完成时保留 unresolved
     `pre_write`，不把目录路径本身误当成已经证明全部副作用；
   - `git commit` 补充 `pre_commit`；
   - `git add / rm / restore` 等写类子命令解析静态可证明的路径参数为精确 `pre_write`
     （`operation: write` + `path_scope: exact`），复用现有 evaluator 保护；无法静态证明的
     参数/选项/仓库外路径保持 unresolved `pre_write`。`git -C <绝对路径>` 解析为仓库根后
     走同一套归一化。只读子命令（`status / diff / log / ls-files / rev-parse / show`）只产生
     `pre_command`；远端/分支类（`pull / push / fetch / branch / remote / init / stash`）同样
     不产生 `pre_write`；无法证明目标集合的子命令（`checkout / switch / merge / rebase / reset /
     clean / mv / tag`）保留 unresolved `pre_write`；
   - 每个工具调用都产生 `pre_tool` 并携带 provider capabilities；
   - 无法可信确定的潜在副作用记录为 unresolved event。
4. scope_guard 只让当前任务的 effective root/workflow/spec deny 参与裁决。`script` 只在其
   root/workflow skill effective 且所有权校验通过时执行。工具类型本身不授予也不撤销权限；相同约束
   不按 File/Bash/PowerShell 重复声明。
5. 先求值已有精确证据，使具体约束原因优先；再判断 unresolved event 是否可能绕过当前有效 deny。
   `state_field`、`build_freshness` 这类只依赖治理状态的 evaluator 会先精确求值；状态已满足时不作为
   unresolved blocker，状态未满足时仍阻断当前调用。

normalizer 不是“所有 AI 行为”的穷举分类器，也不是命令白名单。它只为现有 evaluator 提取必要证据；
只读命令、受控 wrapper 等识别结果表示“本次未发现这些 evaluator 所关心的副作用”，不构成授权。
新门禁确实需要新证据时才扩展 adapter 或通用 evaluator。

### 四种结果

| 结果 | 条件 | WorkBuddy 响应 | AI 行为 |
|---|---|---|---|
| `abstain` | 精确证据未命中约束，且 unresolved effect 不会绕过当前有效 deny | `continue:true`，不输出 `permissionDecision:allow` | 交回 WorkBuddy 原生权限系统 |
| `deny_current_call` | 命中当前有效 deny，或 unresolved effect 可能绕过它 | `permissionDecision:deny`，不带任务终止字段 | 只放弃本次调用，改用可受控入口继续 |
| `human_confirmation` | 精确写入 Harness 控制面或动态治理策略面 | `permissionDecision:ask`，退出码 0 | 等待用户确认这一次工具调用；不得把批准扩展为持久维护模式 |
| `abort_task` | hook 协议、任务上下文、有效宿主声明或 evaluator 运行失效 | `permissionDecision:deny` + `HARNESS_TASK_ABORT` + `stopReason` / `systemMessage` | 结束正常任务并把完整报告交给人；只有 task-context 报告明确提供恢复协议且人另行发起修复时才进入受限恢复 |

unresolved 拒绝使用 `HARNESS_OPERATION_UNRESOLVED` 与 `operation_report`，不是全局默认拒绝。没有相关
effective deny 时，即使命令行为未被完整建模，harness 也必须 abstain，不能覆盖 WorkBuddy 原生权限。
例外是 Harness 自身控制面：未知 `pre_write` 无法形成范围明确的人工批准，因此只拒绝当前调用。
`require_active_skill` 命中属于普通 constraint deny，只拒绝当前调用；它表示外部任务选择未满足宿主
声明的前置条件，不是 Harness 协议或 evaluator 故障。

### 硬门禁成立条件

一条动态约束只有同时满足以下条件，才能称为 WorkBuddy 当前客户端范围内的硬门禁：

1. 客户端真实工具调用被通配 matcher 捕获并恰好一次进入 scope_guard；
2. 通用 envelope 可可信解析；需要具体语义时有已验证专用 adapter 或唯一有效动态 provider；
3. normalizer 能提供 evaluator 所需证据，或能识别其相关副作用仍 unresolved；
4. 约束宿主在当前任务 effective；
5. 若使用 `script`，脚本位于所属 skill 物理目录内并遵守固定退出协议；
6. WorkBuddy 执行 `permissionDecision:deny`。

缺少任一条件都只能称为认知约束或覆盖缺口，不能写成“已经硬拦截”。该模型不替代 OS 沙箱、可执行
文件签名、PATH 完整性或客户端之外的行为审计。`--context` 的 `enforcement_contract` 是当前适配器、
语义证据、unresolved 策略和四种结果的机器可读视图。

### Harness 控制面自保护

项目 hook 配置 `.codebuddy/settings.json`、可能以更高项目优先级覆盖它的
`.codebuddy/settings.local.json`，以及 `harness/scripts/` 下全部可执行机制和依赖，是 Harness 的
项目级信任基。该清单属于跨项目框架机制，不由 root/workflow/spec 声明，也不能由 skill 激活状态关闭。

能够精确归一化到这些文件、目录后代或其删除祖先的 `pre_write`，由 Harness 返回
`permissionDecision:ask`，要求用户逐次确认。批准只作用于当前工具调用，不写入仓库维护开关；非交互
客户端会把 ask 转为拒绝。存在 unresolved `pre_write` 时无法向用户展示可信修改范围，因此只拒绝当前
调用并要求改用结构化文件工具或可静态分析的命令。

这项保护只覆盖被当前 WorkBuddy hook 捕获的 AI 工具调用；不替代操作系统权限、CodeBuddy 的原生受保护
路径、`/hooks` 配置审查或客户端外部修改检测。

### 动态治理策略面完整性

Harness 机制不可篡改仍不够：`harness/state/harness_state.json` 决定阶段及 skill/spec 激活，
`.workbuddy/skills/` 承载 root/workflow 声明、数据与门禁脚本。两者始终属于策略面，精确修改或删除其祖先路径
时逐次 `permissionDecision:ask`。skill 通过 junction 暴露时，Harness 同时保护发现入口和已发现 junction
的物理目标；从 plugin 物理目录直接修改不能绕过。

active spec 的生命周期不同：未激活 spec 是普通草稿，Harness abstain；一旦被
`harness_state.json.active_spec` 选中，它的文件就动态进入策略面，后续修改逐次 ask。修改 state
本身同样 ask，因此 AI 不能先清空 `active_skills`、切换 stage 或替换 active spec 再绕过约束。
内置 evaluator 逻辑存在于受控制面保护的 Harness 中；复杂门禁逻辑可以存在于受策略面保护的
root/workflow skill 脚本中。junction 的物理目录保护同时覆盖脚本，不能从 plugin 路径绕过。

`init_root_skill.py` 是受信包装器，但在目标 root skill 尚不存在时，normalizer 会显式产生目标明确的
`pre_write`，因此初始化仍需用户批准；目标已存在时初始化器自身拒绝覆盖，不产生虚假的维护授权。
无法解析写入范围的工具会同时把策略面列为 unresolved blocker，只拒绝当前调用。

“未激活 spec 是普通草稿”只描述是否需要策略面逐次 ask；它仍可能命中当前 effective 宿主声明的
`require_active_skill`、路径或其他约束。active spec 则在这些约束之外继续逐次 ask。

### 任务上下文损坏后的受限恢复

当前有效 root/workflow/spec、state 或 selector 的静态完整性错误仍然终止正常任务。为了避免“修复治理
需要工具、所有工具又先要求治理有效”的自锁，`task_context_report` 会从阻断诊断动态派生：

- `repair_targets`：声明错误的治理源，以及能由有效 script 约束安全计算出的缺失门禁脚本；
- `inspection_roots`：只读检查范围，包括损坏宿主、固定治理说明，以及当前 selector 已选择的 workflow；
- `recovery.status`：存在安全目标时为 `restricted_human_confirmation_available`，否则要求 hook 外人工处理。

受限恢复不是 repository maintenance flag，也不是第五种授权结果。原任务已经结束，只有用户随后明确
发起治理修复时，AI 才能使用以下封闭入口：

1. 使用已从真实 WorkBuddy 会话确认 `tool_input.file_path` 的 `Read`，且目标必须位于
   `inspection_roots`；
2. 运行受信 `scope_guard.py` 做只读 context/explain/预检或重新校验；
3. 使用已验证的结构化 Write/Edit 精确写入一个 `repair_target`，Harness 返回
   `permissionDecision:ask`，由用户逐次确认。

删除、移动、复制、shell 写入、未知工具、unresolved 副作用、越界路径以及其他正常任务调用继续
`HARNESS_TASK_ABORT`。恢复写入后必须重新运行 `--context`；只有上下文重新有效，正常求值流水线才恢复。
Hook stdin 已不是合法 JSON、scope_guard 本身无法启动，或诊断无法安全推导仓库内目标时，Harness
无法识别调用范围，必须在 hook 外由人修复，不能 fail-open 猜测。

### 编译新鲜度门禁

`PostToolUse` 自动编译只是即时反馈，不是编译门禁的正确性来源。`build_editor.py` 在 UBT 编译前后分别
计算 UE 编译输入内容指纹；两次一致且编译成功时，才把 `success:true` 与
`source_fingerprint` 写入 `Saved/harness_last_build.json`。编译过程中源码变化会使本次结果失效。

编译执行采用独立的内外时间预算：WorkBuddy PostToolUse timeout 为 1860 秒，`build_editor.py` 的整个
生命周期共享 1800 秒预算，预留 60 秒用于清理、状态和报告。执行器启动时先用原子替换写入
`starting/running + success:false`，成功状态只能在编译和指纹验证全部完成后原子写入。Windows 下 UBT
进入带 `KILL_ON_JOB_CLOSE` 的 Job Object；内部超时会显式清理进程树，客户端取消或父进程退出时句柄
关闭也会由 OS 清理子树。编译输出使用随句柄关闭删除的临时文件，避免取消后留下每次运行的临时目录。

指纹覆盖根 `.uproject`、`Source/` / `Plugins/` 中的 C/C++ 头源文件、`.Build.cs` / `.Target.cs` 等
C# 构建规则和 `.uplugin`，并排除 `Intermediate`、`Binaries`、`Saved` 等生成目录。路径和内容都参与
SHA-256；时间戳不作为正确性证据。

`build_freshness` evaluator 在 stop/commit 时重新计算指纹。状态缺失、上次编译失败、旧状态没有指纹
或当前指纹不同都会命中 deny；读取当前编译输入失败属于 evaluator 机制故障并终止任务。这样修改来自
文件工具、PowerShell、Bash、MCP 或客户端外部编辑时，旧的 `success:true` 都不能继续使用。

有效且 `action: deny` 的 `build_freshness` 实例还会动态保留其 `data.file`：所有能够精确归一化到该
路径的 `pre_write`（写入、删除、移动或覆盖），以及删除/移动包含它的祖先目录，均只拒绝当前调用；
副作用包含 unresolved `pre_write` 的工具也会因为可能篡改证明而只拒绝当前调用。保护路径不在
Harness 中硬编码，随 root/workflow/spec 宿主的激活生命周期生灭。证明生成器在受信包装器进程内部
原子更新文件，不会产生新的 WorkBuddy 文件工具调用，因此不需要绕过或白名单。

## 统一约束 schema

```yaml
- id:        <稳定唯一标识> # 用于来源追踪、重复检测和测试
  evaluator: <封闭枚举>     # 求值器类型（机制里有对应函数）
  when:      <封闭枚举>     # pre_tool | pre_write | pre_command | pre_commit | stop
  data:      {...}          # 该 evaluator 的输入结构（开放填值，结构固定）
  action:    deny|warn      # 默认 deny
  reason:    "<拦截提示>"    # 自由文本，无逻辑
```

`when` 描述 evaluator 需要哪类语义证据，不等同于某个客户端工具名。比如 `pre_write` 既可来自
WorkBuddy 文件工具，也可来自 PowerShell/Bash 的可识别文件操作。

当前 evaluator 枚举由 7 个内置声明式原子求值器和 1 个受控脚本求值器组成：`path_glob`（路径命中）、
`command_write`（命令文本中的项目写入特征）、`state_field`（状态文件字段）、`path_writable_stage`
（阶段写权限）、`require_tool_capability`（工具能力路由）、`require_active_skill`（任务 workflow
前置条件）、`build_freshness`（成功编译源码新鲜度）和 `script`（skill 自有复杂门禁）。

### evaluator 速查（data 字段 + 语义）

| evaluator | data 字段 | 含义 / 命中条件 |
|---|---|---|
| `path_glob` | `{pattern, invert?}` | 目标文件路径命中 `pattern` glob 即命中；`invert:true` 则"不命中才算违规"（用于"只许改某范围"） |
| `command_write` | `{asset_patterns, write_hints, read_hints}` | `pre_command` 原始命令同时命中任一 `asset_patterns` 与任一 `write_hints` 时命中；三个字段都由当前 constraint 自有，`read_hints` 明确列出已知纯读特征但不覆盖同一命令中已出现的写特征。它不是 Harness 授权白名单 |
| `state_field` | `{file, field, equals, missing_equals?}` | 读取 `file`（严格的仓库内相对 JSON 状态路径），其 `field` 字段值等于 `equals` 即命中；只有文件确实不存在时才使用 `missing_equals`（不指定则缺文件放行）。文件存在但不可读、JSON/根结构损坏或字段缺失均属于 evaluator 失效并终止任务 |
| `build_freshness` | `{file}` | `file` 中没有对应当前 UE 编译输入指纹的成功编译结果即命中；用于 stop/pre_commit |
| `path_writable_stage` | `{stages, code:{exts, areas}}` | 当前 stage（只读 `state/harness_state.json`）在本 constraint 的 `stages` 中 `writable:false`，且目标命中本 constraint 的 `code.exts` 与 `code.areas` 时命中；当前 stage 未声明或数据损坏时终止任务 |
| `require_tool_capability` | `{capability, path_pattern?, operation?}` | 在该 `when` 的适用请求上，当前唯一 provider 未提供 `capability` 即命中；可按路径/操作缩小范围 |
| `require_active_skill` | `{skill, path_patterns, exempt_patterns?}` | `skill` 必须是可发现且非 root 的 workflow；目标命中任一 `path_patterns` 且未命中例外时，外部 task selector 未激活它即命中；只检查选择结果，不激活 skill |
| `script` | `{script, timeout?}` | 执行所属 root/workflow skill 物理目录内的相对 `.py` 门禁；stdin 接收 ctx JSON，exit 0=未命中、1=命中、其他=任务终止；默认 10 秒，允许 1..30 秒且受共享预算限制 |

> 填 `data` 时严格按上表字段，不多不少；字段语义与判定逻辑见 `scripts/scope_guard.py` 对应 `EVALUATORS` 函数。旧实例缺少 `id` 时仍兼容，但 context 会给出诊断。

## 如何扩展（统一添加方式）

| 要添加 | 添加到哪 | 动机制? |
|---|---|---|
| 一条项目级约束 | 根 skill `SKILL.md` 的 frontmatter `constraints`（填数据） | 否 |
| 一个工作流（可带约束） | `.workbuddy/skills/<name>/SKILL.md`（frontmatter + 薄壳） | 否 |
| 一个动态/MCP 工具的语义和能力 | 所属 root/workflow/spec 的 `tool_providers`；使用真实 `tool_name`/payload 字段 | 否 |
| 一个任务必须具备的工具能力 | 所属 root/workflow/spec 的 `required_tool_capabilities` | 否 |
| 一个复杂项目/工作流门禁 | 所属 root/workflow skill 的 `checks/*.py` + frontmatter `script` 约束 | 否 |
| 一类新的跨项目稳定原子语义（极罕见） | `scope_guard.py` 加封闭 evaluator 函数 + 登记 EVALUATORS，并补机制测试 | 是（加函数） |
| WorkBuddy 通用 envelope 变化或需要内置专用 adapter | `scope_guard.py` 严格解析 + `operation_normalizer.py` 证据适配；通配 matcher 不按工具扩展 | 是 |
| evaluator 所需的阶段/代码定义/命令特征 | 声明该约束的 root/workflow/spec 中对应 constraint 的 `data` | 否（数据） |
| 当前开发阶段/进行中 spec/显式 task skill | `state/harness_state.json` 的 `stage`/`active_spec`/`active_skills` | 否（运行时状态） |

## 认知与来源查询

```bash
# 当前治理摘要：任务 skill、发现/有效约束数量与诊断
python harness/scripts/scope_guard.py --context

# 完整解释：逐条列出 id、宿主、来源文件、生命周期、激活原因和 evaluator
python harness/scripts/scope_guard.py --explain
```

`--context` 用于会话和治理任务的认知启动；`--explain` 用于新增、移动、删除约束前的来源与重复检查。
二者都是只读操作。`constraints_discovered` 是完整来源目录，`constraints_effective` 和
`constraints_evaluation_candidates` 是当前任务真正参与求值的集合；二者不得混用。
同一宿主内重复 constraint ID 是该宿主声明错误；不同宿主的同名 ID 只有同时 effective 才产生
`activation.effective_constraint_id_duplicate` 并终止任务。Spec 的 `required_skills` 负责在意图对齐后
选择哪些 workflow 同时生效，不会合并或覆盖它们的 constraint。
`tool_governance` 同时报告 provider/requirement 的 discovered/effective 数量、当前候选 provider 和
`runtime_availability: unverified_use_toolsearch`；声明可解析不等于客户端已安装。

## 约束选型决策树

需要加一条新的门禁？先按以下优先级判断，不满足才往下一级走。

1. **限写某路径** → `path_glob`（写内置求值器行的第一条约束）
2. **限命令文本中的某类写入** → `command_write`；若需要跨工具路径语义，优先 `path_glob`
3. **依赖某个 JSON 状态文件的值** → `state_field`
4. **要求成功编译对应当前源码** → `build_freshness`
5. **取决于开发阶段** → `path_writable_stage`（如 design 禁写）
6. **某类行为必须使用具备指定能力的工具** → `require_tool_capability`
7. **某类路径要求任务已选择指定 workflow skill** → `require_active_skill`
8. **以上都不满足，且逻辑随项目/工作流生灭** → 所属 skill 的 `script`
9. **缺失的是跨项目稳定的原子机制能力** → 讨论后扩展内置 evaluator

> 内置 evaluator 集合应保持收敛。单个项目的复杂门禁逻辑不搬进 Harness。

## skill-owned script 门禁协议

`script` 是受信治理扩展，不是普通命令执行器：

- 只允许 root/workflow skill 声明；spec 需要复杂门禁时通过 `required_skills` 引用 workflow skill。
- `data.script` 必须是相对所属 skill 物理目录的 `.py` 路径，不得包含空段、`.`、`..`、盘符或绝对路径。
- junction-backed skill 按 `SKILL.md` 的真实物理目录解析，脚本 realpath 必须仍在该目录内。
- schema 不提供 `args` 或 `cwd`；Harness 使用当前 Python 的 `-I -B` 和所属 skill 目录作为工作目录。
- 脚本从 stdin 读取一次 JSON；exit 0 表示未命中，exit 1 表示命中，其他退出码、缺失、超时或启动失败
  均返回 `HARNESS_TASK_ABORT` 并交给人。静态发现阶段确认脚本缺失时，task-context 报告可把声明
  `SKILL.md` 与可安全计算出的缺失脚本列为受限修复目标；这不把脚本路径固化进 Harness。
- 脚本是治理策略面的一部分；修改 root/workflow skill 树或 junction 的物理 skill 目录时逐次 ask。

```yaml
- id: workflow.example.check
  evaluator: script
  when: pre_write
  data:
    script: "checks/check_example.py"
    timeout: 10
  action: deny
  reason: "命中工作流门禁"
```

stdin JSON 的稳定字段为：`path`、`content`、`command`、`operation`、`source_tool`、
`tool_capabilities`、`tool_provider_ids`、`confidence`、`event`。脚本不得依赖 Harness 私有字段。
命中时可在 stdout 输出具体原因；约束实例显式 `reason` 仍优先。若 normalizer 无法可信提取脚本所需
事件证据，脚本不会在未知数据上猜测，而是沿用统一 unresolved 语义拒绝当前调用。

## 质量门禁分层

### 机制测试约定（测试套件尚未建立）

未来的 `harness/tests/` 只测试通用机制，使用临时 `TestProject` 构造动态宿主，不固化真实项目规则：

```bash
python -B -m unittest discover -s harness/tests -v
```

项目级或工作流规则的测试放在所属 skill 的 `tests/` 下，确保规则与测试随宿主一起回收。

- **AI 层**（`.codebuddy/settings.json`，即时反馈）：SessionStart→context；
  PreToolUse(WorkBuddy 全部工具通配捕获)→单次 scope_guard→有效 provider/capability→normalizer 产生所需语义事件；
  PostToolUse(文件工具)→build_editor 在受管进程树和独立时间预算内即时编译；
  Stop/commit→`build_freshness` 比较当前源码指纹。
  PostToolUse 不覆盖所有写入入口只影响即时反馈，不再形成门禁缺口。
- **系统层**（`.githooks/`，硬，最终防线）：pre-commit 编译+禁区、commit-msg 格式。
  ⏳ 待办：项目 `git init` 后建立（阶段 2）。

## 降级

hook 未生效（未信任/不支持）时，机制脚本仍可手动/CI 调用；AGENTS.md 提供认知入口和降级加载顺序。
