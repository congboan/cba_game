#pragma once

#include "DataRegistryId.h"
#include "GameplayTagStack.h"
#include "Inventory/InventoryItemDef.h"
#include "InventoryItemInstance.generated.h"

struct FInventoryFragment_Base;

UCLASS(BlueprintType)
class INVENTORYFRAMEWORKRUNTIME_API UInventoryItemInstance : public UObject
{
	GENERATED_BODY()

public:
	UInventoryItemInstance(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual bool IsSupportedForNetworking() const override { return true; }
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ── Stat Tags ──

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void AddStatTagStack(FGameplayTag Tag, int32 StackCount);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Inventory")
	void RemoveStatTagStack(FGameplayTag Tag, int32 StackCount);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 GetStatTagStackCount(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool HasStatTag(FGameplayTag Tag) const;

	// ── Item Definition ──

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Inventory")
	FDataRegistryId GetItemDefinitionId() const { return ItemDefinitionId; }

	const FInventoryItemDef* GetItemDef() const;
	const FItemRow* GetItemRow() const;

	// ── Fragment Lookup ──

	const FInventoryFragment_Base* K2_FindFragment(UScriptStruct* Type) const;

	template <typename T>
	const T* FindFragment() const
	{
		return static_cast<const T*>(FindFragmentByType(T::StaticStruct()));
	}

	const FInventoryFragment_Base* FindFragmentByType(const UScriptStruct* Type) const;

private:
	void SetItemDefinitionId(const FDataRegistryId& InId);

	friend struct FInventoryList;

private:
	UPROPERTY(Replicated)
	FGameplayTagStackContainer StatTags;

	UPROPERTY(Replicated)
	FDataRegistryId ItemDefinitionId;
};
