---
dev_mode: "fragment-based stacking via base-class virtual interface"
required_skills: []
tool_providers: []
required_tool_capabilities: []
status: done
created: "2026-08-02"
constraints: []
---

# Inventory 堆叠策略（Fragment 基类虚接口方案）

## 做什么
- 扩展 FInventoryFragment_Base 基类虚接口：ShouldMergeOnPickup() / GetStackSizeLimit(Def) / CanSplitStack()
- 默认值 = Lyra 原生行为（不合并、上限 1、不可拆分）
- FInventoryFragment_Stacking 覆写这三个虚方法
- 组件只调用基类虚接口聚合，不出现任何具体 fragment 类型引用
- 新增 SplitStack（拆分 100->2x50）与 MergeStacks，同样只走基类接口

## 不做什么
- 不改 AddEntry 的无条件新建语义（保持 Lyra 兼容）
- 不改 ConsumeItemsByDefinitionId（与堆叠正交）
- 不改复制/网络路径
- 不改 FInventoryItemDef 字段结构（MaxStackSize 保留作默认值来源）

## 验收标准
- [ ] 编译 0 error（build_editor.py）
- [ ] 组件源文件中 grep 不到 FInventoryFragment_Stacking 引用
- [ ] 无 fragment 的物品 AddEntry 行为与改动前一致（每次新建）
- [ ] 配置 Stacking fragment 后：合并、按上限分批、拆分可用

## 爆炸半径
- 允许改动: Plugins/InventoryFramework/Source/InventoryFrameworkRuntime/Public/Inventory/ 与 Private/Inventory/
- 禁止改动: 其它路径
