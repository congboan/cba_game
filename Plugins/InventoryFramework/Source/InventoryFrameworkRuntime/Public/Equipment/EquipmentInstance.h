// Copyright Epic Games, Inc. All Rights Reserved.
// 改编自 LyraStarterGame

#pragma once

#include "Engine/World.h"
#include "StructUtils/InstancedStruct.h"

#include "EquipmentInstance.generated.h"

class AActor;
class APawn;
class UEquipmentDefinition;
struct FEquipmentFragment_Base;
struct FFrame;

/**
 * UEquipmentInstance
 *
 * 生成并应用到 Pawn 上的装备实例。
 * 运行时状态，回指其 UEquipmentDefinition 以支持 FindFragment<T>()。
 */
UCLASS(BlueprintType, Blueprintable)
class INVENTORYFRAMEWORKRUNTIME_API UEquipmentInstance : public UObject
{
	GENERATED_BODY()

public:
	UEquipmentInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	//~UObject interface
	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual UWorld* GetWorld() const override final;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	//~End of UObject interface

	UFUNCTION(BlueprintPure, Category = Equipment)
	UObject* GetInstigator() const { return Instigator; }

	void SetInstigator(UObject* InInstigator) { Instigator = InInstigator; }

	UFUNCTION(BlueprintPure, Category = Equipment)
	APawn* GetPawn() const;

	UFUNCTION(BlueprintPure, Category = Equipment, meta = (DeterminesOutputType = "PawnType"))
	APawn* GetTypedPawn(TSubclassOf<APawn> PawnType) const;

	UFUNCTION(BlueprintPure, Category = Equipment)
	TArray<AActor*> GetSpawnedActors() const { return SpawnedActors; }

	// ── EquipmentDefinition ──

	/** 设置此实例对应的装备定义类（由 FEquipmentList::AddEntry 调用）。 */
	void SetEquipmentDefinition(TSubclassOf<UEquipmentDefinition> InDef) { EquipmentDefinition = InDef; }

	/** 获取此实例对应的装备定义 CDO。 */
	const UEquipmentDefinition* GetEquipmentDefinition() const;

	// ── Fragment Lookup ──

	/** 按类型查找 Fragment（脚本/蓝图用）。 */
	const FEquipmentFragment_Base* K2_FindFragment(UScriptStruct* Type) const;

	/** 按类型查找 Fragment（C++ 模板）。 */
	template <typename T>
	const T* FindFragment() const
	{
		static_assert(std::is_base_of_v<FEquipmentFragment_Base, T>, "T must derive from FEquipmentFragment_Base");
		return static_cast<const T*>(FindFragmentByType(T::StaticStruct()));
	}

	const FEquipmentFragment_Base* FindFragmentByType(const UScriptStruct* Type) const;

	// ── SpawnedActors 可变访问（供 Fragment 使用） ──

	TArray<TObjectPtr<AActor>>& GetSpawnedActorsMutable() { return SpawnedActors; }

	// ── 生命周期 ──

	virtual void OnEquipped();
	virtual void OnUnequipped();

protected:
	/** 注册所有复制 Fragment */
	virtual void RegisterReplicationFragments(UE::Net::FFragmentRegistrationContext& Context, UE::Net::EFragmentRegistrationFlags RegistrationFlags) override;

	UFUNCTION(BlueprintImplementableEvent, Category = Equipment, meta = (DisplayName = "OnEquipped"))
	void K2_OnEquipped();

	UFUNCTION(BlueprintImplementableEvent, Category = Equipment, meta = (DisplayName = "OnUnequipped"))
	void K2_OnUnequipped();

private:
	UFUNCTION()
	void OnRep_Instigator();

private:
	UPROPERTY(ReplicatedUsing = OnRep_Instigator)
	TObjectPtr<UObject> Instigator;

	UPROPERTY(Replicated)
	TArray<TObjectPtr<AActor>> SpawnedActors;

	/** 此装备实例对应的 UEquipmentDefinition 子类（不复制，仅在服务器端设置）。 */
	UPROPERTY()
	TSubclassOf<UEquipmentDefinition> EquipmentDefinition;
};
