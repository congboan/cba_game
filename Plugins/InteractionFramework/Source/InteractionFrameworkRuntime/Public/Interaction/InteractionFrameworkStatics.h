#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Interaction/InteractionFrameworkTypes.h"
#include "InteractionFrameworkStatics.generated.h"

UCLASS()
class INTERACTIONFRAMEWORKRUNTIME_API UInteractionFrameworkStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Interaction")
	static void AppendInteractionOptionsFromHitResults(const FInteractionFrameworkQuery& Query, const TArray<FHitResult>& HitResults, TArray<FInteractionFrameworkOption>& OutOptions);

	UFUNCTION(BlueprintCallable, Category = "Interaction")
	static void AppendInteractionOptionsFromActor(const FInteractionFrameworkQuery& Query, AActor* TargetActor, TArray<FInteractionFrameworkOption>& OutOptions);
};
