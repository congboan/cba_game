#pragma once

#include "DataRegistryId.h"
#include "Engine/DataAsset.h"
#include "PickupDefinition.generated.h"

USTRUCT(BlueprintType)
struct PICKUPFRAMEWORKRUNTIME_API FPickupItemGrant
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	FDataRegistryId ItemDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup", meta = (ClampMin = "1"))
	int32 StackCount = 1;
};

UCLASS(BlueprintType, Const)
class PICKUPFRAMEWORKRUNTIME_API UPickupDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup")
	FText DisplayText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup")
	TArray<FPickupItemGrant> ItemGrants;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Pickup")
	bool bDestroyActorAfterPickup = true;
};
