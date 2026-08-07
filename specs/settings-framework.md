---
dev_mode: "MVVM rewrite of GameSettings as generic plugin"
required_skills: []
tool_providers: []
required_tool_capabilities: []
status: confirmed
created: "2026-08-06"
constraints: []
---

# SettingsFramework 设置系统（MVVM 重写 GameSettings）

## 做什么

- 新建通用插件 `Plugins/SettingsFramework/`（Runtime 模块），用 MVVM 体系 1:1 重写 GameSettings 插件功能，不依赖项目类型
- VM 体系：`USettingViewModelBase`（UMVVMViewModelBase 子类）+ Scalar/Enum/Bool/Action/Page 子类，暴露 FieldNotify 属性
- 值类型能力：Scalar（SourceRange/Step/Min/Max 限制/显示格式）、Discrete（选项/字符串序列化）、Bool/Enum/Color/Vector2D
- 生命周期：StoreInitial / ResetToDefault / RestoreToInitial
- 条件体系：EditableState（visible/enabled/resetable）+ EditCondition 数据化求值（Platform.Trait）
- 脏追踪：bIsDirty + 注册表级快照（Apply/Restore/ClearDirtyState）
- 注册表：过滤（含表达式搜索）/ FindSettingByDevName / SaveChanges / 导航栈
- 渲染：C++ 实现（USettingsScreenWidget + USettingsEntryWidget），不依赖项目 Content 蓝图
- 绑定方式：C++ 基类 Blueprintable + FieldNotify 订阅；生产环境用蓝图子类继承做 UMG 绑定（BindWidget）
- 数据源：USettingCollection（UPrimaryDataAsset，可用 AssetRegistry 按类型查找/打包 AssetBundle）+ 条目配反射路径（BindingPath），VM 读写宿主对象属性（宿主由使用方注入，插件不感知）
- 值存储：插件内不实现具体存储；使用方传入任意宿主对象（如项目 CBASettingsLocal）

## 不做什么

- 不引入/依赖 GameSettings 插件（UGameSetting/UGameSettingScreen）
- 不改已有插件（CommonGame/UIExtension/ExperienceFramework 等）
- 不做 UI 骨架层（CBAActivatableWidget 等，归 Spec 2）
- 不改项目代码（Source/cba_game/** 仅保留已提交的 CBASettingsLocal 作为宿主示例，本 spec 不新增项目文件）
- 不改 harness/root skill/AGENTS.md

## 渲染与绑定策略（2026-08-07 确认）

- 插件内 C++ 实现全部 widget 功能：USettingsScreenWidget（CommonActivatableWidget + UListView）
  + USettingsEntryWidget（IUserObjectListEntry，WidgetTree 构建，按 VM 类型创建 Slider/Toggle/文本/按钮）
- MVVM 绑定：C++ 基类暴露 FieldNotify 字段并订阅刷新；Blueprintable 供生产蓝图子类
  继承后再做 UMG 声明式绑定（BindWidget / Entry ViewModel），运行时 SetViewModel 自动生效
- ListView 用引擎原生机制（UMVVMViewListViewBaseExtension）：条目对象即 VM，生成时自动 SetViewModel
- 不依赖 ue5-editor-mcp / Content 资产；无 Content/UI 蓝图

## 验收标准

- [ ] build_editor.py 编译 0 error（注意：UBA error 9001/9666 为引擎环境故障，重试即可）
- [ ] 插件内 C++ 编译通过：USettingsScreenWidget / USettingsEntryWidget / USettingsListView
- [ ] 4 条验证条目（Scalar 音量 / Bool 全屏 / Enum 画质 / Action 测试项）渲染出设置页
- [ ] 插件源码 grep 不到 UGameSetting / UGameSettingScreen 引用
- [ ] 插件目录 grep 不到 CBA 前缀类型（无项目依赖）
- [ ] PIE 中 Slider 拖动双向同步、Toggle 状态同步、条目随 Platform.Trait 显隐

## 爆炸半径

- 允许改动: Plugins/SettingsFramework/**（本次全部工作）、Config/DefaultGameplayTags.ini（已提交，如需调整）
- 禁止改动: Source/cba_game/**（本 spec 不新增项目文件）、其它路径
