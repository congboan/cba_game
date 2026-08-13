# 治理层（Harness）决策记录（ADR 简式）

只记录 **harness 这个治理底座本身**的决策及理由（跨项目稳定、不漂移）。
**不记录项目的技术选型/架构路线**（那是"做法"，会漂移，属 skill/知识层，不进本文件）。
新决策追加在顶部。

---

## 2026-08-13 · build_editor 证明有效时跳过 UBT

**决策**：build_editor.py 算完当前指纹后，若上次成功且指纹一致，直接打印 skip 返回 0，不启动 UBT；新增 --force 强制重建。

**理由**：PostToolUse 对每次文件工具都触发编译，hook 截断又强制分段小写入，N 次编辑带来 N 次完整 UBT；指纹匹配已证明当前源码即上次成功编译的源码，重复编译无证明增益。

**边界**：快路径不修改证明文件；失败或无指纹仍走完整编译。--force 供人工重建。本决策不改 PostToolUse 即时反馈定位。

---

## 2026-08-06 · 纯渲染/任务管理工具补入无建模副作用集合

**决策**：`operation_normalizer.py` 的 `TOOLS_WITH_NO_MODELED_EFFECTS` 补充
`read_me`、`show_widget`、`TaskCreate`、`TaskUpdate`。

**理由**：`read_me`/`show_widget` 是 WorkBuddy 纯渲染/内联可视化工具，只向
会话流输出 SVG/HTML 片段，不写仓库文件、不执行命令、不触发 git；`TaskCreate`/
`TaskUpdate` 只改会话内任务清单，与已收录的 TaskGet/TaskList 同类。此前它们
因未被建模而被保守判为 unresolved pre_write，导致会话内无法使用可视化与任务
管理，属于工具覆盖缺口而非安全风险。补入集合后仍产生 pre_tool，仍受动态
tool capability 与 WorkBuddy 原生权限约束。

**边界**：只扩展"该工具当前调用不产生现有 evaluator 关心的 repository 副作用"
的静态集合；不新增授权、不扩大命令白名单。集合语义见注释（仍产生 pre_tool）。

---


**决策**：WorkBuddy PreToolUse hook 对长 Write/Edit payload 的 stdin JSON 序列化
会在约 441-797 列截断，产生 hook_payload.invalid_json 与 HARNESS_TASK_ABORT。
截断发生在 scope_guard 收到 stdin 之前，harness 无法在机制内修复或规避。

规避准则（认知约束，非机械门禁）：
- 长文件写入/编辑用分段小操作，每段 payload 控制在约 200 字符内；
- 命令行避免长中文参数（UTF-8 多字节加剧截断），改用短参数或从文件读；
- 触发 invalid_json abort 后不得降级放行或换未适配入口绕过，按既有协议交回。

**理由**：hook stdin 是治理门禁的信任边界（见 2026-07-23 协议错误 ADR）。
客户端序列化截断属于公共 envelope 损坏，abort 语义正确且必须保留。
真实修复点在 WorkBuddy 客户端的 stdin 序列化，不在 harness 侧。

**边界**：本决策只记录协议边界与认知规避，不新增约束实例、不扩大白名单、
不授权任何绕过入口。分段写入是任务执行规范（写入根 skill 认知区），不是
harness 可以机械强制的门禁。

---

## 2026-08-05 · git 命令归一化改为精确副作用派生并支持 `-C`

**决策**：`operation_normalizer.py` 的 `_git_analysis` 改为**按子命令精确派生副作用证据**。

只读子命令（status/diff/log/ls-files/rev-parse/show）只产生 pre_command；
写类子命令（add/rm/restore/commit）解析静态可证明的路径为精确 pre_write，
无法证明的形态保持 unresolved；commit 仍恰好一次 pre_commit。

远端/分支类（pull/push/fetch/branch/remote/init/stash）只产生 pre_command；
无法证明目标集合的（checkout/switch/merge/rebase/reset/clean/mv/tag）保留
unresolved pre_write；git -C 绝对路径解析为仓库根后走同一套归一化。

GIT_NON_WORKTREE_SUBCOMMANDS 移除 add/rm（原被标无副作用放行，git rm 实际
删除工作区文件，属虚假放行漏洞），reset 移入 unresolved 集合。

理由：git add 被 unresolved 保守拒绝，同一机制又把 git rm 的删除副作用虚假
放行——二元分类既过度拒绝又过度放行。按子命令精确派生让 git 写操作获得与
Write/Edit 相同的保护语义。

边界：只描述 git 语义证据归一化，不扩大白名单、不新增授权；git 写 .git/ 仍受
OS 权限/沙箱约束，harness 不替代 OS 文件锁。未建模的合法新子命令保持
unresolved，由真实需求驱动扩展。

---

## 2026-07-28 · 递归目录删除使用精确 tree 副作用并复用现有 evaluator

**决策**：`pre_write` 路径证据增加 `path_scope: exact|tree`。结构化删除的当前目标是普通目录，或
PowerShell/POSIX shell 明确使用递归删除参数时，`operation_normalizer.py` 将该删除声明为精确
`tree`，而不是只报告目录节点或把所有递归删除统一降级为未知调用。

`scope_guard.py` 先求值目录根请求，以保留具体路径约束、证明文件和控制面/治理面的既有优先级；根请求
未被拒绝后，在同一 45 秒预算内遍历当前逻辑后代，为每个文件或目录派生
`operation: delete + path_scope: exact` 的 `pre_write` 请求并复用当前 effective evaluator。遍历不跟随
symlink/junction，避免把删除链接入口误述为删除物理宿主。目标不存在、位于项目外或是链接入口时不补造
后代；读取目录失败时保留 unresolved `pre_write`，由现有“仅在可能绕过有效 deny 时拒绝当前调用”
策略裁决；预算耗尽仍按既有 `time_budget_report` 终止任务。

**理由**：只把 `Remove-Item Source -Recurse` 归一化为“删除 `Source`”会让按后代文件扩展名或 glob
求值的约束看不到 `Source/**/*.cpp`；同样会让资产、描述符和 skill-owned script 失去实际目标。把所有
递归删除直接标成 unresolved 又会无差别拒绝空目录，并破坏精确删除治理树时原有的逐次人工确认语义。
tree 证据与后代派生让现有约束继续拥有规则含义，Harness 只提供跨项目稳定的作用域展开机制。

**边界**：该机制只描述 hook 执行前当前可观察到的目录树，不替代 OS 文件锁、并发修改隔离或客户端外
行为审计。具体哪些后代应拒绝仍完全来自当前 effective root/workflow/spec 约束；Harness 不硬编码
`Source`、`Content`、资产扩展名或项目目录。

---

## 2026-07-27 · 无效任务上下文保留受限、人工确认的治理修复通道

**决策**：当前有效 root/workflow/spec、state 或 selector 的静态完整性错误继续
`HARNESS_TASK_ABORT`，正常任务不得求值或继续。`task_context_report` 同时从结构化阻断诊断动态派生
仓库内 `repair_targets`；`inspection_roots` 只包含损坏宿主、固定治理说明和当前 selector 已选择的
workflow，以便修复任务重建必要认知而不读取普通项目内容。对于所有权路径本身合法但文件缺失的
skill-owned script，还可由 constraint 与物理宿主安全计算缺失脚本目标，不解析自然语言错误消息。

用户在收到报告后明确发起治理修复时，PreToolUse 进入封闭恢复裁决：真实会话已确认字段的 WorkBuddy
`Read` 只能读取 `inspection_roots`；受信 `scope_guard.py` 可以做只读自检；已验证结构化 Write/Edit
必须精确命中单个 `repair_target`，且每次返回 `permissionDecision:ask`。删除、移动、复制、shell 写入、
未知或 unresolved 副作用、越界路径和其他调用仍然 abort。恢复没有持久开关，skill/spec/state 不能
激活或扩展它；修改后必须重新运行 context，只有完整性恢复后才退出恢复态。

**理由**：把“正常任务必须因治理源损坏而停止”解释为“任何修复工具也永远不能运行”，会形成自举死锁；
反过来在上下文损坏时放行任意工具，又会把 fail-closed 变成全局绕过。由错误本身给出最小目标，并把
唯一写权限交给用户对当前调用的逐次确认，可以保留人可修复性而不建立 AI 可自启的维护模式。

**边界**：该通道只处理 scope_guard 已经启动、公共 hook envelope 可可信解析且目标能安全限定在仓库内
的 task-context 静态错误。Hook JSON 损坏、scope_guard 启动/语法错误、目标无法安全推导，以及仅在
evaluator 运行时才出现且尚未形成静态诊断的故障，仍必须在 hook 外人工处理。本决策不新增项目约束，
也不恢复或补造任何缺失的项目门禁内容。

---

## 2026-07-27 · 候选 workflow 在意图对齐阶段只读预检

**决策**：Harness 提供 `scope_guard.py --preview-skills [<host_id> ...]`，供任务/spec 构建层在写入
`required_skills` 或 `state.active_skills` 前模拟 `root skill + 候选 workflow skills` 的 effective
集合。预检复用正式 workflow 身份解析、宿主生命周期、constraint ID 和 tool capability 校验；空参数
表示显式候选空集合。它不修改 selector、不激活 skill、不授权，也不以当前运行任务的 stage 判断候选
workflow 的阶段适配。只有 `status: ready` 才应固化 `resolved_skills`。

候选自身无效返回普通 preflight `invalid`，由意图对齐阶段处理，不伪装为一个已经激活任务的
`HARNESS_TASK_ABORT`。但当前真实任务上下文本来已经失效时，预检必须保留原有
`task_context_report` 并停止，不能成为绕过运行期 fail-closed 的入口。实际 selector 的运行期完整性
校验继续保留，负责捕获人工编辑、跳过预检以及预检后的宿主漂移。

**理由**：workflow 组合及其冲突本来就是任务意图的一部分。把首次发现点放在 selector 写入后，会把
可在对话中解决的选择问题升级为治理任务终止；完全删除运行期校验又无法防止漂移。只读预检把正常冲突
前移到意图对齐，同时保持 selector 权威、skill 无自激活权和运行期最终防线。

---

## 2026-07-27 · Constraint ID 按宿主与 effective 集合校验

**决策**：constraint ID 有两层唯一性。单一 root/workflow/spec 宿主内出现同一 ID，属于该宿主自身的
`constraint.id_duplicate` 声明错误，继续服从宿主生命周期。跨宿主出现同名 ID 在 discover 阶段可以共存；
只有这些宿主在当前任务中同时 effective 时，Harness 才产生不附属于任一单一宿主的
`activation.effective_constraint_id_duplicate` 并 fail-closed，报告所有冲突宿主和来源文件。

Spec 的意图对齐阶段通过 `required_skills` 选择当前会并存的 workflow skill；它不合并、覆盖或重命名
已声明 constraint。若同时 effective 的同名规则确实不同，必须改用不同 ID；若确实是同一个长期规则，
必须选择唯一共同宿主并删除其他复制。

**理由**：全局 discover 顺序查重会让一个未激活宿主的声明影响当前任务，并使结果依赖目录排序；反之，
完全忽略当前有效集合内的同名又会留下双重身份。两层校验既保留单宿主的声明完整性，也使跨宿主冲突只在
实际共同参与裁决时终止任务，符合 `discovered ≠ effective`。

---

## 2026-07-24 · Workflow skill selector 只接受字符串 host_id

**决策**：spec `required_skills` 与 state `active_skills` 的列表项只接受字符串 workflow `host_id`。
删除历史兼容的 `{name: <host_id>}` 对象格式；Harness 不再提取对象的 `name`，也不静默忽略对象中的
其他字段。state 中的非字符串项产生 `activation.skill_reference_invalid` 并 fail-closed；spec 中的
非字符串项产生 `spec.required_skill_reference_invalid`，未激活时只保留宿主诊断，激活后随有效宿主
完整性规则 fail-closed。

字符串仍必须通过既有规范身份解析：非空、无首尾空白、单段、精确大小写，并解析到可发现的非 root
workflow skill。本决策只收敛引用的表示形式，不改变 skill 的选择权或激活生命周期。

**理由**：对象兼容会建立未文档化的第二套 selector schema，并让 AI 误以为对象中的额外字段具有治理
语义；实际实现却会静默忽略它们。只保留字符串 host_id 后，state、spec、README 和身份解析器使用同一
表示，错误输入不会被猜测迁移。

---

## 2026-07-24 · Spec 必须显式声明 required_skills

**决策**：每个 spec frontmatter 都必须显式包含列表 `required_skills`；没有 workflow 依赖时使用
`required_skills: []`。字段缺失产生 `spec.required_skills_missing`，容器错型产生
`spec.required_skills_invalid`。两者都是 spec 宿主声明错误：未激活 spec 只保留诊断，被
`state.active_spec` 选中后随有效宿主完整性规则 fail-closed。

Active spec 解析不再把缺失字段的 `None` 猜成空依赖。规范引用列表中的具体宿主身份仍使用既有 workflow
skill 解析器验证；本决策不让 skill 自行加入 selector，也不要求任何 spec 必须依赖至少一个 skill。

**理由**：`required_skills` 是 spec 驱动任务的唯一 workflow selector。把字段缺失与显式 `[]` 合并，会
在 spec 损坏时无诊断地撤销全部 workflow skill 及其约束。显式空列表保留“确实没有依赖”的合法表达，
同时让字段丢失按治理源损坏处理。

---

## 2026-07-24 · State 必需运行指针缺失时禁止补造默认值

**决策**：`harness_state.json` 必须显式包含 `stage`、`active_spec`、`active_skills`。`stage` 必须是
非空字符串，`active_spec` 必须是字符串，`active_skills` 必须是列表；具体 active spec 与 skill 引用
继续交给既有规范身份解析器验证。字段缺失或错型产生 `activation.state_schema_invalid` 并
fail-closed。读取、JSON 或根结构已经失效时，内部只可使用 `null` 占位生成完整报告，不能把占位值当成
有效任务上下文。

Harness 不再为结构完整但字段缺失的 state 补造 `stage: design`、空 active spec 或空 active skills。
Harness 也不固定 stage 枚举；某个当前 stage 是否可被采用，仍由当前有效 constraint 自有的
`data.stages` 配置决定。

**理由**：state 是当前任务运行指针的唯一真相源。静默默认会让 Harness 成为第二真相源，并可能在 state
损坏时改变阶段、退出 active spec 或撤销 workflow skill，从而让相应动态约束无诊断地失效。结构错误
必须终止任务并交给人恢复真实状态，不能由机制猜测原意。

---

## 2026-07-24 · Spec 身份与当前阶段各自只有一个真相源

**决策**：spec 的规范文件名 `specs/<id>.md` 是其唯一身份，frontmatter 不再声明 `spec_id`；
`harness/state/harness_state.json.stage` 是当前运行阶段的唯一来源，spec frontmatter 不再声明
`stage`。Harness 在发现 spec 时把这两个字段视为宿主声明错误，并附带该 spec 的宿主身份和生命周期：
未激活 spec 只保留诊断，不影响无关任务；该 spec 被 `state.active_spec` 选中后，同一错误随有效宿主
完整性规则 fail-closed，终止任务交给人迁移。

`status` 继续只表示 spec 生命周期和激活资格，不能自行选择 spec；`state.active_spec` 仍是唯一
selector。`required_skills` 继续属于需求依赖，约束 `data` 也可声明自身识别的阶段配置；它们都不建立
第二个 spec 身份或当前阶段指针。

**理由**：同时保留文件名与 `spec_id`、state stage 与 spec stage，会允许两个值彼此矛盾，并让 AI 在
构建、读取或评估需求时选择不同真相源。身份和运行指针收敛后，Spec 只承载需求范围、验收、依赖和
生命周期信息，state 只承载当前任务运行状态；宿主生命周期诊断则让旧草稿可迁移而不污染无关任务。

---

## 2026-07-24 · active spec 只有 confirmed 状态具备激活资格

**决策**：`harness_state.json.active_spec` 仍是 spec 的唯一 selector，spec 不能通过自身 frontmatter
决定激活；但 selector 指向的 spec 必须具有精确字符串 `status: confirmed` 才具备激活资格。
`draft`、`done`、缺失、非字符串或未知状态产生 `activation.active_spec_status_invalid`，作为任务上下文
错误 fail-closed。状态不合格的 spec 不解析其 `required_skills`、不进入 effective 宿主集合，也不得
回退到 state 遗留的 `active_skills`。

`done` spec 需要重新开发时，先由人明确把生命周期恢复为 `confirmed`，再通过受保护 state 选择；
修改 `status` 本身不构成激活或授权，最终 selector 写入仍需逐次人工确认。context 对有效 active spec
显式报告 `active_spec_status: confirmed`。

**理由**：仅验证文件身份会让 draft、已完成或状态损坏的 spec 提前/再次施加约束与 workflow 依赖；
反过来让 status 自身成为 selector 又会形成 spec 自我授权。将 state 选择与 confirmed 资格分离，可以
同时保留外部激活权和生命周期完整性。

---

## 2026-07-24 · active spec 指针使用唯一规范文件身份

**决策**：`harness_state.json.active_spec` 只能是空字符串或精确的 `specs/<id>.md`。`<id>` 必须是
无首尾空白的单段文件名；basename 简写、盘符、额外目录、`.` / `..`、反斜杠、扩展名漂移及仅依赖
文件系统大小写折叠才能命中的引用均非法。解析器按 `specs/` 实际发现到的精确文件名建立 spec 身份，
不再对任意输入取 basename 或做路径归一化后猜测目标。

非规范引用产生 `activation.active_spec_reference_invalid`；格式规范但目标不存在时产生
`activation.active_spec_missing`。两者都是任务上下文错误并 fail-closed；无效 active spec 不得回退到
state 中遗留的 `active_skills`。本决策只封闭指针身份；激活资格由上方状态 ADR 定义。

**理由**：basename 回退会把 `../foo.md`、外部绝对路径或任意前缀静默解释为 `specs/foo.md`，使错误
指针激活另一个需求的约束和 workflow skill。唯一规范身份让 state、spec 激活、约束过滤与策略面保护
指向同一文件。

---

## 2026-07-24 · workflow skill 引用使用唯一规范宿主身份

**决策**：active spec 的 `required_skills`、state 的 `active_skills` 与 evaluator
`require_active_skill.skill` 统一通过同一个 workflow skill 引用解析器。有效引用只能是
`.workbuddy/skills/<host_id>/SKILL.md` 的无首尾空白、单段、精确大小写宿主名；盘符、路径分隔符、
`.` / `..`、root skill 及仅依赖文件系统大小写折叠才能命中的名称均非法。解析成功后只把发现到的
规范 `host_id` 带入 task context、激活过滤和 evaluator 求值。

selector 中引用缺失 workflow skill 产生 `activation.skill_missing`；引用形式、大小写或宿主类型非法
产生 `activation.skill_reference_invalid`。二者都是任务上下文错误并 fail-closed。constraint 中的同类
错误继续归属于声明宿主，服从其 effective 生命周期。

**理由**：如果 selector 只验证路径存在却保留原始字符串，而激活过滤按发现到的目录名比较，同一个
skill 会出现“已选择但未生效”的双重身份；Windows 的大小写折叠还会让结果随运行平台变化。共享规范
解析器让发现、选择、激活和前置条件使用同一个宿主身份。

---

## 2026-07-24 · workflow 前置条件使用通用 require_active_skill evaluator

**决策**：Harness 新增原子 evaluator `require_active_skill`，固定数据结构为
`{skill, path_patterns, exempt_patterns?}`。它只在目标路径属于声明范围且外部 task selector 没有激活
指定 workflow skill 时命中；不选择、不加载、不激活 skill，也不修改 state 或 spec。

具体 skill 名、目标路径、例外和 reason 必须由 root/workflow/spec 约束实例声明。Harness 代码、contract
和 context 不得硬编码任何具体 authoring workflow。skill 正文同样无权决定自己的生效方式：spec 的
`required_skills` 由 spec 构建阶段确定，无 spec 的任务选择来自对话和 state。

声明中的 `skill` 必须在 context 阶段解析到可发现的 workflow `SKILL.md`；盘符、路径分隔符、越界引用、
缺失目标和项目 root skill 均视为声明错误。诊断归属于声明该 constraint 的宿主，因此当前 effective
宿主 fail-closed，未激活 workflow/spec 只保留诊断。

无法解析实际路径的 `pre_write` 在 required skill 未激活时保留该 constraint 为潜在绕过 blocker；skill
已经激活时，该前置条件已被 selector 证明，不再因为未知路径阻断。精确 state 修改仍服从既有策略面
逐次确认；是否把 state 排除在某条 workflow 前置约束之外由该约束宿主的数据决定。

**理由**：把具体 workflow 名和治理路径写入 scope_guard 会让 Harness 从框架退化为项目策略宿主，并
制造 skill 自我激活的循环依赖。通用 evaluator 保留机械强制能力，同时让规则数据、生命周期和删除条件
跟随声明宿主。

---

## 2026-07-24 · evaluator 规则数据跟随声明它的 constraint

**决策**：`path_writable_stage` 与 `command_write` 不再固定读取 root skill 顶层的
`stages` / `code` / `command_hints`，也不再使用 Harness 内置默认值。它们改用封闭且自包含的
constraint data：

- `path_writable_stage`: `{stages, code:{exts, areas}}`
- `command_write`: `{asset_patterns, write_hints, read_hints}`

当前 stage 仍是 `harness_state.json` 的运行指针，但可用阶段及权限、代码范围和命令特征全部属于声明
constraint 的 root/workflow/spec 宿主。任何宿主都能声明这两个 evaluator，移动约束不改变语义。

Harness 在 context 阶段验证列表、阶段配置、`writable` 布尔值、代码扩展名和相对目录前缀；当前 stage
未被该 constraint 声明时，诊断继续服从宿主生命周期：有效宿主 fail-closed，未激活宿主只保留诊断。
运行时再次验证同一协议，缺失或损坏不允许降级。根 scope 只保留运行状态和 root 加载状态，不再承载
项目规则数据。

**理由**：宿主顶层共享配表使 constraint 存在隐藏依赖，并让 workflow/spec 声明错误地读取 root 数据；
静默默认值又把项目知识复制进 Harness。自包含 data 让声明、数据、激活和回收使用同一生命周期，也让
schema 能在 evaluator 执行前完整证明约束可求值。

**替代关系**：本决策取代历史 ADR 中由 root scope 统一提供 `stages` / `code` / `command_hints` 的现行
语义；旧记录仅保留为演进背景。

---

## 2026-07-24 · `state_field` 严格区分状态缺失与治理状态损坏

**决策**：`state_field.data.file` 必须是无盘符、非绝对、不含空段/`.`/`..` 且物理解析后仍位于项目内的
相对路径；`field` 必须是非空字符串。以上声明错误随宿主生命周期进入 schema 诊断：当前有效宿主
fail-closed，未激活宿主只保留诊断。

运行时只有 `FileNotFoundError` 被视为“状态文件不存在”，并按声明的 `missing_equals` 求值；文件存在但
不可读、无法解析为 JSON、根节点不是对象或缺少声明字段，都属于当前有效 evaluator 失效，沿统一
`evaluator.runtime_error` 协议返回 `HARNESS_TASK_ABORT` 并交给人处理。显式
`missing_equals: null` 与未声明该字段不再混为一谈。

**理由**：状态不存在可以是约束宿主明确建模的业务状态，状态损坏则意味着门禁失去可信输入，不能伪装
成“不命中”继续执行。路径边界和字段结构属于跨项目稳定的原子 evaluator 协议，应由 Harness 验证，而
不是由每个 skill 重复实现。

**边界**：本决策不规定任何项目状态文件、字段和值，也不增加约束实例；具体状态语义仍完全由
root/workflow/spec 宿主声明。

---

## 2026-07-24 · 恢复 skill-owned script 作为动态门禁实现

**决策**：恢复 `script` evaluator。root/workflow skill 可以在自身物理目录的 `checks/` 中拥有 Python
门禁脚本，并由约束声明 `data: {script: <owner-relative .py>, timeout?: 1..30}`。脚本只在其宿主当前
effective 时执行；spec 不直接执行代码，需要复杂门禁时通过 `required_skills` 引用 workflow skill。

Harness 对该扩展点保持封闭协议：`script` 必须是无盘符、非绝对、不含空段/`.`/`..` 的相对 `.py`
路径；解析 junction 后的 realpath 必须仍位于声明约束的 `SKILL.md` 物理目录内。schema 不接受 `args`
或 `cwd`。Harness 使用当前 Python 的 `-I -B`、宿主目录 cwd 和稳定 stdin JSON 调用脚本；exit 0 表示
未命中，exit 1 表示命中，其他退出码、缺失、越界、启动失败或超时均视为当前有效治理源失效，返回
`HARNESS_TASK_ABORT` 交给人。单脚本默认 10 秒、上限 30 秒，并受 45 秒 gate 共享预算约束。

根 skill 的硬编码引擎路径规则恢复为 `checks/ban_engine_path.py`；删除仅为替代它而加入的
`content_contains_any` evaluator。脚本与声明、数据和测试随同一 skill 生灭；项目 skill 树始终受策略
面 ask 保护，junction 的物理目录保护同时覆盖 plugin-owned 脚本。

**理由**：在本治理模型中，skill 脚本是被 Hook 捕获后由 scope_guard 动态加载的受信门禁代码，不是 AI
通过 shell 任意执行的任务脚本。Hook 是拦截入口，scope_guard 是发现/激活/所有权/调度/裁决框架，skill
脚本实现随项目或做法生命周期变化的具体规则。禁止全部宿主脚本会迫使项目检查进入 Harness，使框架随
项目膨胀，并破坏“删除 skill 即回收知识、约束和门禁实现”的语义。真正需要封闭的是脚本的宿主所有权、
调用协议和失败策略，而不是取消这个扩展点。

**信任边界**：脚本与 Harness 内置 evaluator 一样属于治理信任基，能够在 scope_guard 进程权限内运行；
`-I -B` 和路径约束不是 OS 沙箱。其可修改性由 skill 策略面逐次人工确认控制。若未来要求在不信任
skill 代码的条件下执行，必须另行设计 OS 级隔离，不能把 timeout 描述成沙箱。

**替代关系**：本决策直接推翻紧邻下方“约束求值器禁止执行宿主提供的任意代码”ADR，并恢复
2026-07-22 script 扩展方向，同时以上述所有权和退出协议取代旧版可配置路径/参数/cwd 的宽泛语义。

---

## 2026-07-24 · 约束求值器禁止执行宿主提供的任意代码

**决策**：移除通用 `script` evaluator，不再允许 root/workflow/spec 通过约束数据指定可执行文件、参数、
工作目录或超时。Harness 的 evaluator 集合只包含机制内实现的封闭声明式原子语义；本次新增
`content_contains_any`，以固定 schema `{patterns, case_sensitive?}` 检查 `pre_write` 的可信新内容。
具体禁止片段仍由约束宿主拥有。若当前 evaluator 无法表达某条规则，只有缺失语义本身跨项目稳定、原子且
能由可信上下文封闭求值时才扩展 Harness；否则先讨论，不能宣称为硬门禁。

根 skill 的硬编码引擎路径规则迁移为 `content_contains_any` 数据，原
`.workbuddy/skills/cba_game/checks/ban_engine_path.py` 删除。动态策略面不再发现或保护“当前有效脚本”，
因为约束宿主不再拥有可执行逻辑；skill 树本身和 active spec 的既有生命周期保护保持不变。

**理由**：由 `scope_guard.py` 启动的宿主脚本是当前 hook 调用内部的普通子进程，可以用与 agent 相同的
文件和进程权限产生任意副作用，却不会形成新的 WorkBuddy 工具调用，也不会再次经过 PreToolUse。
超时只能限制运行时长，不是权限沙箱，因而破坏“所有硬门禁来自客户端 hook、Harness 逻辑封闭、实例只
开放数据”的边界。把字面内容检查收敛为纯函数 evaluator 后，项目数据仍随 skill 生命周期生灭，但不再
获得执行能力。

**替代关系**：本决策取代 2026-07-22 “引入 script evaluator”以及后续 ADR 中关于 script 文件动态保护、
单脚本 timeout 和共享 script 预算的现行语义；旧记录保留为历史背景，不再作为实现依据。

---

## 2026-07-23 · 动态治理策略面修改采用生命周期感知的逐次确认

**决策**：`harness/state/harness_state.json` 与 `.workbuddy/skills/` 始终属于动态治理策略面；精确写入、
覆盖、移动、删除或删除祖先路径时返回 `permissionDecision:ask`。skill junction 同时按逻辑发现入口和
当前已发现的物理目录保护，防止从 plugin 物理路径绕过。

当前 active spec 文件与当前有效 `script` evaluator 文件随任务生命周期动态加入策略面。未激活 spec
保持可正常编写；修改 state 以激活、切换或清空 spec/skills 本身需要逐次确认。未知副作用包含
unresolved `pre_write` 时不请求范围模糊的批准，而是拒绝当前调用。

`init_root_skill.py` 在 root skill 缺失时由 normalizer 产生精确 `pre_write` 目标，沿用同一 ask 协议；
root 已存在时初始化器只读检查并自行拒绝覆盖。

**理由**：只保护 `scope_guard.py` 等机制代码仍允许 AI 改写状态、删除 skill、扩大 active spec 或让
evaluator 永不命中。动态宿主的生命周期既决定约束何时生效，也必须决定其内容何时成为受保护治理源。
全部 `specs/` 永久锁定会污染正常设计流程，因此只保护 active spec；skill 本身是可复用治理源，始终要求
人确认。

---

## 2026-07-23 · Harness 控制面修改采用逐次人工确认

**决策**：`.codebuddy/settings.json`、`.codebuddy/settings.local.json` 与整个 `harness/scripts/` 是
项目 Harness 的固定信任基。精确归一化的写入、覆盖、移动、删除以及删除其祖先目录时，PreToolUse 返回
`permissionDecision:ask` 且进程退出码为 0，由 WorkBuddy 对本次调用强制弹出人工确认；非交互环境按
客户端语义转为拒绝。

不设置仓库内 maintenance flag，也不允许 root/workflow/spec 激活状态取消该保护。未知工具或命令仅能
证明存在 unresolved `pre_write` 时，不向用户请求范围模糊的批准，而是拒绝当前调用并要求换用目标明确
的受控入口。控制面清单和当前策略同时进入 `--context`，供 SessionStart 的 AI 认知。

**理由**：编译证明文件即使不可直接伪造，只要 AI 能无提示改写 hook 路由、求值器、归一化器、指纹器或
编译包装器，下一次调用仍可绕过全部门禁。仓库内可写的维护开关同样能由 AI 自行开启，不能构成人工授权。
WorkBuddy 官方 PreToolUse `ask` 在普通权限规则与 permission mode 之前生效，既保留 AI 协助维护 Harness
的能力，也把放宽信任基的决定留给人。

**边界**：这是 WorkBuddy 当前 hook 表面内的项目控制面保护，不声称替代 OS 文件权限、客户端原生
`.codebuddy` 保护、配置快照/`/hooks` 审查或客户端外部篡改检测。

---

## 2026-07-23 · 有效编译门禁动态保留其证明文件

**决策**：Harness 从当前激活且 `action: deny` 的 `build_freshness` 实例动态取得 `data.file`，将其视为
该门禁的证明文件。归一化后的直接 `pre_write` 命中该文件，或删除/移动包含它的祖先目录时拒绝当前
调用；工具存在 unresolved `pre_write` 时，也因无法证明不会篡改有效证明而拒绝当前调用。以上拒绝都不
终止 AI 任务，执行者应改用生成该证明的受信 Harness 包装器。

保护不硬编码 `Saved` 或任何项目文件名，随 root/workflow/spec 宿主的激活生命周期生灭；`warn` 实例不
升级为硬保护。`build_editor.py` 在自身进程内完成原子状态写入，不会形成独立 WorkBuddy 文件工具调用，
因此正常编译生命周期不需要特殊客户端白名单。

**理由**：内容指纹只能证明“记录的指纹等于当前输入”，不能证明记录确由真实 UBT 成功产生。如果普通
文件工具、Shell 或动态工具可以直接改写状态文件，就能伪造 `success:true` 与当前指纹，在未编译时通过
stop/commit。证明完整性必须由使用该证明的有效硬门禁自动派生，不能依赖另一个项目 skill 规则重复声明。

---

## 2026-07-23 · PostToolUse 编译采用原子状态与受管进程树

**决策**：WorkBuddy PostToolUse timeout 从 600 秒改为 1860 秒；`build_editor.py` 的发现、指纹、UBT、
诊断恢复和最终状态共享 1800 秒内部生命周期预算，预留 60 秒用于进程树清理、状态写入和用户报告。

编译执行器在任何项目/引擎检查和 UBT 启动前，先通过同目录临时文件 + `os.replace` 原子写入
`starting + success:false`；取得编译前指纹后更新为 `running + success:false`。只有 UBT 成功、编译
期间输入未变化且最终状态原子写入成功时，才记录 `status:success + success:true`。因此客户端取消、
内部超时、状态写入失败或父进程异常退出都不会留下可被误认成当前运行结果的旧成功状态。

Windows 下每次 UBT 运行创建启用 `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` 的 Job Object，并在启动后立即
把 UBT 加入；正常完成后关闭 job，内部超时/异常显式关闭 job 并等待清理，父 Python 被客户端终止时
OS 关闭 job handle 并终止仍存活的 UBT 子树。无 job 的异常路径使用 `taskkill /T /F` 回退；非 Windows
使用独立 session/process group。stdout/stderr 改用 delete-on-close 临时文件，不再创建可能因取消而
遗留的临时目录。

**理由**：原外层 600 秒会先于内部 UBT 1800 秒终止 Python，导致超时状态无法落盘，并可能留下 UBT
或编译子进程。单纯延长 settings 仍不能处理用户取消和父进程异常；必须由执行器同时拥有状态失效、
子进程树和内部总预算，外层只提供更大的最终包络。

**边界**：PostToolUse 仍只是即时反馈。stop/commit 的正确性继续由 `build_freshness` 对当前源码内容
指纹求值；PostToolUse 的 matcher 覆盖范围不参与门禁正确性证明。

---

## 2026-07-23 · WorkBuddy gate 使用内外两级时间预算

**决策**：PreToolUse 与 Stop 的 WorkBuddy 外层 timeout 统一为 60 秒；scope_guard 从开始加载治理上下文
起，为一次 gate 调用建立 45 秒内部总预算，给结构化 deny/abort 输出预留 15 秒。当前调用涉及的全部
effective evaluator 共享这一总预算，不能把每个 script 的上限相加后突破外层 timeout。

`script` evaluator 的默认 timeout 改为 10 秒，声明值必须是 1..30 的整数秒；超限声明属于有效宿主
完整性错误，在 context 阶段 fail-closed。运行时单脚本超时或共享总预算耗尽属于 evaluator/治理裁判
失效，返回 `HARNESS_TASK_ABORT`；总预算在 evaluator 之外耗尽时返回 `time_budget_report`。源码指纹
遍历与内容读取也接收同一截止时间，避免大型项目在生成报告前被客户端直接杀死。

**理由**：原配置给整个 PreToolUse 15 秒，而 script 默认可运行 30 秒，且多个 evaluator 串行时间无
总上限；外层可能先终止 scope_guard，使 harness 无法返回三态裁决或人工处理报告。内层预算必须严格
小于客户端 timeout，并覆盖发现、解析、全部 evaluator 与指纹计算，才能保证超时也走明确的
fail-closed 语义。

**边界**：PostToolUse 的编译执行属于即时反馈，不使用这套 gate 预算；其超时与进程管理另行决策。

---

## 2026-07-23 · 编译门禁以源码内容指纹证明新鲜度

**决策**：`Saved/harness_last_build.json` 中单独的 `success:true` 不再足以通过 stop/commit。
`build_editor.py` 在 UBT 编译前后分别计算 UE 编译输入的稳定内容指纹；只有编译成功且两次指纹一致，
才记录成功状态与 `source_fingerprint`。编译过程中输入变化时，本次编译结果判为无效并要求重跑。

新增封闭原子 evaluator `build_freshness`，读取指定状态文件并重新计算当前指纹。状态缺失、编译失败、
旧状态没有指纹、指纹模型不兼容或内容不一致都命中 deny；无法完整读取当前输入属于 evaluator 机制故障，
沿用 `HARNESS_TASK_ABORT`。项目是否要求 stop/commit 新鲜编译仍由 root/workflow/spec 约束实例声明，
harness 只提供跨项目机制。

对于无法解析具体副作用的工具调用，`build_freshness` 与 `state_field` 一样先按当前治理状态精确求值：
状态已经满足时不作为 unresolved blocker；状态未满足时仍阻断当前调用；求值机制故障仍终止任务。

指纹模型覆盖根 `.uproject`、`Source/` / `Plugins/` 下的 C/C++/Objective-C++ 源头文件、C# Build/Target
规则和 `.uplugin`，排除 `Intermediate`、`Binaries`、`Saved` 等生成目录；排序后的相对路径和文件内容
共同进入 SHA-256，mtime 不作为正确性证据。

**理由**：`PostToolUse` 只匹配文件工具时，PowerShell、Bash、MCP 或客户端外部编辑可能在上次成功编译
之后改变源码，却保留旧的 `success:true`。把更多工具名加入 PostToolUse 仍会受客户端表面变化影响，
并会让查询工具误触发编译。内容指纹直接验证“当前源码是否就是成功编译过的源码”，与修改入口无关。

**边界**：PostToolUse 自动编译保留为即时反馈和便利，不再承担门禁正确性。旧的无指纹成功状态必须重跑
一次 `build_editor.py` 才能迁移；这是有意 fail-closed，不做时间戳兼容放行。

---

## 2026-07-23 · 通配捕获与动态 tool provider/capability

**决策**：`.codebuddy/settings.json` 对 WorkBuddy `PreToolUse` 只保留一个 `matcher: "*"`，每个真实工具
调用恰好一次进入 `scope_guard.py --hook-stdin`。静态配置只负责覆盖客户端入口，不再按已知工具名维护
捕获列表；合法的新 `tool_name` 进入通用 envelope，不因未知名称成为协议错误。

通用 envelope 固定验证根对象、`hook_event_name: PreToolUse`、非空 `tool_name`、对象
`tool_input`，以及存在时必须为绝对路径的顶层 `cwd`。JSON 或这些公共结构损坏仍返回
`hook_protocol.invalid` / `HARNESS_TASK_ABORT`，交由人工处理。已确认的文件/命令工具继续使用专用
adapter；未知工具没有可凭名称猜测的语义。

动态和 MCP 工具通过 root skill、workflow skill 或 spec frontmatter 的 `tool_providers` 声明：

- provider 使用区分大小写的 `tool_glob` 匹配真实工具名，并声明 `capabilities`、封闭 `effects` 字段映射
  与 `unresolved_events`；
- provider 与 `required_tool_capabilities` 复用既有 root/workflow/spec 生命周期和唯一激活判定；
- 同一调用只能匹配一个当前有效 provider。冲突或 provider 无法按显式字段映射解释真实 payload，
  返回 `tool_adapter_report` 并终止任务；
- capability 只有 provider 能提供，工具名称本身不提供；`require_tool_capability` evaluator 用于限制
  某类语义行为必须走具备指定能力的入口；
- requirement 能解析到 provider 只证明治理映射存在，不证明客户端已安装工具。AI 使用 ToolSearch
  确认可用性；ToolSearch 不激活 skill、不授权，也不改变门禁。

无有效 provider 的未知工具把相关副作用标记 unresolved：没有可能被绕过的 effective deny 时
abstain；存在时只拒绝当前调用。`DeferExecuteTool` 等未获得真实 payload 的工具不得在机制中猜字段。
若唯一有效 provider 已精确提供 capability，则无 `path_pattern` / `operation` 限定的
`require_tool_capability` 已被证明满足，不再仅因同一事件还有其他 unresolved 副作用而成为 blocker；
带范围限定时仍因未知副作用范围不明而保守阻断。

**理由**：WorkBuddy 的内置、延迟与 MCP 工具集合会变化。按工具名扩展 settings 会使静态覆盖落后，
而按名称猜测行为会制造虚假硬门禁。通配入口闭合“能否进入裁判”，动态 provider 则让“如何理解工具”
跟随真正需要它的治理宿主生灭，同时保留三态裁决和 WorkBuddy 原生权限的边界。

**推翻范围**：本决策修订下方 ADR 中“settings 只覆盖文件/命令工具”“未知工具属于 hook 协议错误”
以及“新增工具必须增加 matcher”的部分。严格公共 envelope、已知专用 adapter、协议损坏终止任务、
约束驱动三态裁决和单次 `pre_commit` 语义继续有效。PostToolUse 编译触发范围不在本决策内。

---

## 2026-07-23 · PreToolUse 采用约束驱动的语义证据与三态裁决

**决策**：`.codebuddy/settings.json` 只承担 WorkBuddy 工具的静态 hook 覆盖，并把每次文件或命令
`PreToolUse` 调用恰好一次路由到 `scope_guard.py --hook-stdin`。协议解析、工具调用归一化、git commit
识别、effective constraint 求值和最终响应都属于 harness 机制，不在 settings 中并行预判。

WorkBuddy 文件工具与 `Bash` / `execute_command` / `PowerShell` 进入同一流程：

1. `scope_guard.py` 按工具名和真实 payload 字段严格解析；
2. `operation_normalizer.py` 只产生现有 evaluator 需要的 `pre_write` / `pre_command` / `pre_commit`
   证据，不建立“所有 AI 行为”枚举，也不做授权；
3. 精确证据先由当前任务 effective root/workflow/spec constraints 求值；
4. 无法确定的潜在副作用只有在可能绕过当前 effective deny 时才拒绝本次调用。

`PowerShell` matcher 与 adapter 根据已观察到的 WorkBuddy 工具名加入；第一版只接受
`tool_input.command` 与顶层绝对 `cwd`，字段不同必须取得真实 payload 后显式扩展，不做猜测。
`.codebuddy/settings.json` 删除原 command 工具的 `pre_command` / `pre_commit` 并行 hook；`git commit`
是否产生 `pre_commit` 由 normalizer 分析。

**三态语义**：

- `abstain`：没有命中当前有效约束，且 unresolved effect 不会绕过当前有效 deny；返回
  `continue:true`，不输出 `permissionDecision:allow`，WorkBuddy 原生权限继续有效。
- `deny_current_call`：命中当前有效 deny，或 unresolved effect 可能绕过它；返回
  `permissionDecision:deny`，不输出任务终止字段。unresolved 报告使用
  `HARNESS_OPERATION_UNRESOLVED` / `operation_report`。
- `abort_task`：hook 协议、任务上下文、有效宿主声明或 evaluator 运行失败；返回
  `HARNESS_TASK_ABORT`、`permissionDecision:deny`、`stopReason` 与 `systemMessage`，交由人工处理。

**硬门禁边界**：一条动态约束只有在真实工具被 matcher 捕获、payload 有可信 adapter、所需语义证据
可得或相关副作用被标记 unresolved、宿主当前 effective、客户端执行 deny 时，才是该 WorkBuddy 表面内
的硬门禁。工具名本身既不授权也不禁止；相同规则不按 File/Bash/PowerShell 复制。PostToolUse 编译触发
范围保持不变，另行决策。

**理由**：硬门禁来自“静态客户端覆盖 × 动态有效约束”，而不是 shell 白名单。全局默认拒绝会把
harness 变成第二套客户端权限系统；全局放行未知副作用又会让已明确的动态 deny 被宽入口绕过。三态模型
让 harness 在没有规则命中时克制 abstain，在规则确实可能失守时 fail-closed，并把机制失效与可恢复的
当前调用拒绝分开。

**推翻范围**：本决策推翻下方“无法证明只读的 shell 只拒绝当前工具调用”中的全局白名单/default-deny
授权模型，也推翻“WorkBuddy PreToolUse 协议错误终止 AI 任务”中删除 `PowerShell` 及按外部
`pre_command` / `pre_commit` 事件绑定工具的配置边界；其严格 payload 解析和协议错误终止语义继续有效。

---

## 2026-07-23 · 无法证明只读的 shell 只拒绝当前工具调用（已被上方决策推翻）

**决策**：WorkBuddy `Bash` / `execute_command` 进入 `pre_command` / `pre_commit` 后，必须先按闭合
shell 语法和白名单分类。只接受单条静态命令；管道、连接、重定向、展开、动态解释器入口和未知程序
默认拒绝。只读程序、git 只读子命令、git `add` / `commit` 以及四个精确 harness wrapper 由机制枚举；
Python `-c` / `-m`、PowerShell/cmd/bash 动态脚本、未登记 Python 脚本和可能执行外部程序的选项不放行。
shell payload 必须提供非空绝对 `cwd`，否则 wrapper 身份无法可信解析，按 hook 协议错误处理。

**失败语义**：无法证明只读或受控时返回 `HARNESS_COMMAND_DENY`、`command_policy_report` 和
`permissionDecision:deny`，只拒绝当前工具调用，`abort_task:false`，不输出 `stopReason` /
`systemMessage`。AI 读取报告后应改用 WorkBuddy 文件工具或精确受控 wrapper 继续任务，不能换动态脚本
绕过。只有 `HARNESS_TASK_ABORT`（协议或治理上下文失效）要求结束整个 AI 任务并交给人处理。

**组合顺序**：项目约束 evaluator 与通用分类都执行；若项目约束已经 deny，保留更具体的项目原因，
否则再应用通用 default-deny。git `commit` 仍执行现有 `pre_commit` 状态门禁。根 skill 的
`command_hints` 只服务项目 `command_write` evaluator，不是通用 shell 安全白名单或只读证明。

**理由**：shell 可通过解释器、子进程和重定向绕开仅接收结构化路径的 `pre_write`，字符串关键词也无法
证明任意命令没有写副作用。完全终止任务又会把一个可恢复的工具选路错误升级为治理上下文事故；拒绝
当前调用并向 Agent 返回安全恢复路径，能保持 fail-closed 而不误杀整个任务。

**边界**：这是 WorkBuddy hook 层的静态分类，不替代 OS 沙箱、可执行文件签名、PATH 完整性或外部进程
行为审计。新增通用命令必须作为 harness 机制决策评审，不能通过项目 `command_hints` 偷渡。

---

## 2026-07-23 · WorkBuddy PreToolUse 协议错误终止 AI 任务（配置边界已被上方决策修订）

**决策**：hook 输入是治理门禁的信任边界，不再采用“解析失败后用空对象继续求值”的 fail-open 行为。
scope_guard 只接受 WorkBuddy 官方 PreToolUse 结构以及明确列出的 IDE/CLI 工具别名：
`Write/write_to_file`、`Edit/replace_in_file`、`delete_files`、`Bash/execute_command`。

JSON 损坏、根节点或 `tool_input` 类型错误、`hook_event_name` 错误、未知工具、内部事件与工具不匹配、
必需字段缺失/错型/别名冲突，以及相对路径缺少 `cwd`，全部返回 `hook_protocol.invalid` 与
`HARNESS_TASK_ABORT`。输出同时使用 WorkBuddy 官方 `continue:false`、`hookEventName: PreToolUse`、
`permissionDecision: deny`、`stopReason`、`systemMessage`，进程退出码为 2，并把结构化
`hook_protocol_report` 保留在顶层 `harnessReport` 交给人处理。AI 不得猜测字段、换工具绕过或自行
降级后继续。

**配置边界**：`.codebuddy/settings.json` 只注册上述 WorkBuddy 工具；删除未建立协议适配的
`MultiEdit`、`NotebookEdit`、`apply_patch`、`shell_command`、`functions.exec`、`PowerShell` 等名称。
`delete_files` 纳入 pre_write 和 PostToolUse。其他客户端或新 WorkBuddy 工具必须基于真实 payload 单独
增加显式适配，不能复用宽松的通用字段猜测。

**理由**：无法识别操作目标时继续 evaluator，只会让所有路径和内容约束看到空上下文并返回 allow；
这不是普通工具故障，而是门禁已经失去判断能力。终止整个 AI 任务能够防止同一协议偏差在后续调用中
持续绕过治理。

**边界**：本决策闭合 hook 载荷解析与工具路由；shell 命令的通用静态分类已由上方新 ADR 处理，
evaluator 运行期异常策略和 PostToolUse 编译触发范围仍分别讨论。

---

## 2026-07-23 · 有效治理宿主的声明错误按生命周期 fail-closed

**决策**：约束来源读取与 schema 校验产生的诊断必须携带宿主身份。scope_guard 使用约束实例相同的
激活判定计算诊断所属宿主在当前任务是否有效：

- 已加载 root skill、当前选中的 workflow skill、active spec 的 `source.*` / `constraint.*` error
  立即返回 `HARNESS_TASK_ABORT`，跳过正常 evaluator 求值；
- 未激活 workflow skill 和非 active spec 的同类 error 只保留为诊断，不阻断当前任务；
- warning 不阻断；`activation.*` 与 `root_skill.invalid` 继续无条件阻断。

诊断输出增加 `host_type`、`host_id`、`lifecycle` 与运行时派生的 `host_effective`，让人能够区分
“仓库中存在坏声明”和“当前任务依赖的治理源已经失效”。

**理由**：只报告 schema error 但继续求值，会让未知 evaluator、缺字段或丢失检查脚本等已激活约束静默
放行；反过来让所有已发现宿主的 error 都阻断，又会把未采用 workflow 的维护问题错误扩大到当前任务。
阻断行为必须与宿主生命周期使用同一激活语义。

**边界**：本决策只处理声明发现与静态 schema 完整性；evaluator 运行期异常和 hook payload 解析失败的
故障策略仍属于后续独立机制决策。

---

## 2026-07-22 · Root skill 采用 loaded / missing / invalid 三态

**决策**：root skill 是推荐建立但不作为 harness 硬运行依赖的项目治理入口。机制必须区分：

- `loaded`：root skill 存在且 frontmatter 可解析，始终生效；
- `missing`：非阻断 onboarding 状态，harness 继续发现和求值 workflow/spec，同时要求 AI 告知用户并建议初始化；
- `invalid`：文件已经存在但不可读或 frontmatter 无法解析，视为已声明治理源损坏，返回
  `HARNESS_TASK_ABORT` 并交由人工处理。

缺失 root 不得关闭其他约束发现，也不得被伪装成工具故障后静默降级。新增通用空模板
`harness/templates/root-skill.md` 与非覆盖式初始化器 `harness/scripts/init_root_skill.py`；初始化器只创建
空 schema，不补造项目知识或约束，AI 必须先告知用户并取得确认。明确写入 spec `required_skills` 或
state `active_skills` 的 workflow skill 在运行环境缺失时，仍按 `activation.skill_missing` 立即终止任务。

**理由**：新项目可以先运行通用 harness，再逐步建立项目治理；但一个已经存在却损坏的 root skill
代表明确声明的治理源失真，继续执行会静默丢失项目级约束，必须人工介入。

**推翻范围**：推翻旧实现中“root skill 不可用就停止全部 discovery 并 fail-open”的行为；下方 ADR 中
“root skill 始终生效”统一解释为“root skill 存在且有效时始终生效”。

---

## 2026-07-22 · Plugin 开发约束统一为 plugin-owned workflow skill

**决策**：plugin 不再是独立约束宿主。删除 `Plugins/*/harness.yaml` 发现机制和 plugin 专用激活分支；
plugin 自身开发所需的知识、流程、机器约束、检查与测试统一由 plugin 物理拥有的 workflow skill 承载：

```text
Plugins/<X>/.workbuddy/skills/<name>/   # 唯一物理真相源
              ↓ junction
.workbuddy/skills/<name>/               # WorkBuddy 发现入口
```

该 skill 被发现后仍是 `workflow_skill`，只通过 active spec 的 `required_skills` 或无 spec 任务的
`active_skills` 激活。任务只是使用、依赖、调用或配置 plugin 时，不激活 plugin 开发 skill；相关约束由
实际工作流对应的项目 workflow skill 提供。plugin 业务说明继续留在 plugin 文档中，机器约束只声明在
skill frontmatter，复杂检查跟随同一 skill 的 `checks/`。

**理由**：WorkBuddy 只从项目 skill 入口发现任务 skill，而 junction 已验证可同时满足 WorkBuddy 可发现性、
scope_guard 动态发现和 plugin 物理所有权。复用现有 selector 可以避免 `developing_plugins`、plugin selector
与第二套约束 schema，消除“plugin 存在/被使用就自动施加开发约束”的语义错误。

**来源追踪**：`host_type` 与生命周期始终保持普通 `workflow_skill`；`source_file` 记录 junction 发现入口，
`canonical_source_file` 只记录链接解析后的物理文件。harness 不解析 `Plugins/<X>`、不推断 plugin 所有者、
也不引入 plugin 特殊生命周期；删除 plugin 时物理 skill 与 junction 由项目生命周期协议一并回收。

**推翻范围**：推翻下方“扩展约束发现：spec 级 + 插件级”中所有 plugin host、`harness.yaml` 和
四层发现结论；spec 发现部分继续有效。治理模型升级为 `dynamic-hosted-constraints/v2`。

---

## 2026-07-22 · 区分约束发现、任务激活与 AI 上下文刷新

**决策**：root skill 是项目常驻宿主；其他 workflow skill 是任务依赖，不因安装在目录中就自动成为
当前任务认知。任务 skill 可以由 confirmed spec 的 `required_skills` 声明，也可以在对话中由 AI
根据任务选择并明确说明。skill 激活后，其 frontmatter constraints 及所属 script 检查共同生效；
skill 只能收紧流程与边界，不能扩大 spec 授权。

治理协议正式区分：宿主可被机制找到的 **发现**、当前任务采用宿主的 **激活**，以及磁盘变化后
机器/AI 重新读取的 **刷新**。删除宿主可让下一次机器发现立即回收实例，但已经进入 AI 上下文的旧内容
仍需显式重载，不能宣称在当前会话中自动遗忘。

**实现状态**：任务级过滤已落地。有 active spec 时，scope_guard 只从其 `required_skills` 读取
当前 active workflow skills；无 active spec 时才读取 `harness_state.active_skills`。root skill 始终生效；只有 active spec
自身的 constraints 生效。scope_guard 不选择 skill、不提供 activate/deactivate 命令，也不写入任务
状态；AI 或其他任务执行者负责把选择结果写入 spec/state。`evaluate()`、context 和 explain 共用同一个
`_is_active()` 判定，分别执行或展示 discovered/effective 实例。

任务上下文解析错误采用 fail-closed：任何 `activation.*` error 都会跳过正常 evaluator 求值，返回
`HARNESS_TASK_ABORT` 与结构化 `task_context_report`；`--context` / `--explain` 以退出码 2 结束，hook
直接 deny。AI 收到该结果后必须立即结束当前任务并把完整报告交给人处理，不得猜测、降级或自行修复后继续。

当前 selector 已校验 `active_spec` 的规范文件身份，并要求被选择 spec 具备 `status: confirmed`；
state 仍是唯一 selector，status 不构成自我激活。AI 上下文刷新握手仍待后续单独决策。plugin 开发约束
已由上方新 ADR 收敛为普通 workflow skill。

**推翻范围**：下方“扩展约束发现”ADR 中“发现后直接全部生效”的隐含语义不再成立；
workflow/spec 的发现机制保留，激活必须作为独立步骤补充。

---

## 2026-07-22 · 引入 `script` 求值器 + `when` 多值，终结 evaluator 膨胀

**决策**：新增 `script` 求值器——运行外部脚本，非零退出码即拦截。同时 `when` 支持 list（`[pre_write, pre_commit]`），
单值字符串仍兼容。此后项目特定检查需求不再改 harness——在 skill 目录下写脚本 + 声明 `evaluator: script` 约束即可，
脚本随 skill 生灭。

**理由**：
- 每加检查维度就加求值器会导致 harness 无限膨胀，违背"机制不动、数据动"的原则。
- `script` 求值器是通用机制——逻辑在 skill 侧，机制只管"跑脚本、看退出码"。
- 4 个内置求值器覆盖原子维度，不再增加；此后全部走 script。

**改动**：
- `scope_guard.py`：新增 `_eval_script()` + EVALUATORS 注册 + `evaluate()` 支持 `when` list +
  `main()` 提取 `content` + `event` 到 ctx。
- 新建 `checks/ban_engine_path.py`：禁止硬编码引擎路径的检查脚本。
- 根 skill SKILL.md：constraints 9→10 条（+ script）。
- README：evaluator 表 + 扩展表更新。

**验证**：script 拦截含 `D:\UnrealEngine` 的内容 → deny；无害内容放行；when 双时机命中。10 条约束全部加载。

---

## 2026-07-22 · 扩展约束发现：spec 级 + 插件级（plugin 部分已被上方 ADR 推翻）

> 历史说明：以下关于 `_discover_plugin_constraints()`、`Plugins/*/harness.yaml` 和 plugin 独立宿主的
> 决策均已失效，只保留 spec 约束发现部分作为现行机制。

**决策**：在 `scope_guard.py` 的 `discover_constraints` 中新增 `_discover_spec_constraints()`
和 `_discover_plugin_constraints()` 两个发现函数，使约束实例从二源（根 skill + 其他 skill）
扩展为四源（根 skill + 其他 skill + specs + plugins）。

**理由**：
- README 从 07-20 起就规划了四层实例模型，但 spec 和 plugin 两层从未实现，一直是文档超前于代码。
- spec 级约束（爆炸半径）是需求开发的核心护栏——不做这一步，日后创建 spec 时约束槽位只是摆设。
- 插件级约束是"随插件生灭"的天然载体，插件自带约束声明比在根 skill 集中声明更符合实例生灭原则。

**改动**：
- `scope_guard.py`：新增 `_discover_spec_constraints()`（扫描 `specs/*.md` frontmatter）+
  `_discover_plugin_constraints()`（扫描 `Plugins/*/harness.yaml` YAML）；在 `discover_constraints`
  尾部追加两行调用。
- README：spec/plugin 行去掉"（后置）"标注。
- 验证：创建测试 spec + 测试 harness.yaml，`--context` 显示 12 条（root:10 + spec:1 + plugin:1），
  spec 的 invert 约束和 plugin 的 Private 约束均正确拦截，Public 和无关路径放行；清理测试夹具后
  回到 10 条基线。

**影响**：spec 模板的 `constraints: []` 槽位和 `Plugins/<X>/harness.yaml` 的声明即日生效——
创建即拦截，删除即回收。当前无 spec 也无 plugin 声明，机制就位，零实例开销。

---

## 2026-07-22 · 闭合机械门禁缺口：补 5 条约束实例 + state_field 扩展 missing_equals

**决策**：在根 skill 的 frontmatter `constraints` 中新增 5 条约束实例，使 4 个已有求值器
（`path_writable_stage`、`state_field`×2、`command_write`×2）从"有实现无实例"变为机械强制。
同时扩展 `_eval_state_field` 求值器，新增 `missing_equals` 字段——文件缺失时按此值等效判定，
解决"从未编译 = 门禁放行"的漏洞。

**理由**：
- `path_writable_stage` 求值器自 07-20 已实现，但无约束实例，design 阶段禁写代码仅靠 AI 自觉。
- `pre_commit` / `stop` 编译门禁是 AGENTS.md 铁律要求，但 `state_field` 无实例触发。
- `command_write` 求值器已实现但无实例，无法机械拦截 shell 直接操作 .uasset/.umap。
- 编译文件缺失（从未编译）时 `state_field` 默认放行，使 pre_commit / stop 门禁形同虚设；
  `missing_equals` 让约束声明方决定"缺文件=成功还是失败"。

**改动**：
- 根 skill `SKILL.md`：`constraints` 从 5 条 → 10 条（+ path_writable_stage、state_field×2、command_write×2）。
- `scope_guard.py` `_eval_state_field`：新增 `missing_equals` 参数（向后兼容，不指定则行为不变）。
- README：state_field 文档更新。
- 验证：7 项测试全部通过（design 写代码 deny、pre_commit/stop 放行、.uasset 写 deny、.uasset 读放行、
  编译文件缺失 deny）。

---

## 2026-07-21 · 删除 architecture.md，"架构做法"不进 harness 固定文档

**决策**：删除 `docs/ai/architecture.md`。它原写"Lyra 式三层架构（Framework/Base/GameFeatures）、
数据驱动、依赖单向"——这**不是 harness 的内容，而是 cba_game 当前选择的一套技术路线/做法**。

**理由**：
- 该内容会随技术路线变化而漂移（换项目/换架构就过时），不该作为 harness 的稳定底座文档。
- 与"换项目只换 skill、机制不动"的原则矛盾：架构做法属"做法/知识"，应进 skill 随用随换，
  不该躺在固定 architecture.md 里假装稳定。
- "Framework Plugins"是 Lyra 特指叫法，非普适；对不用 GameFeature 的项目该文档毫无价值。

**归位**：
- 项目技术路线（Lyra 式三层等）**不记在本文件**——它属"做法"，将来做成 skill 随用随换。
- 当前项目实际架构以 `Plugins/`、`Source/` 目录现状 + 各插件自带文档为准。

---

## 2026-07-20 · 建立治理层（Harness）最小骨架

**决策**：采用"机制与知识分离 + 拦截点计算 + 薄壳 skill"的轻量治理层，
不建 HET 式的状态机引擎 / rule-id 锚点 / 完整 intent 治理。

**理由**：
- 空白项目，痛点未出现，最小骨架先行，痛点驱动扩展。
- 授权在写前拦截点（PreToolUse）机械判定即可，无需状态机引擎管理流转。
- 机制（脚本）一次写好，知识（根 skill 配表 / 各做法 skill / spec）随项目生长。

**结构**：AGENTS.md（入口）+ harness/（机制+数据+本决策记录）+ .workbuddy/（绑定+skill+memory）。

**门禁分层**：AI 层（settings.json 4 hook，软）+ 系统层（.githooks，硬，待 git init 后建）。

**补充（同日）**：skill 机制保留（目录 + 按痛点添加的约定），但 skill 内容当前为空——
工作流/领域流程待真实需求出现时再沉淀，不预先按示例实现。
