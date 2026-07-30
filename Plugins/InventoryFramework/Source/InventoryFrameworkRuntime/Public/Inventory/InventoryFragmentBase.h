#pragma once

#include "StructUtils/InstancedStruct.h"
#include "InventoryFragmentBase.generated.h"

class UInventoryItemInstance;

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
};
