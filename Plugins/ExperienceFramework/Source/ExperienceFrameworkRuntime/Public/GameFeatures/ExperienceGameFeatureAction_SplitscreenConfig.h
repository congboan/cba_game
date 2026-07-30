#pragma once

#include "GameFeatures/ExperienceGameFeatureAction_WorldActionBase.h"
#include "UObject/ObjectKey.h"
#include "ExperienceGameFeatureAction_SplitscreenConfig.generated.h"

UCLASS(meta = (DisplayName = "Splitscreen Config"))
class EXPERIENCEFRAMEWORKRUNTIME_API UExperienceGameFeatureAction_SplitscreenConfig final : public UExperienceGameFeatureAction_WorldActionBase
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;
	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext) override;

	UPROPERTY(EditAnywhere, Category = "Action")
	bool bDisableSplitscreen = true;

private:
	TArray<FObjectKey> LocalDisableVotes;

	static TMap<FObjectKey, int32> GlobalDisableVotes;
};
