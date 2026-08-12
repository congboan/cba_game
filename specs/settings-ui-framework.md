---
dev_mode: "SettingsFramework UI 骨架 - 全局宿主迁移"
required_skills: []
tool_providers: []
required_tool_capabilities: []
status: confirmed
created: "2026-08-12"
constraints:
  - id: spec.settings-ui.allowed-path
    evaluator: path_glob
    when: pre_write
    data:
      pattern: "**"
      exempt_patterns:
        - "Source/cba_game/**"
        - "Plugins/GameFeatures/GFTests/**"
        - "Config/**"
        - "harness/state/**"
        - "specs/**"
        - ".workbuddy/skills/**"
        - ".workbuddy/memory/**"
    action: deny
    reason: "只允许全局宿主迁移范围（Source/cba_game、GFTests、Config）及治理路径"
---

# SettingsFramework UI 骨架 - 全局宿主迁移

## 做什么

把 GFTests 插件内的全局宿主类迁移到 `Source/cba_game/**`（符合架构约束：全局宿主不进 GameFeature 插件）：

- `UCBACommonGameInstance : UCommonGameInstance`（替代 UGFTestsGameInstance）
- `UCBAGameUIManagerSubsystem : UGameUIManagerSubsystem`（替代 UGFTestsUIManagerSubsystem）
- `UCBAGameUIPolicy : UGameUIPolicy`（替代 UGFTestsGameUIPolicy）
- `UCBAPrimaryGameLayout : UPrimaryGameLayout`（替代 UGFTestsPrimaryGameLayout）
- `UCBALocalSettings : UGameUserSettings`（替代 UGFTestsLocalSettings）

同时：
- `Source/cba_game/cba_game.Build.cs` 增加 CommonGame/CommonUI/UMG 依赖
- 项目 `Config/DefaultEngine.ini` / `DefaultGame.ini` 配置宿主类引用
- 删除 GFTests 插件内 5 个类及其 Config 引用

## 不做什么（边界）

- 不改 GFTests 的 GFTestActor / GFTestGameFeatureAction / GFTests.Hello
- 不改 harness / root skill / AGENTS.md / 其他插件

## 验收标准（机器可判优先）

- [ ] `build_editor.py` 编译 0 error（UBT Exit 0）
- [ ] `scope_guard.py --event stop` 返回 `{"continue": true}`
- [ ] 项目 Config 的 `GameInstanceClassName` / `GameUserSettingsClassName` 指向 `/Script/cba_game.*`
- [ ] GFTests 插件不再含 UI 宿主类（Grep 验证）

## 爆炸半径

- 允许改动: `Source/cba_game/**`、`Plugins/GameFeatures/GFTests/**`、`Config/**`
- 禁止改动: 其它路径
