#include "Experience/ExperienceManager.h"

TMap<FString, int32> UExperienceManager::GameFeaturePluginRequestCountMap;

void UExperienceManager::NotifyOfPluginActivation(const FString& PluginURL)
{
	int32& RequestCount = GameFeaturePluginRequestCountMap.FindOrAdd(PluginURL);
	++RequestCount;
}

bool UExperienceManager::RequestToDeactivatePlugin(const FString& PluginURL)
{
	int32* RequestCount = GameFeaturePluginRequestCountMap.Find(PluginURL);
	if (RequestCount == nullptr)
	{
		return true;
	}

	--(*RequestCount);
	if (*RequestCount <= 0)
	{
		GameFeaturePluginRequestCountMap.Remove(PluginURL);
		return true;
	}

	return false;
}
