---
dev_mode: "AAF correctness hardening + dead-weight cleanup + named-slot CostPolicy first block"
required_skills: [agent-parallel-work]
tool_providers: []
required_tool_capabilities: []
status: confirmed
created: "2026-08-27"
constraints: []
---

# AAF 加固与具名插槽第一阶段

## 背景（2026-08-27 设计讨论收敛）

- Fragment「镜像全部 GA 虚函数」架构正式否决。根因是违反接口隔离与多实现语义冲突无裁决规则；
  性能只是表象。
- 替代范式 = 具名插槽组合（Type Object + Strategy）：Row 上每个正交关注点占一个
  `TInstancedStruct` 窄接口槽，GA 管线对每槽单次直接虚调用，零遍历冗余。
- 引擎证据：UE 官方 GameplayEffectComponent、Mover 四轴正交分解、Lyra ULyraAbilityCost /
  ULyraGameplayAbility。
- Row 保持唯一真相源：不做编辑器 bake（双真相源 + 热更 diff 膨胀）；预载缓存放授予期一次性完成。

## Phase 1 正确性修复

### P1-1 授予期预载缓存
- `UAssemblyAbilitySource` 新增缓存字段：`CachedCooldownGE` / `CachedCostGE`（Grant 时填充）。
- `FAssemblyExecutionBase` 新增窄接口 `CollectSoftObjects(TArray<TSoftObjectPtr<UGameplayEffect>>&) const`，
  各执行策略上报自身软资产引用（InstantGE/AOE 报 OnCastGE，Projectile 无引用直接返回）。
- `GiveAbilityFromRow` 统一同步加载一次并写入缓存；策略运行时读缓存，不再自行加载。
- 冷却/成本读取（GetCooldown/CostGameplayEffect）改读 Source 缓存。
- 验收：插件内 LoadSynchronous 仅存在于授予路径，激活热路径零同步加载。

### P1-2 OnAvatarSet 激活守卫（对照 Lyra TryActivateAbilityOnSpawn）
补四重防护后再 TryActivateAbility：
1. `!Spec.IsActive()` 防重入；
2. Avatar 已 TearOff 或 LifeSpan > 0 时不激（将死）；
3. 客户端侧：IsLocallyControlled 且 NetExecutionPolicy 为 LocalPredicted/LocalOnly；
4. 服务器侧：IsNetAuthority 且 NetExecutionPolicy 为 ServerOnly/ServerInitiated。
仅满足任一侧时发起激活。
- 验收：LocalPredicted 技能仅预期侧发起激活；Pawn 将死时 Give 的 on-spawn buff 不再幽灵激活。

### P1-3 构造默认对齐 Lyra
- `UGA_AssemblyBase` 构造补 `NetSecurityPolicy = ClientOrServer`。
- 验收：默认策略三件套与 LyraGameplayAbility 构造一致。

### P1-4 ExecutionOverrides 有序化（消除容器序隐式依赖）
- `FAssemblyAbilityRow::ExecutionOverrides` 从 TMap 改为有序数组 `TArray<FNamedExecutionOverride>`
 （结构含 Tag + InstancedStruct）。
- `ResolveExecutionData` 改为按声明顺序遍历、首个命中标签获胜（first-match-wins，Mover Transition
  同款语义），命中时打 Verbose 日志标明来源下标。Validator 同步校验数组项。
- 验收：多覆写标签同时持有时选择结果确定且随声明顺序可预测。
- 注意：这是存档格式变更，实施前确认无存量 DataTable 依赖旧字段。

### P1-5 AOE 注释一致性
- `FAssemblyExecution_AOE::CollisionChannel` 注释与默认值统一：改注释为 WorldDynamic 语义说明，
  策划可按需配置。验收：头文件注释与默认值描述同一行为。

## Phase 2 死重清理（R1：删除优先于校验）

### P2-1 FAssemblyChainContext 死成员删除
删除零调用成员：AppliedRuleIds / RuleApplyCounts / TriggerMask / BlockedTriggerGroups /
IsTriggerMasked()。保留 ChainDepth / RootEventId / InstigatorActor / DamageModifiers。
- 验收：grep 全插件无残留引用；链事件行为不变。

### P2-2 Router 死 API 删除与深度限制收敛
- 删 UGA_AssemblyEventRouter::CheckChainDepth / ResetFrameEventCounter。
- MaxChainDepth 收敛为 UGA_AssemblyBase::MaxChainDepth 单一常量来源；Router 不再另设同名实例属性。
- 保留并继续生效：MaxFanOutCount / MaxEventsPerFrame。
- 验收：grep 无孤立 API；限频保险丝语义不变。

### P2-3 ini 与文档修正
- DefaultAssemblyAbilityTags.ini 删除 Fragment.* 三个 meta tag（Fragment 层已废弃）。
- 新建 Plugins/AssemblyAbilityFramework/Docs/AGENTS.md：一页 tag namespace 规范
 （Event/Emit/Origin/State/Proc/Talent/Buff/Debuff/Cue 用途与新增规则），使 ini 第 5 行既有引用不再悬空。
- 验收：ini 引用的文档真实存在；namespace 清单与 ini 条目一一对应。

### P2-4 Validator 自动化接线
- 将 ValidateRow 逻辑并入 FAssemblyAbilityRow 对 FTableRowBase 编辑期校验钩子的覆写，使 DataTable
  编辑器自动报错。实施前先核对 UE 5.9 引擎头文件确认确切虚函数名与签名；与预期不符时以引擎实际 API
  为准并在此回填记录。
- 兜底：授予路径 Give 前强制执行一次校验，失败打 Error 日志并拒绝授予。
- 验收：非法 Row（如 CooldownOverrideTags 无 CooldownGE）在 DataTable 编辑器即时可见，且 Give 被拒。

## Phase 3 具名插槽第一块：CostPolicy

### P3-1 成本插槽
- 新增抽象 `FAssemblyCostBase`：CheckCost（含 OptionalRelevantTags 失败原因输出）+ ApplyCost。
- FAssemblyAbilityRow 新增 `TArray<TInstancedStruct<FAssemblyCostBase>> Costs`。
- GA 集成（对齐 Lyra 顺序）：CheckCost = Super && 全部 Costs 通过，失败写入 fail reason tag；
  ApplyCost = Super 后依次 Apply。
- 本期不实现 ShouldOnlyApplyCostOnHit（明确二期扩展点，接口预留注释）。
- 验证：编写一个临时最小 C++ 成本子类做激活失败/成功路径验证；临时类验证后移除或明示标记，
  不留在生产示例集（不预先发明未被需求的成本子类）。
- 验收：不满足条件的 Costs 阻止激活且原因可读；空数组 Row 行为零变化。

### P3-2 授予 authority 断言
- GiveAbilityFromRow / FromDataRegistry / FromTag 增加 `ensureMsgf(ASC->IsNetAuthority(), ...)`
  早退保护，客户端误调时给出可读错误而非引擎 assert。
- 验收：客户端 ASC 上调用三入口均安全早退并输出定位日志。

## 不做什么（显式暂缓，防日后误判缺陷）

- 激活组机制（Exclusive 组互斥池）——CancelAbilitiesWithTags 已覆盖当前粒度
- 失败消息总线（MessageSubsystem / Montage 表）——接 HUD 时再立项
- Targeting / CooldownPolicy 插槽——出真实需求再做
- 编辑器 bake——未来若对接配表平台，bake 产物只作 lint 报告不作运行时真相
- 发包后新增行为类型的 GameFeature 化方案——另行需求
- EventRouter 的 queue-flush 化与 rollback 支持（Mover 先例已归档，等并发需求）

## 未做（后续 spec 候选）

- LayeredMove 式中央混合器：DamageModifiers 支持乘算/覆盖型 mix-mode + Priority 裁决
- cancel-by-tag 特征寻址（HasGameplayTag + CancelFeaturesWithTag 形态复活触发屏蔽能力）
- Logic/Data 二分（参照 Mover ULayeredMoveLogic + FLayeredMoveInstancedData 收拢链上下文）

## 设计收敛（2026-08-27 下午：Fragments 聚合与职责归位）

- 单数组聚合定案：`Row.Fragments`（TInstancedStruct<FAssemblyFragmentBase>），成本/冷却为子类片（X-1/X-2' 已实施）；ExecutionStrategy 单槽不并入；旧 FAssemblyCostBase.h 转弃用桩，物理删除待工具链窗口
- 引擎原生 Cost/Cooldown 四通道在 DataDriven 整体退役（getter 空返回），片段为唯一结算层；FAssemblyFragment_Cooldown = CooldownTags 判定 + 条件 DurationOverrides（A1 平行容器消亡）
- B1 AssetTags 行字段 + CanActivateAbility 内 AreAbilityTokensBlocked 检查（已实施）
- Router 角色定案：纯 tag→tag 无状态路由表，不参与授予/激活决策；触发声明不进 Router
- B2 重设计定案（取代原 RespondToTags+GA 自订阅）：Trigger 声明 = FAssemblyFragment_Trigger，字段同引擎 FAbilityTriggerData（TriggerTag+TriggerSource）；授予期 OnGrant 注册到 ASC 公开委托（GenericGameplayEventCallbacks / RegisterGameplayTagEvent），行为等价引擎 AbilityTriggers（引擎表 :1815 为 private 无 API，实证）
- 基类新增授予期贡献钩子 `OnGrant(Spec, ASC, Owner)`，与 Precache 并列双循环；原 RespondToTags 字段与 DataDriven 订阅代码删除（净减 ~130 行）
- DataDriven 职责收窄：纯投影层 + 编排管线，零硬编码行为（触发→Trigger 片、屏蔽→Gate 片候选、上下文→N1 记录）
- 闭合家族清单（后续候选）：充能/体力片、生命献祭片、ActivationGate（Blocked+Required）、免疫/抵抗片、命中才扣修饰（Cost 属性）；三类反例不入片：订阅声明/编排副作用/管线参数
- N7 双端对称 Give 契约（Spec.SourceObject 不复制，客户端实例亦需 Row）

## 门禁与流程要求

- 源码变更后必须通过 build_editor.py 内容指纹刷新方可进入 commit / stop（build_freshness 生效）。
- 按 agent-parallel-work 流程执行：Coder/Reviewer 分离，Reviewer 只读，迭代不设上限；
  审查清单 = 本 spec 全部验收项逐条勾稽 + root skill 机械约束。
- 新增类型命名遵守 project.code.ue-naming-prefix（U/F/E + Assembly 词根）。

## 实现状态（下午批次）

- [x] X-1 Fragments 更名迁移（CostBase→FragmentBase，旧头弃用桩）
- [x] X-2' 冷却片 + getter 收敛 + A1 平行容器消亡
- [x] B1 AssetTags（行字段 + AreAbilityTokensBlocked 检查）
- [x] B2 终稿（Spec.DynamicAbilityTriggers 方案）：Trigger 片 = FAbilityTriggerData 一比一数据化；
  `ContributeToSpec` 基类钩子（管线零具体类型引用）把行内 Trigger 片汇总进 Spec.DynamicAbilityTriggers；
  引擎在 GiveAbility:578 原生注册 / 移除:637 原生注销，payload 由 TriggerAbilityFromGameplayEvent 原生投递；
  自建订阅机件（lambda/簿记/pending/OnGrant 双循环）全部删除归零
- [x] 生命周期钩子窄基类（8 个）：OnGiveAbility / OnRemoveAbility / OnAvatarSet / OnCanActivate /
  OnPreActivate / OnActivated / OnEndAbility / OnFailed —— 每钩子独立窄基类（ISP），片按需继承；
  DataDriven 各流程点一行转发
- [x] 分派查表化：Source::HookIndices（TMap<UScriptStruct*, TArray<int32>>，惰性构建）
  + ForEachHook<HookT>(Source) O(1) 定位、遍历命中组——消除每流程点 O(N) IsChildOf 探测；
  曾论证低频/高频双区分区后判定无意义（TMap 同为 O(1)，成本在钩子调用频率与存储无关），单表定案
- [x] 硬编码迁移成片：`FAssemblyFragment_ActivationGate`（BlockedTags + RequiredTags + bCheckAssetBlock，
  承接原行级 ActivationBlockedTags 与 B1 AssetTags 全局屏蔽；Row 删 ActivationBlockedTags 字段；
  CancelAbilitiesWithTags 保持行级编排参数）
- [x] R1 清理：`#if 0` 遗留冷却段物理删除（含 Legacy 四字段从 Row 移除）、旧 FAssemblyCostBase.h 物理删除、
  DataDriven.h 过时头注释更新（CanActivate 说明 → 门卫片转发）
- [x] 结算窄基类 `FAssemblyFragmentCostBase`（成本语义，体现 Check/Apply=CheckCost/ApplyCost 管线）：
  Check/Apply 从基类下沉——FAssemblyFragmentBase 仅剩授予期工具（Precache/ContributeToSpec），
  零行为虚函数；Cooldown 片改继承 CostBase（冷却降级为成本管线一员）；CheckCost/ApplyCost 改
  ForEachHook<CostBase> 查表分派（first-fail / 逐片结算）
- [x] InputTag 删除（Row 字段 + Statics 步骤2 + 注释；输入绑定暂不需要）
- [x] 身份子系统（引擎 CDO 身份体系对动态行失效的完整归位）：
  - 实证链：GetAssetTags() 非虚（:173）→ SetAssetTags 构造期锁（RF_NeedInitialization）→
    CreateNewInstanceOfAbility check RF_ClassDefaultObject（:1199）→ 行身份无法进 CDO
  - `FAssemblyFragment_Identity` 片承载行 AssetTags（Row 平铺字段删除）
  - ActivateAbility 开头直写实例 AbilityTags（per-actor 独立，无类级污染）→ 引擎 block/cancel/可取消副作用读到行身份
  - 取消路径对齐引擎：裸 `ASC->CancelAbilities(&Tags)`（CDO 匹配，对动态行失效）→
    `ApplyAbilityBlockAndCancelTags`（显式传行身份，引擎同款 :999）
  - Statics 删 `AddTag(AbilityTag)` 错位挂载（DynamicSpecSourceTags=GE 源标签，与身份正交，实证 :247 注释）
  - Gate 片 bCheckAssetBlock 数据源 → 身份片；按 tag 检索（TryActivateByTag）对动态行失效 → 记「不做」区，AAf 行主键寻址承接
- [x] BlockCancel 片化：`FAssemblyFragment_BlockCancel`（BlockTags/CancelTags 纯数据片，与 Identity 同构）；
  Row 删 Cancel/BlockAbilitiesWithTags 平铺字段；DataDriven 加 GetBlockCancelConfig 辅助；
  ActivateAbility 施加 / EndAbility 对称回收（引擎 :999/:888 同款，Block 计数式须配对）——指纹 af2335a7（20:37）
- [x] Gate 片 7-tag 矩阵补全（引擎 DoesAbilitySatisfyTagRequirements 全量，GameplayAbility.cpp:349-443）：
  - 实证修正：Source/Target Required/Blocked 非遗留字段，引擎真实消费（:411-426）；SourceTags/TargetTags
    来自 TriggerEventData.InstigatorTags/TargetTags（ASC :1813-1814 实证，事件激活有数据流）
  - Gate 片 + SourceBlocked/SourceRequired/TargetBlocked/TargetRequired 四字段；判定序列对齐引擎
    （全 blocked → 全 required → AssetBlock :432）；失败写 ActivateFailTags* 原因 tag（:361-401 同款）
  - `OnCanActivateBase` 签名演进：+ SourceTags/TargetTags 指针（空指针 = 该侧不判定，:409-416 同款）
- [x] `FAssemblyFragment_OwnedTags` 片（ActivationOwnedTags 语义）：纯数据片 + GA 编排
  （ActivateAbility AddLooseGameplayTags :990 / EndAbility 对称移除 :870，复制参数走 ShouldReplicateActivationOwnedTags）
- [x] build_editor 指纹刷新（0d1a1592，318 文件，2026-08-28 13:04，--force 实证）
- [x] 数据片访问模式统一（用户纠错）：数据片自带聚合入口（Collect*），GA 编排 ForEachHook 逐片聚合
  - 消灭旧写法三错：Indices[0] 多片忽略、static_cast 挖字段、数据片硬套查表语义
  - GetIdentityTags/GetActivationOwnedTags value-return aggregate; GetBlockCancelConfig removed
  - HookIndices key = dispatch type; fingerprint 96ab585d (13:27, --force verified)
- [x] Tags 片整合（用户定案，13:52）：引擎 9 tag 字段合一 `FAssemblyFragment_Tags`（CancelAbilitiesWithTag/
  BlockAbilitiesWithTag/ActivationOwnedTags/ActivationRequiredTags/ActivationBlockedTags/SourceRequiredTags/
  SourceBlockedTags/TargetRequiredTags/TargetBlockedTags + bCheckAssetBlock），每字段一个 Collect 函数 +
  CollectTags 全集；Gate/BlockCancel/OwnedTags 三片删除（字段迁入）；判定统一 GA 内部
  （CanActivateAbility 内联引擎 7 项序列，GameplayAbility.cpp:349-443 同款：全 blocked → 全 required →
  AssetBlock，原因 tag 写入）；OnCanActivateBase 钩子保留框架无消费者；指纹 87b7746e（315 文件，--force 实证）
- [x] AssetTags 并入 Tags 片（13:56）：Identity 片删除（字段并入 FAssemblyFragment_Tags::AssetTags +
  CollectAssetTags + CollectTags 全集含入）；GetIdentityTags 改读 Tags 片 CollectAssetTags；实例直写/AssetBlock 判定路径不变；
  指纹 7faaea49（314 文件，--force 实证）
- [x] 插件目录整理（用户定案，15:20）：去掉 Experimental/ 前缀（include 全量更新，残留清零）；
  Fragment/Execution 拆子目录（AbilityData/Fragment|Execution）；UAssemblyAbilitySource/AssemblyAbilityStatics
  独立 Assembly/ 目录；40 文件全量归位（新旧无并存）；编译绿 5cfef8a7（314 文件，15 编译单元，--force 实证；
  首轮 UHT generated 文件锁失败 → editor/并行 UBT 释放后重试成功）
- [x] 链上下文载体版本适配（用户定案，15:36）：FGameplayEventData::InstancedEventData 为 5.9 新增字段，
  插件要兼容 <5.9 → 内部适配层（API 归 UAssemblyAbilityStatics 静态）：版本宏 AAF_HAS_INSTANCED_EVENT_DATA
  （FAssemblyChainTargetData.h 定义）；5.9+ SetChainContext 填 InstancedEventData / <5.9 填 TargetData
  （新 FAssemblyChainTargetData 自定义子类，官方 TargetData 即多态扩展容器；NetSerialize 四字段，
  基类 NetSerialize 非虚不加 override，SerializeObject 需 UObject*& 4 参）；GetChainContextFrom/GetTargetActors
  按版本读取/按类型跳过链载体；SendChainEvent/GetChainContext/执行策略（InstantGE/AOE）内部改用适配 API，
  使用者零感知；指纹 3f78ae24（315 文件，--force 实证）


- [x] build_editor 指纹刷新（d8f86b72，316 文件，2026-08-27 20:13）

## 实现状态

- [ ] P1-1 授予期预载缓存
- [ ] P1-2 OnAvatarSet 激活守卫
- [ ] P1-3 NetSecurityPolicy 默认值
- [ ] P1-4 ExecutionOverrides 有序化
- [ ] P1-5 AOE 注释一致
- [ ] P2-1 链上下文死成员删除
- [ ] P2-2 Router 死 API 删除 + MaxChainDepth 收敛
- [ ] P2-3 ini 清理 + Docs/AGENTS.md
- [ ] P2-4 Validator 自动化接线
- [ ] P3-1 CostPolicy 插槽
- [ ] P3-2 authority 断言
- [ ] build_editor 指纹通过 + Reviewer 全项通过

## 爆炸半径

- 允许改动：Plugins/AssemblyAbilityFramework/**（含 Docs、Config、Source）
- 禁止改动：Source/cba_game/**、其它 Plugins/*、harness/**、specs 其它文件、
  Config/Default*.ini、Content/**

## 参考锚点

- LyraGameplayAbility.cpp:442-465（OnSpawn 四重守卫）、:202-276（AdditionalCosts 管线）、:39-45（构造默认值）
- Mover（d:/unrealengine/Engine/Plugins/Experimental/Mover）：MovementMode.h:104-110、
  MovementModeStateMachine.h:20-25 与 :74-75、LayeredMoveBase.h:146 与 :239-244、
  MovementModifier.h:26-70
- 本仓库证据：UGA_AssemblyDataDriven.cpp:136（TMap 迭代序隐患）、FAssemblyExecution_AOE.h:42-43、
  DefaultAssemblyAbilityTags.ini:5,62-65
