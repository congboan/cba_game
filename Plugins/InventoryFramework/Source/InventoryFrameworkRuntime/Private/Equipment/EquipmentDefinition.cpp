// Copyright Epic Games, Inc. All Rights Reserved.
// 改编自 LyraStarterGame

#include "Equipment/EquipmentDefinition.h"
#include "Equipment/EquipmentInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipmentDefinition)

UEquipmentDefinition::UEquipmentDefinition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstanceType = UEquipmentInstance::StaticClass();
}
