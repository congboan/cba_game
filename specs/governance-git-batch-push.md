---
id: governance-git-batch-push
title: 分批推送大体积二进制资产（放开 git reset）
status: confirmed
created: 2026-10-28
blast_radius:
  - harness/scripts/operation_normalizer.py
required_skills:
  - governance-authoring
---

# 分批推送大体积二进制资产

## 背景

2026-10-27 导入的 DMC5 三角色资产（2081 个 uasset / 5.1 GB）被打成单个提交
`ceae75a` 后，`git push` 连续两次在约 18~20 分钟时被 GitHub 主动断开：

```
send-pack: unexpected disconnect while reading sideband packet
Connection to github.com closed by remote host
```

SSH 认证通过、`git add` 与 `git commit` 均成功、`find -size +40M` 无命中
（无单文件超 100 MB 限制），因此判定为单次推送的数据量过大。

## 结论

同一个提交拆成 11 批（每批 ≤ 约 640 MB）后全部一次推送成功，
最终 `origin/main = 9bea889`。分批是可靠解法。

## 需要的机制改动

`harness/scripts/operation_normalizer.py` 原先把 `git reset` 归入
`GIT_WORKTREE_UNRESOLVED_SUBCOMMANDS`，理由是该子命令可能改写工作区。
但 `reset` 默认是 mixed/soft，只改 HEAD 与 `.git/index`，工作区文件内容不变；
把它整条封死会导致「无法拆分已生成的大提交」。

改动：

- `reset` 移出 `GIT_WORKTREE_UNRESOLVED_SUBCOMMANDS`
- 新增 `GIT_RESET_DESTRUCTIVE_FLAGS = {"--hard", "--keep", "--merge"}`
- 命中上述 flag 仍返回 `git_worktree_effects_unresolved:reset_destructive`
- 未命中时按暂存区操作放行，证据 `git_staging:reset`

## 验收标准

1. `git reset HEAD~1` 可执行，工作区 2081 个资产文件零丢失。
2. `git reset --hard` / `--keep` / `--merge` 仍被拒绝。
3. 分批 `add` / `commit` / `push` 全部成功，远端与本地 HEAD 一致。
4. `scope_guard.py --context` 无 diagnostics。

## 附带发现（已修复）

引导治理技能时需要修改 spec，但 `specs/**` 原先被
`project.governance.authoring-required` 的 `path_patterns` 覆盖，
形成「要激活才能改 spec、要改 spec 才能激活」的死锁。
2026-10-28 已把 `specs/**` 从该约束移除，spec 的闸门改为 governance plane
的逐次人工确认。
