#pragma once

#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "Inventory/InventoryFragmentBase.h"
#include "InventoryItemDef.generated.h"

class UEquipmentDefinition;

/**
 * 模块化物品定义结构体。
 * 包含显示信息、分类和可组合 Fragment。
 * 存储在 DataTable 行中 — 每种物品类型一行。
 */
USTRUCT(BlueprintType)
struct INVENTORYFRAMEWORKRUNTIME_API FInventoryItemDef
{
	GENERATED_BODY()

	/** UI 中显示的物品名称。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText DisplayName;

	/** 用于过滤/排序的分类 Tag（例如 Item.Category.Weapon）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FGameplayTag ItemCategory;

	/** 最大堆叠数量（0 = 不可堆叠，1 = 单个，N = 最大）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stacking")
	int32 MaxStackSize = 1;

	/** 创建新堆叠时的最小堆叠数量（默认 = 1）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stacking")
	int32 StackCountOnCreate = 1;

	/** 定义此物品行为和数据的可组合 Fragment。 */
	UPROPERTY(EditAnywhere, Category = "Fragments")
	TArray<TInstancedStruct<FInventoryFragment_Base>> Fragments;

	/** 按类型查找 Fragment。O(n) — 如频繁调用请在 Instance 中缓存结果。 */
	const FInventoryFragment_Base* FindFragment(const UScriptStruct* Type) const
	{
		for (const TInstancedStruct<FInventoryFragment_Base>& Fragment : Fragments)
		{
			if (Fragment.GetScriptStruct() == Type)
			{
				return Fragment.GetPtr<FInventoryFragment_Base>();
			}
		}
		return nullptr;
	}

	template<typename T>
	const T* FindFragment() const
	{
		static_assert(std::is_base_of_v<FInventoryFragment_Base, T>, "T must derive from FInventoryFragment_Base");
		return static_cast<const T*>(FindFragment(T::StaticStruct()));
	}
};

/**
 * 物品定义的 DataTable 行。
 * 每行 = 一种物品类型（例如 "Sword_Iron_01"）。
 */
USTRUCT()
struct INVENTORYFRAMEWORKRUNTIME_API FItemRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Item")
	FInventoryItemDef Definition;
};
