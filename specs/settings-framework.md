---
dev_mode: "MVVM rewrite of GameSettings as generic plugin"
required_skills: [agent-parallel-work]
tool_providers: []
required_tool_capabilities: []
status: confirmed
created: "2026-08-06"
constraints: []
---

# SettingsFramework 设置系统（MVVM 重构 GameSettings）

## 做什么

- 新建通用插件 `Plugins/SettingsFramework/`（Runtime 模块），用 MVVM 体系重构 GameSettings 核心功能
- VM 体系：`USettingViewModelBase`（UMVVMViewModelBase 子类）+ Scalar/Bool/Enum/Action/Page 子类
- 值类型：Scalar（Min/Max/Step）、Bool、Enum（静态选项）、Action、Page（子页面）
- 生命周期：StoreInitial / ResetToDefault / RestoreToInitial
- 条件：EditableState + GameplayTag 驱动的 EditCondition（Platform.Trait）
- 脏追踪 + SaveChanges
- 注册表：搜索 / FindSettingByDevName / SaveChanges / 导航栈
- 渲染：USettingsScreenWidget + USettingsEntryWidget + USettingsListView（C++，Blueprintable）
- 数据源：USettingCollection + USettingEntry（UPrimaryDataAsset），**Entry 单树**：Entry 递归 Children（Instanced），ValueType=Page/Group 为容器节点，Collection 仅作屏幕根容器（含 DevName），配 BindingPath 反射路径
- 值存储：插件不存值；宿主限定 `TSubclassOf<UGameUserSettings>` 注入（对齐 Lyra `ULyraSettingsLocal : UGameUserSettings`），VM 通过 BindingPath 反射读写；ResolveHost 校验宿主类可解析静态 Get()

## 不做什么

- 不引入 GameSettings 插件
- 不改已有插件
- 不做 UI 骨架（Spec 2）
- 不改项目代码（Source/cba_game/**）
- 不改 harness / root skill / AGENTS.md

## 未做（后续 spec）

- Dynamic 值类型（运行时选项，如分辨率列表）
- 延迟 Apply（预览→确认）
- 可扩展 EditCondition 子类
- Scalar 归一化层
- Widget Panel/DetailView/VisualData
- Color/Vector2D 值类型

## 实现状态

- [x] 编译 0 error（9 h + 9 cpp）
- [x] 0 引用 UGameSetting / UGameSettingScreen
- [x] 0 CBA 前缀类型
- [ ] PIE 验证

## 设计收敛（2026-08-10 讨论确认）

- 宿主限定 `TSubclassOf<UGameUserSettings>`（Lyra 对照 `ULyraSettingsLocal : UGameUserSettings`），不再使用开放 UObject；ResolveHost 校验静态 Get() 契约
- 页面树统一为 Entry 单树（对齐 Lyra `UGameSettingCollection : UGameSetting`）：`USettingEntry` 递归 `Children`（Instanced），ValueType=Page/Group 为容器节点，页面 = 列表中的可导航条目
- `USettingCollection` 删 `ChildPages`、加 `DevName`；删除 `USettingCollectionPage` 独立资产类
- `USettingRegistry::LoadCollectionInto` 统一递归 Entry 树（仅 Entry→值 VM / Page→PageVM 两种形态）
- spec 执行依赖 `agent-parallel-work` workflow skill（多 agent 并行开发）

## 爆炸半径

- 允许改动: Plugins/SettingsFramework/**
- 禁止改动: Source/cba_game/**、其它路径
