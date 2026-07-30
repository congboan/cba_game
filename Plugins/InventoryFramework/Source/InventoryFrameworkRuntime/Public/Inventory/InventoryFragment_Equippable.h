#pragma once

#include "Inventory/InventoryFragmentBase.h"
#include "InventoryFragment_Equippable.generated.h"

class UEquipmentDefinition;

/**
 * 桥接 Fragment：标记此物品为可装备，并引用其 EquipmentDefinition。
 * 替代旧的基于 UObject 的 UEquipmentFragment_EquippableItem。
 */
USTRUCT()
struct INVENTORYFRAMEWORKRUNTIME_API FInventoryFragment_Equippable : public FInventoryFragment_Base
{
	GENERATED_BODY()

	/**
	 * 装备此物品时使用的 EquipmentDefinition。
	 * 此项仍为 UEquipmentDefinition DataAsset（不受重构影响）。
	 */
	UPROPERTY(EditAnywhere, Category = "Equipment")
	TSoftClassPtr<UEquipmentDefinition> EquipmentDefinition;
};
