#pragma once

#include "UObject/Interface.h"
#include "InteractionInstigator.generated.h"

UINTERFACE(BlueprintType)
class INTERACTIONFRAMEWORKRUNTIME_API UInteractionFrameworkInstigator : public UInterface
{
	GENERATED_BODY()
};

class INTERACTIONFRAMEWORKRUNTIME_API IInteractionFrameworkInstigator
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Interaction")
	AActor* GetInteractionAvatar() const;
};
