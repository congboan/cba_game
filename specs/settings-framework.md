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
- 渲染：W_SettingsScreen + W_SettingsEntry_*（ListView 条目，Entry ViewModel 绑定）
- 数据源：DataAsset 条目配反射路径（BindingPath），VM 读写宿主对象属性
- 项目实例层：Source/cba_game/Settings/CBASettingsLocal（UPROPERTY Config 值存储）+ 验证 DataAsset

## 不做什么

- 不引入/依赖 GameSettings 插件（UGameSetting/UGameSettingScreen）
- 不改已有插件（CommonGame/UIExtension/ExperienceFramework 等）
- 不做 UI 骨架层（CBAActivatableWidget 等，归 Spec 2）
- 不改 harness/root skill/AGENTS.md

## 验收标准

- [ ] build_editor.py 编译 0 error
- [ ] 4 条验证条目（Scalar 音量 / Bool 全屏 / Enum 画质 / Action 测试项）渲染出设置页
- [ ] 插件源码 grep 不到 UGameSetting / UGameSettingScreen 引用
- [ ] 插件目录 grep 不到 CBA 前缀类型（无项目依赖）
- [ ] PIE 中 Slider 拖动双向同步、Toggle 状态同步、条目随 Platform.Trait 显隐

## 爆炸半径

- 允许改动: Plugins/SettingsFramework/**、Source/cba_game/Settings/**、Config/DefaultGameplayTags.ini
- 禁止改动: 其它路径
