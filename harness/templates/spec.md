---
dev_mode: ""              # 本需求采用的实现做法/路线（自由填写，通常对应某个工作流 skill）
required_skills: []        # 由 spec 构建阶段根据需求确定；skill 无权把自己加入或修改 selector
                           # 只引用任务执行所需宿主，不复制 skill 内容或约束
                           # 开发 plugin 本身时可引用其 junction 暴露的 development skill；普通使用/依赖 plugin 时不引用
tool_providers: []         # 本需求专用的工具语义/能力声明；不证明客户端已安装该工具
required_tool_capabilities: []  # 本需求必须可解析到有效 provider 的能力
status: draft             # draft | confirmed | done；只有 confirmed 可由 state.active_spec 选择，status 本身不激活 spec
created: "YYYY-MM-DD"
# 本需求的爆炸半径约束（统一 schema；spec 归档/删除即回收）
# evaluator 枚举见 harness/scripts/scope_guard.py EVALUATORS；禁止逻辑组合。
# spec 不得使用 script；复杂门禁通过 required_skills 引用 workflow skill。
constraints: []
#  非规范示例：只演示 schema，不代表当前项目规则；不得直接取消注释使用。
#  - id: spec.example.allowed-path
#    evaluator: path_glob
#    when: pre_write
#    data: {pattern: "__EXAMPLE_ALLOWED_PATH__/**", invert: true}  # invert: 命中“不在允许区”才拦
#    action: deny
#    reason: "示例：本需求只允许修改明确确认的目标路径"
---

# <需求标题>

## 做什么
- （一句话说清要交付什么）

## 不做什么（边界）
- （明确排除项，防止范围蔓延）

## 涉及工作流
- （在 spec 构建/确认过程中由对话确定本需求执行所需的 workflow skill；skill 不自行选择）
- （若开发 plugin 本身，列出对应 plugin-owned development skill；若只是使用 plugin，不得据此加载开发 skill）

## 验收标准（机器可判优先）
- [ ] 编译 0 error（build_editor.py）
- [ ] （可机器检查的条目）
- [ ] （必要时才写需人工判断的条目）

## 爆炸半径
- 允许改动: （路径范围，与 frontmatter.constraints 对应）
- 禁止改动: （路径范围）

## 开放问题（澄清阶段遗留，进入 build 前须关闭）
- （无）
