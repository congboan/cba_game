// Copyright Epic Games, Inc. All Rights Reserved.
// 改编自 LyraStarterGame

#include "Equipment/EquipmentInstance.h"

#include "Equipment/EquipmentDefinition.h"
#include "Equipment/EquipmentFragmentBase.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

#include "Iris/ReplicationSystem/ReplicationFragmentUtil.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(EquipmentInstance)

class FLifetimeProperty;
class UClass;

UEquipmentInstance::UEquipmentInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UWorld* UEquipmentInstance::GetWorld() const
{
	if (APawn* OwningPawn = GetPawn())
	{
		return OwningPawn->GetWorld();
	}
	else
	{
		return nullptr;
	}
}

void UEquipmentInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, Instigator);
	DOREPLIFETIME(ThisClass, SpawnedActors);
}

void UEquipmentInstance::RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags)
{
	using namespace UE::Net;

	// 为此对象构建描述符并分配 PropertyReplicationFragment
	FReplicationFragmentUtil::CreateAndRegisterFragmentsForObject(this, Context, RegistrationFlags);
}

APawn* UEquipmentInstance::GetPawn() const
{
	return Cast<APawn>(GetOuter());
}

APawn* UEquipmentInstance::GetTypedPawn(TSubclassOf<APawn> PawnType) const
{
	APawn* Result = nullptr;
	if (UClass* ActualPawnType = PawnType)
	{
		if (GetOuter()->IsA(ActualPawnType))
		{
			Result = Cast<APawn>(GetOuter());
		}
	}
	return Result;
}

const UEquipmentDefinition* UEquipmentInstance::GetEquipmentDefinition() const
{
	if (EquipmentDefinition)
	{
		return GetDefault<UEquipmentDefinition>(EquipmentDefinition);
	}
	return nullptr;
}

const FEquipmentFragment_Base* UEquipmentInstance::K2_FindFragment(UScriptStruct* Type) const
{
	return FindFragmentByType(Type);
}

const FEquipmentFragment_Base* UEquipmentInstance::FindFragmentByType(const UScriptStruct* Type) const
{
	const UEquipmentDefinition* Def = GetEquipmentDefinition();
	if (!Def)
	{
		return nullptr;
	}

	for (const TInstancedStruct<FEquipmentFragment_Base>& Fragment : Def->Fragments)
	{
		if (Fragment.GetScriptStruct() == Type)
		{
			return Fragment.GetPtr<FEquipmentFragment_Base>();
		}
	}

	return nullptr;
}

void UEquipmentInstance::OnEquipped()
{
	K2_OnEquipped();
}

void UEquipmentInstance::OnUnequipped()
{
	K2_OnUnequipped();
}

void UEquipmentInstance::OnRep_Instigator()
{
}
