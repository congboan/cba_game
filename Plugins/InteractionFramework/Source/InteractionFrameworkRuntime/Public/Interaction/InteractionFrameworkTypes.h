#pragma once

#include "GameplayTagContainer.h"
#include "InteractionFrameworkTypes.generated.h"

class AActor;
class AController;
class UObject;

USTRUCT(BlueprintType)
struct INTERACTIONFRAMEWORKRUNTIME_API FInteractionFrameworkQuery
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<AActor> RequestingAvatar = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<AController> RequestingController = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	FVector TraceStart = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	FVector TraceEnd = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct INTERACTIONFRAMEWORKRUNTIME_API FInteractionFrameworkOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText DisplayText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FGameplayTag InteractionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	float InteractionDuration = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<UObject> SourceObject = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Interaction")
	TObjectPtr<AActor> TargetActor = nullptr;
};
