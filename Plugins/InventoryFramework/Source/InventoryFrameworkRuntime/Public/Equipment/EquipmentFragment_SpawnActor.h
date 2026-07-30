#pragma once

#include "Equipment/EquipmentFragmentBase.h"
#include "EquipmentFragment_SpawnActor.generated.h"

class AActor;

/**
 * 装备时在 Pawn 上生成的 Actor 配置。
 * 从原 UEquipmentDefinition::FEquipmentActorToSpawn 迁移。
 */
USTRUCT()
struct FEquipmentActorToSpawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Equipment)
	TSubclassOf<AActor> ActorToSpawn;

	UPROPERTY(EditAnywhere, Category = Equipment)
	FName AttachSocket;

	UPROPERTY(EditAnywhere, Category = Equipment)
	FTransform AttachTransform;
};

/**
 * 内置 Fragment：装备时在 Pawn 上生成/销毁 Actor。
 * 功能等同于原 UEquipmentDefinition::ActorsToSpawn +
 * UEquipmentInstance::SpawnEquipmentActors / DestroyEquipmentActors。
 */
USTRUCT()
struct INVENTORYFRAMEWORKRUNTIME_API FEquipmentFragment_SpawnActor : public FEquipmentFragment_Base
{
	GENERATED_BODY()

	/** 装备时生成的 Actor 列表。 */
	UPROPERTY(EditDefaultsOnly, Category = "Equipment")
	TArray<FEquipmentActorToSpawn> ActorsToSpawn;

	virtual void OnEquipped(UEquipmentInstance* Instance) const override;
	virtual void OnUnequipped(UEquipmentInstance* Instance) const override;
};
