# AssemblyAbilityFramework — Tag Namespace 规范

本文件是 `Config/DefaultAssemblyAbilityTags.ini` 的命名空间规范说明（ini 第 5 行引用处）。
所有标签由 ini 声明，供项目侧通过 Project Settings → GameplayTags → Import Tags from Config
导入 `DefaultGameplayTags.ini` 后使用。

## 命名空间一览

| 命名空间 | 用途 | 例子 |
|---|---|---|
| `Event.*` | 链事件域：EventRouter 监听源 + GA 触发监听目标 | `Event.Attack.Hit`、`Event.Ability.Cast` |
| `Emit.*` | 技能发射身份；`GiveAbilityFromTag` 以其 TagName 组成 DataRegistry Id（类型固定 "AssemblyAbility"） | `Emit.Fireball` |
| `Origin.*` | 资源/出处标识符 | `Origin.FrostArrow` |
| `State.*` | 控制状态类长效 GE 标签；配 Row.ActivationBlockedTags 或引擎 block/immunity 使用 | `State.Stunned`、`State.Silenced` |
| `Proc.*` | 被动概率触发身份（TriggerTag 侧） | `Proc.Crit`、`Proc.Bash` |
| `Talent.*` | 天赋树槽位 | `Talent.Ogre.S1` |
| `Buff.*` / `Debuff.*` | 增益/减益 GE 归属标识 | `Buff.Haste`、`Debuff.Burn` |
| `Cue.*` | GameplayCue 展示域 | `Cue.Fireball.Impact` |

已废弃：`Fragment.*` meta tag 域（Fragment 层已移除，勿再登记）。

## 新增规则

1. 先确认归属 namespace 已存在；不存在时在 ini 新增父级声明行再挂子 tag。
2. 子段使用 PascalCase 分层（`Event.Damage.AreaDealt`），父级仅作聚合语义。
3. 需要代码常量时走 NativeGameplayTag（`Source/.../Public/AssemblyAbilityTags.h`
   的 `AssemblyAbilityTags` 命名空间），ini 与 native 二选一，禁止双源。
4. Trigger 配对纪律：TriggerTag 必须来自响应方真实监听的域（GA SetTriggers / EventRouter 派发目标），
   ListenTag 必须与上游 SendChainEvent 的 EventTag 同字面量。
