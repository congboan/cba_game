#pragma once

#include "GameFramework/Actor.h"
#include "Interaction/InteractableTarget.h"
#include "Pickup/Pickupable.h"
#include "PickupActor.generated.h"

class UPickupDefinition;
class UInventoryManagerComponent;

UCLASS(Blueprintable)
class PICKUPFRAMEWORKRUNTIME_API APickupActor : public AActor, public IInteractionFrameworkTarget, public IPickupFrameworkPickupable
{
	GENERATED_BODY()

public:
	APickupActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GatherInteractionOptions_Implementation(const FInteractionFrameworkQuery& Query, TArray<FInteractionFrameworkOption>& OutOptions) const override;
	virtual bool TryGrantPickupToInventory_Implementation(UInventoryManagerComponent* InventoryManager) override;

	UFUNCTION(BlueprintCallable, Category = "Pickup")
	UPickupDefinition* GetPickupDefinition() const { return PickupDefinition; }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Pickup")
	TObjectPtr<UPickupDefinition> PickupDefinition;
};
