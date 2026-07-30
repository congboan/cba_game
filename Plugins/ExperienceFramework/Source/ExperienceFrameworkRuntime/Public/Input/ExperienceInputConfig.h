#pragma once

#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ExperienceInputConfig.generated.h"

class UInputAction;

USTRUCT(BlueprintType)
struct FExperienceInputAction
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<const UInputAction> InputAction = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (Categories = "InputTag"))
	FGameplayTag InputTag;
};

UCLASS(BlueprintType, Const)
class EXPERIENCEFRAMEWORKRUNTIME_API UExperienceInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UExperienceInputConfig(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "Experience|Input")
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	UFUNCTION(BlueprintCallable, Category = "Experience|Input")
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputAction"))
	TArray<FExperienceInputAction> NativeInputActions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input", meta = (TitleProperty = "InputAction"))
	TArray<FExperienceInputAction> AbilityInputActions;
};
