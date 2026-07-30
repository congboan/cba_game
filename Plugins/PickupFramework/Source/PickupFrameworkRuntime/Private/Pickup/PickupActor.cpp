#include "Pickup/PickupActor.h"

#include "Inventory/InventoryManagerComponent.h"
#include "Pickup/PickupDefinition.h"
#include "System/PickupFrameworkLog.h"

#define LOCTEXT_NAMESPACE "PickupFramework"

APickupActor::APickupActor(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bReplicates = true;
}

void APickupActor::GatherInteractionOptions_Implementation(const FInteractionFrameworkQuery& Query, TArray<FInteractionFrameworkOption>& OutOptions) const
{
	if (PickupDefinition == nullptr)
	{
		return;
	}

	FInteractionFrameworkOption Option;
	Option.DisplayText = PickupDefinition->DisplayText.IsEmpty()
		? LOCTEXT("DefaultPickupInteractionText", "拾取")
		: PickupDefinition->DisplayText;
	Option.SourceObject = const_cast<APickupActor*>(this);
	Option.TargetActor = const_cast<APickupActor*>(this);
	OutOptions.Add(Option);
}

bool APickupActor::TryGrantPickupToInventory_Implementation(UInventoryManagerComponent* InventoryManager)
{
	if (PickupDefinition == nullptr || InventoryManager == nullptr)
	{
		return false;
	}

	AActor* InventoryOwner = InventoryManager->GetOwner();
	if (InventoryOwner == nullptr || !InventoryOwner->HasAuthority())
	{
		UE_LOG(LogPickupFramework, Verbose, TEXT("拾取授予被忽略，因为库存所有者没有权限。"));
		return false;
	}

	bool bGrantedAnyItem = false;
	for (const FPickupItemGrant& Grant : PickupDefinition->ItemGrants)
	{
		if (Grant.ItemDefinitionId.IsValid() && InventoryManager->CanAddItemByDefinitionId(Grant.ItemDefinitionId, Grant.StackCount))
		{
			bGrantedAnyItem |= InventoryManager->AddItemByDefinitionId(Grant.ItemDefinitionId, Grant.StackCount) != nullptr;
		}
	}

	if (bGrantedAnyItem && PickupDefinition->bDestroyActorAfterPickup)
	{
		Destroy();
	}

	return bGrantedAnyItem;
}

#undef LOCTEXT_NAMESPACE
