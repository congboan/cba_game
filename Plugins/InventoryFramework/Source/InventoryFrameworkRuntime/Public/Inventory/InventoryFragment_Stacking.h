#pragma once

#include "Inventory/InventoryFragmentBase.h"
#include "InventoryFragment_Stacking.generated.h"

struct FInventoryItemDef;

/**
 * 堆叠策略 Fragment：按物品类型声明堆叠行为。
 * 优先级高于 FInventoryItemDef 上的 MaxStackSize。
 *
 * 未配置此 Fragment 的物品保持 Lyra 原生行为（每次添加都新建条目，不合并），
 * 因此现有物品定义无需任何改动即可保持兼容。
 *
 * 组件只通过基类虚接口（ShouldMergeOnPickup / GetStackSizeLimit / CanSplitStack）
 * 读取策略，不直接访问本类型的字段。
 */
USTRUCT()
struct INVENTORYFRAMEWORKRUNTIME_API FInventoryFragment_Stacking : public FInventoryFragment_Base
{
	GENERATED_BODY()

	/** 拾取/添加时是否合并到已有未满堆叠（默认 true）。 */
	UPROPERTY(EditAnywhere, Category = "Stacking")
	bool bMergeOnPickup = true;

	/** 是否允许玩家拆分堆叠（例如 100 → 2x50）。 */
	UPROPERTY(EditAnywhere, Category = "Stacking")
	bool bCanSplit = false;

	/** 覆盖定义的 MaxStackSize（0 = 使用定义值；1 = 单堆叠）。 */
	UPROPERTY(EditAnywhere, Category = "Stacking")
	int32 MaxStackSizeOverride = 0;

	// ── 基类虚接口覆写 ──

	virtual bool ShouldMergeOnPickup() const override { return bMergeOnPickup; }

	virtual bool CanSplitStack() const override { return bCanSplit; }

	virtual int32 GetStackSizeLimit(const FInventoryItemDef* Def) const override;
};
