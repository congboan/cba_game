#include "Inventory/InventoryItemInstance.h"

#include "DataRegistrySubsystem.h"
#include "Engine/DataTable.h"
#include "Inventory/InventoryItemDef.h"
#include "Net/UnrealNetwork.h"

UInventoryItemInstance::UInventoryItemInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UInventoryItemInstance::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, StatTags);
	DOREPLIFETIME(ThisClass, ItemDefinitionId);
}

void UInventoryItemInstance::AddStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	StatTags.AddStack(Tag, StackCount);
}

void UInventoryItemInstance::RemoveStatTagStack(FGameplayTag Tag, int32 StackCount)
{
	StatTags.RemoveStack(Tag, StackCount);
}

int32 UInventoryItemInstance::GetStatTagStackCount(FGameplayTag Tag) const
{
	return StatTags.GetStackCount(Tag);
}

bool UInventoryItemInstance::HasStatTag(FGameplayTag Tag) const
{
	return StatTags.ContainsTag(Tag);
}

const FInventoryItemDef* UInventoryItemInstance::GetItemDef() const
{
	if (!ItemDefinitionId.IsValid())
	{
		return nullptr;
	}

	const FItemRow* Row = GetItemRow();
	return Row ? &Row->Definition : nullptr;
}

const FItemRow* UInventoryItemInstance::GetItemRow() const
{
	if (!ItemDefinitionId.IsValid())
	{
		return nullptr;
	}

	if (const UDataRegistrySubsystem* Registry = UDataRegistrySubsystem::Get())
	{
		return Registry->GetCachedItem<FItemRow>(ItemDefinitionId);
	}

	return nullptr;
}

const FInventoryFragment_Base* UInventoryItemInstance::K2_FindFragment(UScriptStruct* Type) const
{
	return FindFragmentByType(Type);
}

const FInventoryFragment_Base* UInventoryItemInstance::FindFragmentByType(const UScriptStruct* Type) const
{
	if (Type == nullptr)
	{
		return nullptr;
	}

	const FInventoryItemDef* Def = GetItemDef();
	if (Def == nullptr)
	{
		return nullptr;
	}

	return Def->FindFragment(Type);
}

void UInventoryItemInstance::SetItemDefinitionId(const FDataRegistryId& InId)
{
	ItemDefinitionId = InId;
}
