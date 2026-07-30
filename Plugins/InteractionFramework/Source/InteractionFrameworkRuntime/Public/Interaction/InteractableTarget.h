#pragma once

#include "UObject/Interface.h"
#include "Interaction/InteractionFrameworkTypes.h"
#include "InteractableTarget.generated.h"

UINTERFACE(BlueprintType)
class INTERACTIONFRAMEWORKRUNTIME_API UInteractionFrameworkTarget : public UInterface
{
	GENERATED_BODY()
};

class INTERACTIONFRAMEWORKRUNTIME_API IInteractionFrameworkTarget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	void GatherInteractionOptions(const FInteractionFrameworkQuery& Query, TArray<FInteractionFrameworkOption>& OutOptions) const;
};
