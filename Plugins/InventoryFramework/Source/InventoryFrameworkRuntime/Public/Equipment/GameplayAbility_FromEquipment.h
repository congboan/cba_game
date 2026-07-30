// Copyright Epic Games, Inc. All Rights Reserved.
// 改编自 LyraStarterGame

#pragma once

#include "Abilities/GameplayAbility.h"

#include "GameplayAbility_FromEquipment.generated.h"

class UEquipmentInstance;
class UInventoryItemInstance;

/**
 * UGameplayAbility_FromEquipment
 *
 * 由装备实例授予并与之关联的 Ability。
 * 允许 Ability 通过 SourceObject 链访问装备和提供装备的背包物品：
 *   AbilitySpec.SourceObject -> EquipmentInstance -> Instigator -> InventoryItemInstance
 */
UCLASS()
class INVENTORYFRAMEWORKRUNTIME_API UGameplayAbility_FromEquipment : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGameplayAbility_FromEquipment(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "Equipment|Ability")
	UEquipmentInstance* GetAssociatedEquipment() const;

	UFUNCTION(BlueprintCallable, Category = "Equipment|Ability")
	UInventoryItemInstance* GetAssociatedItem() const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
