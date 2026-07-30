#include "Inventory/InventoryManagerComponent.h"

#include "DataRegistrySubsystem.h"
#include "Engine/ActorChannel.h"
#include "GameFramework/Actor.h"
#include "Inventory/InventoryItemDef.h"
#include "Inventory/InventoryItemInstance.h"
#include "Inventory/InventoryFragmentBase.h"
#include "Net/UnrealNetwork.h"

FString FInventoryEntry::GetDebugString() const
{
	FDataRegistryId DefId;
	if (Instance != nullptr)
	{
		DefId = Instance->GetItemDefinitionId();
	}

	return FString::Printf(TEXT("%s (%d x %s)"), *GetNameSafe(Instance), StackCount, *DefId.ToString());
}

void FInventoryList::PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize)
{
	for (int32 Index : RemovedIndices)
	{
		FInventoryEntry& Entry = Entries[Index];
		BroadcastChangeMessage(Entry, Entry.StackCount, 0);
		Entry.LastObservedCount = 0;
	}
}

void FInventoryList::PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize)
{
	for (int32 Index : AddedIndices)
	{
		FInventoryEntry& Entry = Entries[Index];
		BroadcastChangeMessage(Entry, 0, Entry.StackCount);
		Entry.LastObservedCount = Entry.StackCount;
	}
}

void FInventoryList::PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize)
{
	for (int32 Index : ChangedIndices)
	{
		FInventoryEntry& Entry = Entries[Index];
		const int32 OldCount = Entry.LastObservedCount == INDEX_NONE ? 0 : Entry.LastObservedCount;
		BroadcastChangeMessage(Entry, OldCount, Entry.StackCount);
		Entry.LastObservedCount = Entry.StackCount;
	}
}

void FInventoryList::BroadcastChangeMessage(FInventoryEntry& Entry, int32 OldCount, int32 NewCount)
{
	if (OwnerComponent != nullptr)
	{
		OwnerComponent->BroadcastInventoryChanged(Entry.Instance, NewCount, NewCount - OldCount);
	}
}

void FInventoryList::NotifyFragmentsOnCreate(UInventoryItemInstance* Instance)
{
	const FInventoryItemDef* Def = Instance->GetItemDef();
	if (Def == nullptr) return;

	for (const TInstancedStruct<FInventoryFragment_Base>& Fragment : Def->Fragments)
	{
		if (const FInventoryFragment_Base* FragPtr = Fragment.GetPtr<FInventoryFragment_Base>())
		{
			FragPtr->OnInstanceCreated(Instance);
		}
	}
}

void FInventoryList::NotifyFragmentsOnRemove(UInventoryItemInstance* Instance)
{
	const FInventoryItemDef* Def = Instance->GetItemDef();
	if (Def == nullptr) return;

	for (const TInstancedStruct<FInventoryFragment_Base>& Fragment : Def->Fragments)
	{
		if (const FInventoryFragment_Base* FragPtr = Fragment.GetPtr<FInventoryFragment_Base>())
		{
			FragPtr->OnInstanceRemoved(Instance);
		}
	}
}

UInventoryItemInstance* FInventoryList::AddEntry(const FDataRegistryId& ItemDefinitionId, int32 StackCount)
{
	check(ItemDefinitionId.IsValid());
	check(OwnerComponent != nullptr);

	AActor* OwningActor = OwnerComponent->GetOwner();
	check(OwningActor != nullptr && OwningActor->HasAuthority());

	FInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Instance = NewObject<UInventoryItemInstance>(OwningActor);
	NewEntry.Instance->SetItemDefinitionId(ItemDefinitionId);
	NewEntry.StackCount = FMath::Max(1, StackCount);

	NotifyFragmentsOnCreate(NewEntry.Instance);

	MarkItemDirty(NewEntry);
	BroadcastChangeMessage(NewEntry, 0, NewEntry.StackCount);

	return NewEntry.Instance;
}

void FInventoryList::AddEntry(UInventoryItemInstance* Instance, int32 StackCount)
{
	if (Instance == nullptr)
	{
		return;
	}

	FInventoryEntry& NewEntry = Entries.AddDefaulted_GetRef();
	NewEntry.Instance = Instance;
	NewEntry.StackCount = FMath::Max(1, StackCount);

	MarkItemDirty(NewEntry);
	BroadcastChangeMessage(NewEntry, 0, NewEntry.StackCount);
}

void FInventoryList::RemoveEntry(UInventoryItemInstance* Instance)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FInventoryEntry& Entry = *EntryIt;
		if (Entry.Instance == Instance)
		{
			const int32 OldCount = Entry.StackCount;

			NotifyFragmentsOnRemove(Instance);

			EntryIt.RemoveCurrent();
			MarkArrayDirty();

			if (OwnerComponent != nullptr)
			{
				OwnerComponent->BroadcastInventoryChanged(Instance, 0, -OldCount);
			}
			return;
		}
	}
}

int32 FInventoryList::GetStackCount(UInventoryItemInstance* Instance) const
{
	for (const FInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance == Instance)
		{
			return Entry.StackCount;
		}
	}

	return 0;
}

TArray<UInventoryItemInstance*> FInventoryList::GetAllItems() const
{
	TArray<UInventoryItemInstance*> Results;
	Results.Reserve(Entries.Num());
	for (const FInventoryEntry& Entry : Entries)
	{
		if (Entry.Instance != nullptr)
		{
			Results.Add(Entry.Instance);
		}
	}
	return Results;
}

UInventoryManagerComponent::UInventoryManagerComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, InventoryList(this)
{
	SetIsReplicatedByDefault(true);
}

void UInventoryManagerComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, InventoryList);
}

bool UInventoryManagerComponent::CanAddItemByDefinitionId(FDataRegistryId ItemDefinitionId, int32 StackCount) const
{
	if (!ItemDefinitionId.IsValid() || StackCount <= 0)
	{
		return false;
	}

	// 可选地根据定义的 MaxStackSize 进行检查
	const UDataRegistrySubsystem* Registry = UDataRegistrySubsystem::Get();
	if (Registry)
	{
		if (const FItemRow* Row = Registry->GetCachedItem<FItemRow>(ItemDefinitionId))
		{
			if (Row->Definition.MaxStackSize > 0)
			{
				// 查找此定义的已有堆叠
				const int32 ExistingCount = GetTotalItemCountByDefinitionId(ItemDefinitionId);
				if (ExistingCount + StackCount > Row->Definition.MaxStackSize)
				{
					return false;
				}
			}
		}
	}

	return true;
}

UInventoryItemInstance* UInventoryManagerComponent::AddItemByDefinitionId(FDataRegistryId ItemDefinitionId, int32 StackCount)
{
	if (!CanAddItemByDefinitionId(ItemDefinitionId, StackCount))
	{
		return nullptr;
	}

	UInventoryItemInstance* Result = InventoryList.AddEntry(ItemDefinitionId, StackCount);
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication() && Result != nullptr)
	{
		AddReplicatedSubObject(Result);
	}

	return Result;
}

void UInventoryManagerComponent::AddItemInstance(UInventoryItemInstance* ItemInstance, int32 StackCount)
{
	if (ItemInstance == nullptr || StackCount <= 0)
	{
		return;
	}

	InventoryList.AddEntry(ItemInstance, StackCount);
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication())
	{
		AddReplicatedSubObject(ItemInstance);
	}
}

void UInventoryManagerComponent::RemoveItemInstance(UInventoryItemInstance* ItemInstance)
{
	InventoryList.RemoveEntry(ItemInstance);

	if (ItemInstance != nullptr && IsUsingRegisteredSubObjectList())
	{
		RemoveReplicatedSubObject(ItemInstance);
	}
}

TArray<UInventoryItemInstance*> UInventoryManagerComponent::GetAllItems() const
{
	return InventoryList.GetAllItems();
}

UInventoryItemInstance* UInventoryManagerComponent::FindFirstItemStackByDefinitionId(FDataRegistryId ItemDefinitionId) const
{
	for (const FInventoryEntry& Entry : InventoryList.Entries)
	{
		if (Entry.Instance != nullptr && Entry.Instance->GetItemDefinitionId() == ItemDefinitionId)
		{
			return Entry.Instance;
		}
	}

	return nullptr;
}

int32 UInventoryManagerComponent::GetTotalItemCountByDefinitionId(FDataRegistryId ItemDefinitionId) const
{
	int32 TotalCount = 0;
	for (const FInventoryEntry& Entry : InventoryList.Entries)
	{
		if (Entry.Instance != nullptr && Entry.Instance->GetItemDefinitionId() == ItemDefinitionId)
		{
			TotalCount += Entry.StackCount;
		}
	}

	return TotalCount;
}

bool UInventoryManagerComponent::ConsumeItemsByDefinitionId(FDataRegistryId ItemDefinitionId, int32 NumToConsume)
{
	if (!ItemDefinitionId.IsValid() || NumToConsume <= 0 || GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return false;
	}

	if (GetTotalItemCountByDefinitionId(ItemDefinitionId) < NumToConsume)
	{
		return false;
	}

	TArray<UInventoryItemInstance*> StacksToRemove;

	int32 Remaining = NumToConsume;
	for (FInventoryEntry& Entry : InventoryList.Entries)
	{
		if (Entry.Instance == nullptr || Entry.Instance->GetItemDefinitionId() != ItemDefinitionId)
		{
			continue;
		}

		const int32 ConsumeFromThisStack = FMath::Min(Remaining, Entry.StackCount);
		Entry.StackCount -= ConsumeFromThisStack;
		Remaining -= ConsumeFromThisStack;

		InventoryList.MarkEntryDirty(Entry);

		if (Entry.StackCount <= 0)
		{
			StacksToRemove.Add(Entry.Instance);
		}

		if (Remaining <= 0)
		{
			break;
		}
	}

	for (UInventoryItemInstance* Instance : StacksToRemove)
	{
		RemoveItemInstance(Instance);
	}

	return true;
}

void UInventoryManagerComponent::ReadyForReplication()
{
	Super::ReadyForReplication();

	if (IsUsingRegisteredSubObjectList())
	{
		for (const FInventoryEntry& Entry : InventoryList.Entries)
		{
			if (Entry.Instance != nullptr)
			{
				AddReplicatedSubObject(Entry.Instance);
			}
		}
	}
}

bool UInventoryManagerComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (FInventoryEntry& Entry : InventoryList.Entries)
	{
		if (Entry.Instance != nullptr)
		{
			bWroteSomething |= Channel->ReplicateSubobject(Entry.Instance, *Bunch, *RepFlags);
		}
	}

	return bWroteSomething;
}

void UInventoryManagerComponent::BroadcastInventoryChanged(UInventoryItemInstance* Instance, int32 NewCount, int32 Delta)
{
	OnInventoryChanged.Broadcast(Instance, NewCount, Delta);
}
