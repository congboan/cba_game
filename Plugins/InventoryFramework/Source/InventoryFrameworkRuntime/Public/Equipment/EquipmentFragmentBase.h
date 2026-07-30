#pragma once

#include "StructUtils/InstancedStruct.h"
#include "EquipmentFragmentBase.generated.h"

class UEquipmentInstance;

/**
 * 装备定义的基类 Fragment 结构体。
 * 使用 USTRUCT + 虚函数分发（无 UObject 开销）。
 * 对标 FInventoryFragment_Base，继承此类为装备定义添加模块化的数据/行为。
 */
USTRUCT()
struct INVENTORYFRAMEWORKRUNTIME_API FEquipmentFragment_Base
{
	GENERATED_BODY()

	virtual ~FEquipmentFragment_Base() = default;

	/** 装备激活时调用。 */
	virtual void OnEquipped(UEquipmentInstance* Instance) const {}

	/** 装备卸下时调用。 */
	virtual void OnUnequipped(UEquipmentInstance* Instance) const {}
};
