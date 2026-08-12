---
dev_mode: "GFTests 测试插件"
required_skills: []
tool_providers: []
required_tool_capabilities: []
status: confirmed
created: "2026-08-10"
constraints:
  - id: spec.gftests.allowed-path
    evaluator: path_glob
    when: pre_write
    data:
      pattern: "Plugins/GameFeatures/GFTests/**"
      invert: true
      exempt_patterns:
        - "harness/state/**"
        - "specs/**"
        - ".workbuddy/skills/**"
        - ".workbuddy/memory/**"
    action: deny
    reason: "只允许改 GFTests 插件（治理 selector 路径豁免）"
---

# GFTests 测试插件

## 做什么

- 创建项目首个 GameFeature 插件 `Plugins/GameFeatures/GFTests/`（Runtime 模块），作测试游乐场
- 示例：`AGFTestActor` + `UGFTestGameFeatureAction` + 控制台命令 `GFTests.Hello`

## 不做什么（边界）

- 不引入具体业务功能；不改已有插件；不改 `Source/cba_game/**`
- 不改 harness / root skill / AGENTS.md

## 涉及工作流

- 无（GFTests 尚无 plugin-owned development skill）

## 验收标准（机器可判优先）

- [ ] `build_editor.py` 编译 0 error（UBT Exit 0）
- [ ] `launch_editor.py` 启动验证通过
- [ ] `.uplugin` 在 `Plugins/GameFeatures/GFTests/` 且含 `BuiltInInitialFeatureState`
- [ ] `scope_guard.py --event stop` 返回 `{"continue": true}`

## 爆炸半径

- 允许改动: `Plugins/GameFeatures/GFTests/**`
- 禁止改动: 其它路径

## 开放问题

- 无
