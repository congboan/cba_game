#pragma once

#include "GameplayTagContainer.h"
#include "Inventory/InventoryFragmentBase.h"
#include "InventoryFragment_Stats.generated.h"

/**
 * 在物品实例上设置初始 GameplayTag 属性。
 * 功能等同于旧的 UInventoryStatsFragment。
 */
USTRUCT()
struct INVENTORYFRAMEWORKRUNTIME_API FInventoryFragment_Stats : public FInventoryFragment_Base
{
	GENERATED_BODY()

	/** 实例创建时应用的初始属性 Tag 及其值。 */
	UPROPERTY(EditAnywhere, Category = "Stats")
	TMap<FGameplayTag, int32> InitialStats;

	virtual void OnInstanceCreated(UInventoryItemInstance* Instance) const override;
};
