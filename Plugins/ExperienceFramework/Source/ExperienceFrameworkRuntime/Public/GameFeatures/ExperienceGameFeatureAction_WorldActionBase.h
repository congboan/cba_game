#pragma once

#include "GameFeatureAction.h"
#include "GameFeaturesSubsystem.h"
#include "ExperienceGameFeatureAction_WorldActionBase.generated.h"

class UGameInstance;
struct FWorldContext;

UCLASS(Abstract)
class EXPERIENCEFRAMEWORKRUNTIME_API UExperienceGameFeatureAction_WorldActionBase : public UGameFeatureAction
{
	GENERATED_BODY()

public:
	virtual void OnGameFeatureActivating(FGameFeatureActivatingContext& Context) override;
	virtual void OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context) override;

private:
	void HandleGameInstanceStart(UGameInstance* GameInstance, FGameFeatureStateChangeContext ChangeContext);

	virtual void AddToWorld(const FWorldContext& WorldContext, const FGameFeatureStateChangeContext& ChangeContext)
		PURE_VIRTUAL(UExperienceGameFeatureAction_WorldActionBase::AddToWorld, );

private:
	TMap<FGameFeatureStateChangeContext, FDelegateHandle> GameInstanceStartHandles;
};
