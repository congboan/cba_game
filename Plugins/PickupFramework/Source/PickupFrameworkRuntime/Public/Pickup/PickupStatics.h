#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "PickupStatics.generated.h"

class UInventoryManagerComponent;

UCLASS()
class PICKUPFRAMEWORKRUNTIME_API UPickupStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Pickup")
	static bool TryPickupActor(AActor* PickupActor, UInventoryManagerComponent* InventoryManager);
};
