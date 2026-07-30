// Copyright Epic Games, Inc. All Rights Reserved.

#include "Equipment/EquipmentFragment_SpawnActor.h"

#include "Components/SkeletalMeshComponent.h"
#include "Equipment/EquipmentInstance.h"
#include "GameFramework/Character.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipmentFragment_SpawnActor)

void FEquipmentFragment_SpawnActor::OnEquipped(UEquipmentInstance* Instance) const
{
	if (!Instance)
	{
		return;
	}

	APawn* OwningPawn = Instance->GetPawn();
	if (!OwningPawn)
	{
		return;
	}

	USceneComponent* AttachTarget = OwningPawn->GetRootComponent();
	if (ACharacter* Char = Cast<ACharacter>(OwningPawn))
	{
		AttachTarget = Char->GetMesh();
	}

	TArray<TObjectPtr<AActor>>& SpawnedActors = Instance->GetSpawnedActorsMutable();

	for (const FEquipmentActorToSpawn& SpawnInfo : ActorsToSpawn)
	{
		AActor* NewActor = Instance->GetWorld()->SpawnActorDeferred<AActor>(SpawnInfo.ActorToSpawn, FTransform::Identity, OwningPawn);
		NewActor->FinishSpawning(FTransform::Identity, /*bIsDefaultTransform=*/ true);
		NewActor->SetActorRelativeTransform(SpawnInfo.AttachTransform);
		NewActor->AttachToComponent(AttachTarget, FAttachmentTransformRules::KeepRelativeTransform, SpawnInfo.AttachSocket);

		SpawnedActors.Add(NewActor);
	}
}

void FEquipmentFragment_SpawnActor::OnUnequipped(UEquipmentInstance* Instance) const
{
	if (!Instance)
	{
		return;
	}

	TArray<TObjectPtr<AActor>>& SpawnedActors = Instance->GetSpawnedActorsMutable();

	for (AActor* Actor : SpawnedActors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}

	SpawnedActors.Empty();
}
