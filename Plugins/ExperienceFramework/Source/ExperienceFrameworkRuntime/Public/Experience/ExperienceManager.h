#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "ExperienceManager.generated.h"

UCLASS()
class EXPERIENCEFRAMEWORKRUNTIME_API UExperienceManager : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	static void NotifyOfPluginActivation(const FString& PluginURL);
	static bool RequestToDeactivatePlugin(const FString& PluginURL);

private:
	static TMap<FString, int32> GameFeaturePluginRequestCountMap;
};
