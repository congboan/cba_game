---
dev_mode: "ConsumeStack API + OnStackCountChanged fragment event"
required_skills: []
tool_providers: []
required_tool_capabilities: []
status: done
created: "2026-08-02"
constraints: []
---

# Inventory ConsumeStack 与 StackCount 变化事件

## 做什么
- 新增 `UInventoryManagerComponent::ConsumeStack(ItemInstance, NumToConsume)`：
  - 从**指定堆叠**扣除 N 个数量（区别于按 DefId 跨栈 FCFS 的 `ConsumeItemsByDefinitionId`）
  - 部分扣除：`StackCount` 减少，走标准复制路径（`MarkItemDirty` → `PostReplicatedChange`）
  - 耗尽（扣除后 StackCount == 0）：移除该条目（`RemoveItemInstance` → `PreReplicatedRemove` + `OnInstanceRemoved`），不重复触发数量变化事件
  - 非法输入（空实例 / 不在背包 / 数量 <= 0 / 数量超过当前堆叠）返回 false
- 扩展 `FInventoryFragment_Base` 虚接口：`OnStackCountChanged(Instance, OldCount, NewCount)`，默认空实现
- 组件在 StackCount 变化的 4 个点调用（遍历 fragment 基类虚接口，不出现具体类型引用）：
  - AddEntry 合并已有堆叠（`StackCount += AmountToAdd`）
  - ConsumeItemsByDefinitionId 部分扣除（`StackCount -= ConsumeFromThisStack`）
  - ConsumeStack 部分扣除
  - SplitStack 源堆叠减少 / MergeStacks 目标增加与源减少

## 事件语义边界（与既有 Create/Remove 事件正交）
- 新建条目（0→N）：只走 `OnInstanceCreated`，不调 `OnStackCountChanged`
- 条目移除（N→0）：只走 `OnInstanceRemoved`，不调 `OnStackCountChanged`
- 条目存活期间的数量变化（Old>0, New>0）：调 `OnStackCountChanged(Instance, Old, New)`

## 不做什么
- 不改 `ConsumeItemsByDefinitionId` 的跨栈 FCFS 语义（保留，与 ConsumeStack 并存）
- 不改复制/网络路径（FastArray delta 序列化不动）
- 不改 `FInventoryEntry` 字段结构
- 不新增 fragment 具体类型（只在基类加虚接口）

## 验收标准
- [ ] 编译 0 error（build_editor.py）
- [ ] 组件源文件中 grep 不到任何具体 fragment 类型引用（保持只走基类虚接口）
- [ ] ConsumeStack 部分扣除：StackCount 正确减少，`PostReplicatedChange` + `OnStackCountChanged(Old, New)` 各一次
- [ ] ConsumeStack 耗尽：条目移除，`PreReplicatedRemove` + `OnInstanceRemoved` 一次，不重复触发 `OnStackCountChanged`
- [ ] AddEntry 合并已有堆叠时触发 `OnStackCountChanged`
- [ ] 无 fragment 的物品（默认空实现）行为与改动前一致

## 爆炸半径
- 允许改动: Plugins/InventoryFramework/Source/InventoryFrameworkRuntime/Public/Inventory/ 与 Private/Inventory/
- 禁止改动: 其它路径
