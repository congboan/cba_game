#pragma once

#include "Engine/DataAsset.h"
#include "ExperienceDefinition.generated.h"

class UExperienceActionSet;
class UExperiencePawnData;
class UGameFeatureAction;

UCLASS(BlueprintType, Const)
class EXPERIENCEFRAMEWORKRUNTIME_API UExperienceDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UExperienceDefinition();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

#if WITH_EDITORONLY_DATA
	virtual void UpdateAssetBundleData() override;
#endif

	UPROPERTY(EditDefaultsOnly, Category = "Gameplay")
	TArray<FString> GameFeaturesToEnable;

	UPROPERTY(EditDefaultsOnly, Category = "Gameplay")
	TObjectPtr<const UExperiencePawnData> DefaultPawnData;

	UPROPERTY(EditDefaultsOnly, Instanced, Category = "Actions")
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	UPROPERTY(EditDefaultsOnly, Category = "Gameplay")
	TArray<TObjectPtr<UExperienceActionSet>> ActionSets;
};
