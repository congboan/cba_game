---
name: __PROJECT_NAME__
description: "__PROJECT_NAME__ 项目根 skill。用于承载经人确认的项目知识、配表和项目生命周期约束。"
tool_providers: []              # 项目常驻的工具语义/能力声明；不是安装清单或授权白名单
required_tool_capabilities: []  # 项目始终需要的能力；通常应优先放入按需 workflow/spec
constraints: []
---

# __PROJECT_NAME__

本文件是项目 root skill 的空骨架。请与用户确认后再补充项目事实、配表和约束；不得从示例、历史
memory 或未确认假设自动生成项目规则。

## 已确认的项目事实

- （待与用户确认）

## 项目级配表与约束

- 在 frontmatter 的 `constraints` 中填写经确认的项目规则；每条 evaluator 所需数据放在该
  constraint 的 `data` 中，不建立 root 专用隐式配表。
- 复杂项目门禁脚本放在本 skill 的 `checks/` 中，以 owner-relative `script` evaluator 引用。
- 仅当项目始终需要某种工具能力时填写 `tool_providers` / `required_tool_capabilities`；
  按任务变化的声明应放入对应 workflow skill 或 spec。
- 做法性知识放入独立 workflow skill；单个需求边界放入 spec。
