#pragma once

#include "GameFeatureAction.h"
#include "UObject/SoftObjectPath.h"
#include "ExperienceGameFeatureAction_AddGameplayCuePath.generated.h"

UCLASS(meta = (DisplayName = "Add Gameplay Cue Path"))
class EXPERIENCEFRAMEWORKRUNTIME_API UExperienceGameFeatureAction_AddGameplayCuePath final : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	UExperienceGameFeatureAction_AddGameplayCuePath();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	const TArray<FDirectoryPath>& GetDirectoryPathsToAdd() const { return DirectoryPathsToAdd; }

private:
	UPROPERTY(EditAnywhere, Category = "Game Feature | Gameplay Cues", meta = (RelativeToGameContentDir, LongPackageName))
	TArray<FDirectoryPath> DirectoryPathsToAdd;
};
