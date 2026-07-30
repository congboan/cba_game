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
	void BroadcastChangeMessage(FInventoryEntry& Entry, int32 OldCount, int32 NewCount);
	void NotifyFragmentsOnCreate(UInventoryItemInstance* Instance);
	void NotifyFragmentsOnRemove(UInventoryItemInstance* Instance);

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

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Inventory")
	TArray<UInventoryItemInstance*> GetAllItems() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	UInventoryItemInstance* FindFirstItemStackByDefinitionId(FDataRegistryId ItemDefinitionId) const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	int32 GetTotalItemCountByDefinitionId(FDataRegistryId ItemDefinitionId) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	bool ConsumeItemsByDefinitionId(FDataRegistryId ItemDefinitionId, int32 NumToConsume);

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FInventoryChangedDynamicDelegate OnInventoryChanged;

private:
	friend struct FInventoryList;

	void BroadcastInventoryChanged(UInventoryItemInstance* Instance, int32 NewCount, int32 Delta);

private:
	UPROPERTY(Replicated)
	FInventoryList InventoryList;
};
