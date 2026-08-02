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

void FInventoryList::NotifyFragmentsOnStackCountChanged(UInventoryItemInstance* Instance, int32 OldCount, int32 NewCount)
{
	if (Instance == nullptr || OldCount == NewCount)
	{
		return;
	}

	const FInventoryItemDef* Def = Instance->GetItemDef();
	if (Def == nullptr) return;

	for (const TInstancedStruct<FInventoryFragment_Base>& Fragment : Def->Fragments)
	{
		if (const FInventoryFragment_Base* FragPtr = Fragment.GetPtr<FInventoryFragment_Base>())
		{
			FragPtr->OnStackCountChanged(Instance, OldCount, NewCount);
		}
	}
}

UInventoryItemInstance* FInventoryList::AddEntryAsNewStack(const FDataRegistryId& ItemDefinitionId, int32 StackCount)
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

	return NewEntry.Instance;
}

UInventoryItemInstance* FInventoryList::AddEntry(const FDataRegistryId& ItemDefinitionId, int32 StackCount)
{
	check(ItemDefinitionId.IsValid());
	check(OwnerComponent != nullptr);

	AActor* OwningActor = OwnerComponent->GetOwner();
	check(OwningActor != nullptr && OwningActor->HasAuthority());

	const FInventoryItemDef* Def = nullptr;
	if (const UDataRegistrySubsystem* Registry = UDataRegistrySubsystem::Get())
	{
		if (const FItemRow* Row = Registry->GetCachedItem<FItemRow>(ItemDefinitionId))
		{
			Def = &Row->Definition;
		}
	}

	// 通过基类虚接口聚合所有 fragment 的堆叠策略。
	// 默认值（无 fragment / 未覆写）= Lyra 原生行为：不合并、上限 1。
	bool bMergeable = false;
	int32 StackLimit = 1;
	if (Def != nullptr)
	{
		for (const TInstancedStruct<FInventoryFragment_Base>& Fragment : Def->Fragments)
		{
			if (const FInventoryFragment_Base* FragPtr = Fragment.GetPtr<FInventoryFragment_Base>())
			{
				bMergeable |= FragPtr->ShouldMergeOnPickup();
				StackLimit = FMath::Max(StackLimit, FragPtr->GetStackSizeLimit(Def));
			}
		}
	}

	UInventoryItemInstance* ReturnedInstance = nullptr;
	int32 Remaining = FMath::Max(1, StackCount);

	if (bMergeable && StackLimit > 1)
	{
		// 1) 先填满已有的未满堆叠
		for (FInventoryEntry& Entry : Entries)
		{
			if (Remaining <= 0)
			{
				break;
			}

			if (Entry.Instance == nullptr || Entry.Instance->GetItemDefinitionId() != ItemDefinitionId)
			{
				continue;
			}

			const int32 SpaceInStack = StackLimit - Entry.StackCount;
			if (SpaceInStack > 0)
			{
				const int32 OldCount = Entry.StackCount;
				const int32 AmountToAdd = FMath::Min(Remaining, SpaceInStack);
				Entry.StackCount += AmountToAdd;
				Remaining -= AmountToAdd;
				MarkItemDirty(Entry);

				NotifyFragmentsOnStackCountChanged(Entry.Instance, OldCount, Entry.StackCount);

				if (ReturnedInstance == nullptr)
				{
					ReturnedInstance = Entry.Instance;
				}
			}
		}

		// 2) 剩余数量按 StackLimit 分批新建堆叠
		while (Remaining > 0)
		{
			const int32 NewStackCount = FMath::Min(Remaining, StackLimit);
			Remaining -= NewStackCount;

			UInventoryItemInstance* NewInstance = AddEntryAsNewStack(ItemDefinitionId, NewStackCount);

			if (ReturnedInstance == nullptr)
			{
				ReturnedInstance = NewInstance;
			}
		}
	}
	else if (StackLimit > 1)
	{
		// 可堆叠但不合并：按 StackLimit 分批新建独立堆叠
		while (Remaining > 0)
		{
			const int32 NewStackCount = FMath::Min(Remaining, StackLimit);
			Remaining -= NewStackCount;

			UInventoryItemInstance* NewInstance = AddEntryAsNewStack(ItemDefinitionId, NewStackCount);

			if (ReturnedInstance == nullptr)
			{
				ReturnedInstance = NewInstance;
			}
		}
	}
	else
	{
		// 不可堆叠：Lyra 原生行为，每次调用新建一个条目
		ReturnedInstance = AddEntryAsNewStack(ItemDefinitionId, Remaining);
	}

	return ReturnedInstance;
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
}

void FInventoryList::RemoveEntry(UInventoryItemInstance* Instance)
{
	for (auto EntryIt = Entries.CreateIterator(); EntryIt; ++EntryIt)
	{
		FInventoryEntry& Entry = *EntryIt;
		if (Entry.Instance == Instance)
		{
			NotifyFragmentsOnRemove(Instance);

			EntryIt.RemoveCurrent();
			MarkArrayDirty();
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

	// MaxStackSize 是单堆叠上限（0/1 = 不可堆叠），不是持有总量上限。
	// 因此只要物品定义存在即可添加：可堆叠物品会合并/分批新建堆叠；
	// 不可堆叠物品每次添加 1 个条目。不在此处限制总量。
	if (const UDataRegistrySubsystem* Registry = UDataRegistrySubsystem::Get())
	{
		const FItemRow* Row = Registry->GetCachedItem<FItemRow>(ItemDefinitionId);
		return Row != nullptr;
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

bool UInventoryManagerComponent::SplitStack(UInventoryItemInstance* ItemInstance, int32 NewStackCount)
{
	if (ItemInstance == nullptr || GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return false;
	}

	// 拆分后原堆叠保留的数量必须 >= 1（NewStackCount == 0 表示全部拆出，会创建一个空条目，不允许）。
	if (NewStackCount < 1)
	{
		return false;
	}

	FInventoryEntry* SourceEntry = nullptr;
	for (FInventoryEntry& Entry : InventoryList.Entries)
	{
		if (Entry.Instance == ItemInstance)
		{
			SourceEntry = &Entry;
			break;
		}
	}
	if (SourceEntry == nullptr)
	{
		return false;
	}

	const int32 CurrentCount = SourceEntry->StackCount;
	if (NewStackCount >= CurrentCount)
	{
		return false; // 拆分后原堆叠必须变少
	}

	const int32 AmountToSplit = CurrentCount - NewStackCount;

	// 受 Fragment 策略约束：任一 fragment 允许拆分才可拆分。
	bool bCanSplit = false;
	const FInventoryItemDef* Def = ItemInstance->GetItemDef();
	if (Def != nullptr)
	{
		for (const TInstancedStruct<FInventoryFragment_Base>& Fragment : Def->Fragments)
		{
			if (const FInventoryFragment_Base* FragPtr = Fragment.GetPtr<FInventoryFragment_Base>())
			{
				bCanSplit |= FragPtr->CanSplitStack();
			}
		}
	}
	if (!bCanSplit)
	{
		return false;
	}

	SourceEntry->StackCount = NewStackCount;
	InventoryList.MarkEntryDirty(*SourceEntry);

	// 源堆叠仍存活（NewStackCount >= 1），数量从 CurrentCount 变为 NewStackCount
	InventoryList.NotifyFragmentsOnStackCountChanged(ItemInstance, CurrentCount, NewStackCount);

	// 拆出的数量强制作为独立新堆叠（不合并回源堆叠）：
	// 走 AddEntryAsNewStack 自动触发 NotifyFragmentsOnCreate（初始 stats 等）。
	UInventoryItemInstance* SplitInstance = InventoryList.AddEntryAsNewStack(ItemInstance->GetItemDefinitionId(), AmountToSplit);
	if (SplitInstance != nullptr && IsUsingRegisteredSubObjectList() && IsReadyForReplication())
	{
		AddReplicatedSubObject(SplitInstance);
	}
	return SplitInstance != nullptr;
}

bool UInventoryManagerComponent::MergeStacks(UInventoryItemInstance* SourceInstance, UInventoryItemInstance* TargetInstance, int32 Amount)
{
	if (SourceInstance == nullptr || TargetInstance == nullptr || SourceInstance == TargetInstance)
	{
		return false;
	}
	if (GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return false;
	}
	if (Amount <= 0)
	{
		return false;
	}
	if (SourceInstance->GetItemDefinitionId() != TargetInstance->GetItemDefinitionId())
	{
		return false; // 仅同一定义可合并
	}

	FInventoryEntry* SourceEntry = nullptr;
	FInventoryEntry* TargetEntry = nullptr;
	for (FInventoryEntry& Entry : InventoryList.Entries)
	{
		if (Entry.Instance == SourceInstance)
		{
			SourceEntry = &Entry;
		}
		else if (Entry.Instance == TargetInstance)
		{
			TargetEntry = &Entry;
		}
	}
	if (SourceEntry == nullptr || TargetEntry == nullptr)
	{
		return false;
	}

	const int32 AmountToMove = FMath::Min(Amount, SourceEntry->StackCount);
	if (AmountToMove <= 0)
	{
		return false;
	}

	// 受 Fragment 策略约束：目标必须有剩余空间且允许合并。
	// 通过基类虚接口聚合；无 fragment 时默认不可合并（与 Lyra 一致）。
	bool bTargetMergeable = false;
	int32 TargetStackLimit = 1;
	const FInventoryItemDef* Def = TargetInstance->GetItemDef();
	if (Def != nullptr)
	{
		for (const TInstancedStruct<FInventoryFragment_Base>& Fragment : Def->Fragments)
		{
			if (const FInventoryFragment_Base* FragPtr = Fragment.GetPtr<FInventoryFragment_Base>())
			{
				bTargetMergeable |= FragPtr->ShouldMergeOnPickup();
				TargetStackLimit = FMath::Max(TargetStackLimit, FragPtr->GetStackSizeLimit(Def));
			}
		}
	}
	if (!bTargetMergeable)
	{
		return false;
	}

	const int32 SpaceInTarget = TargetStackLimit - TargetEntry->StackCount;
	if (SpaceInTarget <= 0)
	{
		return false;
	}

	const int32 AmountToMerge = FMath::Min(AmountToMove, SpaceInTarget);

	const int32 TargetOldCount = TargetEntry->StackCount;
	TargetEntry->StackCount += AmountToMerge;
	InventoryList.MarkEntryDirty(*TargetEntry);
	InventoryList.NotifyFragmentsOnStackCountChanged(TargetInstance, TargetOldCount, TargetEntry->StackCount);

	const int32 SourceOldCount = SourceEntry->StackCount;
	SourceEntry->StackCount -= AmountToMerge;
	if (SourceEntry->StackCount <= 0)
	{
		// 源堆叠耗尽，直接移除条目（OnInstanceRemoved，不重复触发数量变化事件）
		UInventoryItemInstance* SourceInstanceToRemove = SourceEntry->Instance;
		InventoryList.RemoveEntry(SourceInstanceToRemove);
		if (IsUsingRegisteredSubObjectList())
		{
			RemoveReplicatedSubObject(SourceInstanceToRemove);
		}
	}
	else
	{
		InventoryList.MarkEntryDirty(*SourceEntry);
		// 源堆叠仍存活，数量变化触发事件
		InventoryList.NotifyFragmentsOnStackCountChanged(SourceInstance, SourceOldCount, SourceEntry->StackCount);
	}

	return true;
}

TArray<UInventoryItemInstance*> UInventoryManagerComponent::GetAllItems() const
{
	return InventoryList.GetAllItems();
}

int32 UInventoryManagerComponent::GetStackCount(UInventoryItemInstance* ItemInstance) const
{
	return InventoryList.GetStackCount(ItemInstance);
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
		const int32 OldCount = Entry.StackCount;
		Entry.StackCount -= ConsumeFromThisStack;
		Remaining -= ConsumeFromThisStack;

		InventoryList.MarkEntryDirty(Entry);

		if (Entry.StackCount <= 0)
		{
			StacksToRemove.Add(Entry.Instance);
		}
		else
		{
			// 条目存活期间的数量变化才触发；耗尽走 RemoveItemInstance 的 OnInstanceRemoved
			InventoryList.NotifyFragmentsOnStackCountChanged(Entry.Instance, OldCount, Entry.StackCount);
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

bool UInventoryManagerComponent::ConsumeStack(UInventoryItemInstance* ItemInstance, int32 NumToConsume)
{
	if (ItemInstance == nullptr || NumToConsume <= 0 || GetOwner() == nullptr || !GetOwner()->HasAuthority())
	{
		return false;
	}

	FInventoryEntry* TargetEntry = nullptr;
	for (FInventoryEntry& Entry : InventoryList.Entries)
	{
		if (Entry.Instance == ItemInstance)
		{
			TargetEntry = &Entry;
			break;
		}
	}
	if (TargetEntry == nullptr || TargetEntry->StackCount < NumToConsume)
	{
		return false;
	}

	const int32 OldCount = TargetEntry->StackCount;
	TargetEntry->StackCount -= NumToConsume;

	if (TargetEntry->StackCount <= 0)
	{
		// 耗尽：移除条目，走 OnInstanceRemoved（不重复触发 OnStackCountChanged）
		InventoryList.MarkEntryDirty(*TargetEntry);
		RemoveItemInstance(ItemInstance);
	}
	else
	{
		// 部分扣除：条目存活，数量变化触发事件
		InventoryList.MarkEntryDirty(*TargetEntry);
		InventoryList.NotifyFragmentsOnStackCountChanged(ItemInstance, OldCount, TargetEntry->StackCount);
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
