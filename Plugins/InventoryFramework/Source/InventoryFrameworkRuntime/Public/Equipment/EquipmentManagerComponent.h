// Copyright Epic Games, Inc. All Rights Reserved.
// 改编自 LyraStarterGame

#pragma once

#include "Components/PawnComponent.h"
#include "Net/Serialization/FastArraySerializer.h"

#include "EquipmentManagerComponent.generated.h"

class UEquipmentDefinition;
class UEquipmentInstance;
class UEquipmentManagerComponent;
struct FFrame;
struct FEquipmentList;
struct FNetDeltaSerializeInfo;
struct FReplicationFlags;

/** 单个已应用的装备 */
USTRUCT(BlueprintType)
struct FAppliedEquipmentEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FAppliedEquipmentEntry()
	{}

	FString GetDebugString() const;

private:
	friend FEquipmentList;
	friend UEquipmentManagerComponent;

	// 被装备的装备类
	UPROPERTY()
	TSubclassOf<UEquipmentDefinition> EquipmentDefinition;

	UPROPERTY()
	TObjectPtr<UEquipmentInstance> Instance = nullptr;
};

/** 已应用的装备列表 */
USTRUCT(BlueprintType)
struct FEquipmentList : public FFastArraySerializer
{
	GENERATED_BODY()

	FEquipmentList()
		: OwnerComponent(nullptr)
	{
	}

	FEquipmentList(UActorComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

public:
	//~FFastArraySerializer contract
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32> ChangedIndices, int32 FinalSize);
	//~End of FFastArraySerializer contract

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FAppliedEquipmentEntry, FEquipmentList>(Entries, DeltaParms, *this);
	}

	UEquipmentInstance* AddEntry(TSubclassOf<UEquipmentDefinition> EquipmentDefinition);
	void RemoveEntry(UEquipmentInstance* Instance);

private:
	/** 遍历 Definition 的 Fragments，调用 OnEquipped。对标 FInventoryList::NotifyFragmentsOnCreate。 */
	void NotifyFragmentsOnEquip(UEquipmentInstance* Instance) const;

	/** 遍历 Definition 的 Fragments，调用 OnUnequipped。对标 FInventoryList::NotifyFragmentsOnRemove。 */
	void NotifyFragmentsOnUnequip(UEquipmentInstance* Instance) const;

	friend UEquipmentManagerComponent;

private:
	// 复制的装备条目列表
	UPROPERTY()
	TArray<FAppliedEquipmentEntry> Entries;

	UPROPERTY(NotReplicated)
	TObjectPtr<UActorComponent> OwnerComponent;
};

template<>
struct TStructOpsTypeTraits<FEquipmentList> : public TStructOpsTypeTraitsBase2<FEquipmentList>
{
	enum { WithNetDeltaSerializer = true };
};



/**
 * 管理应用到 Pawn 上的装备。
 *
 * 应作为 PawnComponent 添加（即 Outer = Pawn）。
 * 装备实例作为所属 Pawn 的子对象创建。
 *
 * 行为由 UEquipmentDefinition 中的 Fragment 驱动，
 * EquipmentManagerComponent 只负责列表管理和生命周期触发，不关心具体行为。
 */
UCLASS(BlueprintType, Const)
class INVENTORYFRAMEWORKRUNTIME_API UEquipmentManagerComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UEquipmentManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	UEquipmentInstance* EquipItem(TSubclassOf<UEquipmentDefinition> EquipmentDefinition);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	void UnequipItem(UEquipmentInstance* ItemInstance);

	//~UObject interface
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	//~End of UObject interface

	//~UActorComponent interface
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void ReadyForReplication() override;
	//~End of UActorComponent interface

	/** 返回给定类型的第一个已装备实例，未找到则返回 nullptr */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	UEquipmentInstance* GetFirstInstanceOfType(TSubclassOf<UEquipmentInstance> InstanceType);

	/** 返回给定类型的所有已装备实例，未找到则返回空数组 */
	UFUNCTION(BlueprintCallable, BlueprintPure)
	TArray<UEquipmentInstance*> GetEquipmentInstancesOfType(TSubclassOf<UEquipmentInstance> InstanceType) const;

	template <typename T>
	T* GetFirstInstanceOfType()
	{
		return (T*)GetFirstInstanceOfType(T::StaticClass());
	}

private:
	UPROPERTY(Replicated)
	FEquipmentList EquipmentList;
};
