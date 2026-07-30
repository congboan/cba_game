// Copyright Epic Games, Inc. All Rights Reserved.
// 改编自 LyraStarterGame

#pragma once

#include "Templates/SubclassOf.h"
#include "StructUtils/InstancedStruct.h"
#include "Equipment/EquipmentFragmentBase.h"

#include "EquipmentDefinition.generated.h"

class UEquipmentInstance;

/**
 * UEquipmentDefinition
 *
 * 可应用到 Pawn 上的装备定义。
 * 使用 Fragment 组合模式（对标 FInventoryItemDef）：
 * - InstanceType：运行时装备实例类
 * - Fragments[]：可组合的装备行为（Actor 生成、能力授予等由 Fragment 负责）
 */
UCLASS(Blueprintable, Const, Abstract, BlueprintType)
class INVENTORYFRAMEWORKRUNTIME_API UEquipmentDefinition : public UObject
{
	GENERATED_BODY()

public:
	UEquipmentDefinition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// 作为运行时实例生成的类
	UPROPERTY(EditDefaultsOnly, Category = Equipment)
	TSubclassOf<UEquipmentInstance> InstanceType;

	// 装备时触发的可组合行为 Fragment
	UPROPERTY(EditDefaultsOnly, Category = "Fragments")
	TArray<TInstancedStruct<FEquipmentFragment_Base>> Fragments;
};
