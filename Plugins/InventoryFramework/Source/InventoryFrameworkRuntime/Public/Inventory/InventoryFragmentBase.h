#pragma once

#include "StructUtils/InstancedStruct.h"
#include "InventoryFragmentBase.generated.h"

class UInventoryItemInstance;
struct FInventoryItemDef;

/**
 * 物品定义的基类 Fragment 结构体。
 * 使用 USTRUCT + 虚函数分发（无 UObject 开销）。
 * 继承此类为物品定义添加模块化的数据/行为。
 */
USTRUCT()
struct INVENTORYFRAMEWORKRUNTIME_API FInventoryFragment_Base
{
	GENERATED_BODY()

	virtual ~FInventoryFragment_Base() = default;

	/** 当从包含此 Fragment 的定义创建物品实例时调用。 */
	virtual void OnInstanceCreated(UInventoryItemInstance* Instance) const {}

	/** 当物品实例从背包中移除时调用。 */
	virtual void OnInstanceRemoved(UInventoryItemInstance* Instance) const {}

	/**
	 * 当物品实例存活期间堆叠数量发生变化时调用（Old > 0 且 New > 0）。
	 * 新建条目（0→N）走 OnInstanceCreated；条目移除（N→0）走 OnInstanceRemoved，
	 * 这两个边界不触发本回调。
	 */
	virtual void OnStackCountChanged(UInventoryItemInstance* Instance, int32 OldCount, int32 NewCount) const {}

	// ── 堆叠策略虚接口（默认值 = Lyra 原生行为） ──

	/** 添加到已有堆叠时是否合并（默认 false = 不合并，与 Lyra 一致）。 */
	virtual bool ShouldMergeOnPickup() const { return false; }

	/** 此 Fragment 允许的单堆叠最大数量（默认 1 = 不可堆叠）。 */
	virtual int32 GetStackSizeLimit(const FInventoryItemDef* Def) const { return 1; }

	/** 是否允许拆分堆叠（默认 false = 不可拆分）。 */
	virtual bool CanSplitStack() const { return false; }
};
