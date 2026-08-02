#pragma once

#include "Components/ActorComponent.h"
#include "DataRegistryId.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "InventoryManagerComponent.generated.h"

class UInventoryItemInstance;
struct FInventoryItemDef;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FInventoryChangedDynamicDelegate, UInventoryItemInstance*, Instance, int32, NewCount, int32, Delta);

USTRUCT(BlueprintType)
struct INVENTORYFRAMEWORKRUNTIME_API FInventoryChangeMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UActorComponent> InventoryOwner = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UInventoryItemInstance> Instance = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 NewCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Inventory")
	int32 Delta = 0;
};

USTRUCT(BlueprintType)
struct INVENTORYFRAMEWORKRUNTIME_API FInventoryEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FString GetDebugString() const;

private:
	friend struct FInventoryList;
	friend class UInventoryManagerComponent;

	UPROPERTY()
	TObjectPtr<UInventoryItemInstance> Instance = nullptr;

	UPROPERTY()
	int32 StackCount = 0;

	UPROPERTY(NotReplicated)
	int32 LastObservedCount = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct INVENTORYFRAMEWORKRUNTIME_API FInventoryList : public FFastArraySerializer
{
	GENERATED_BODY()

	FInventoryList() {}
	FInventoryList(UInventoryManagerComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

	TArray<UInventoryItemInstance*> GetAllItems() const;

	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FInventoryEntry, FInventoryList>(Entries, DeltaParms, *this);
	}

	UInventoryItemInstance* AddEntry(const FDataRegistryId& ItemDefinitionId, int32 StackCount);
	void AddEntry(UInventoryItemInstance* Instance, int32 StackCount);
	void RemoveEntry(UInventoryItemInstance* Instance);
	int32 GetStackCount(UInventoryItemInstance* Instance) const;

	/** 标记特定条目为脏以进行复制。仅从服务器端调用。 */
	void MarkEntryDirty(FInventoryEntry& Entry) { MarkItemDirty(Entry); }

private:
	/** 无条件新建一个独立条目（不尝试合并）。用于拆分等需要独立堆叠的场景。 */
	UInventoryItemInstance* AddEntryAsNewStack(const FDataRegistryId& ItemDefinitionId, int32 StackCount);

	void BroadcastChangeMessage(FInventoryEntry& Entry, int32 OldCount, int32 NewCount);
	void NotifyFragmentsOnCreate(UInventoryItemInstance* Instance);
	void NotifyFragmentsOnRemove(UInventoryItemInstance* Instance);
	void NotifyFragmentsOnStackCountChanged(UInventoryItemInstance* Instance, int32 OldCount, int32 NewCount);

private:
	friend class UInventoryManagerComponent;

	UPROPERTY()
	TArray<FInventoryEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UInventoryManagerComponent> OwnerComponent;
};

template<>
struct TStructOpsTypeTraits<FInventoryList> : public TStructOpsTypeTraitsBase2<FInventoryList>
{
	enum { WithNetDeltaSerializer = true };
};

UCLASS(BlueprintType, ClassGroup = "Inventory", meta = (BlueprintSpawnableComponent))
class INVENTORYFRAMEWORKRUNTIME_API UInventoryManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void ReadyForReplication() override;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	virtual bool CanAddItemByDefinitionId(FDataRegistryId ItemDefinitionId, int32 StackCount = 1) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	UInventoryItemInstance* AddItemByDefinitionId(FDataRegistryId ItemDefinitionId, int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void AddItemInstance(UInventoryItemInstance* ItemInstance, int32 StackCount = 1);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void RemoveItemInstance(UInventoryItemInstance* ItemInstance);

	/** 拆分堆叠：将 ItemInstance 的一部分数量拆到新条目（受 fragment 基类虚接口 CanSplitStack 约束）。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool SplitStack(UInventoryItemInstance* ItemInstance, int32 NewStackCount);

	/** 手动合并：将 SourceInstance 的 Amount 数量合并到 TargetInstance（两者须为同一定义）。 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool MergeStacks(UInventoryItemInstance* SourceInstance, UInventoryItemInstance* TargetInstance, int32 Amount);

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory")
	TArray<UInventoryItemInstance*> GetAllItems() const;

	/** 返回指定实例当前的堆叠数量（不在背包中则返回 0）。 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetStackCount(UInventoryItemInstance* ItemInstance) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	UInventoryItemInstance* FindFirstItemStackByDefinitionId(FDataRegistryId ItemDefinitionId) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetTotalItemCountByDefinitionId(FDataRegistryId ItemDefinitionId) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool ConsumeItemsByDefinitionId(FDataRegistryId ItemDefinitionId, int32 NumToConsume);

	/**
	 * 从指定堆叠扣除 NumToConsume 个数量（区别于按 DefId 跨栈 FCFS 的 ConsumeItemsByDefinitionId）。
	 * 部分扣除：StackCount 减少，触发 OnStackCountChanged(Old, New)。
	 * 耗尽（扣除后为 0）：移除该条目，只触发 OnInstanceRemoved，不重复触发数量变化事件。
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool ConsumeStack(UInventoryItemInstance* ItemInstance, int32 NumToConsume);

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FInventoryChangedDynamicDelegate OnInventoryChanged;

private:
	friend struct FInventoryList;

	void BroadcastInventoryChanged(UInventoryItemInstance* Instance, int32 NewCount, int32 Delta);

private:
	UPROPERTY(Replicated)
	FInventoryList InventoryList;
};
