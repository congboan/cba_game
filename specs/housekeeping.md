---
dev_mode: "housekeeping umbrella"
required_skills: []
tool_providers: []
required_tool_capabilities: []
status: confirmed
created: "2026-08-13"
constraints:
  - id: spec.housekeeping.allowed-path
    evaluator: path_glob
    when: pre_write
    data:
      pattern: "**"
      exempt_patterns:
        - "Source/cba_game/**"
        - "Config/**"
        - "docs/**"
        - "specs/**"
        - "harness/state/**"
        - ".workbuddy/skills/**"
        - ".workbuddy/memory/**"
    action: deny
    reason: "housekeeping scope only"
---

# housekeeping umbrella

日常小修：重构、修 bug、加日志、改配置。

## 验收

- 编译 0 error
- stop 门禁通过

## 爆炸半径

- 允许：Source/cba_game/**、Config/**、docs/**
- 禁止：其它路径


