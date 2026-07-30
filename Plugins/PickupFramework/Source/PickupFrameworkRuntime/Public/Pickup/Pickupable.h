#pragma once

#include "UObject/Interface.h"
#include "Pickupable.generated.h"

class UInventoryManagerComponent;

UINTERFACE(BlueprintType)
class PICKUPFRAMEWORKRUNTIME_API UPickupFrameworkPickupable : public UInterface
{
	GENERATED_BODY()
};

class PICKUPFRAMEWORKRUNTIME_API IPickupFrameworkPickupable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pickup")
	bool TryGrantPickupToInventory(UInventoryManagerComponent* InventoryManager);
};
