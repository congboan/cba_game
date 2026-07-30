#pragma once

#include "Engine/DataAsset.h"
#include "ExperienceActionSet.generated.h"

class UGameFeatureAction;

UCLASS(BlueprintType, NotBlueprintable)
class EXPERIENCEFRAMEWORKRUNTIME_API UExperienceActionSet : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UExperienceActionSet();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

#if WITH_EDITORONLY_DATA
	virtual void UpdateAssetBundleData() override;
#endif

	UPROPERTY(EditAnywhere, Instanced, Category = "Actions")
	TArray<TObjectPtr<UGameFeatureAction>> Actions;

	UPROPERTY(EditAnywhere, Category = "Feature Dependencies")
	TArray<FString> GameFeaturesToEnable;
};
