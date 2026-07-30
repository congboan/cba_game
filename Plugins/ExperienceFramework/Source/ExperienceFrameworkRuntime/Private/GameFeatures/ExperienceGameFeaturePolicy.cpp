#include "GameFeatures/ExperienceGameFeaturePolicy.h"

#include "AbilitySystemGlobals.h"
#include "GameFeatureData.h"
#include "GameFeatures/ExperienceGameFeatureAction_AddGameplayCuePath.h"
#include "GameFeaturesSubsystem.h"
#include "GameplayCueManager.h"

void UExperienceGameFeaturePolicy::InitGameFeatureManager()
{
	Observers.Add(NewObject<UExperienceGameFeature_AddGameplayCuePaths>());

	UGameFeaturesSubsystem& Subsystem = UGameFeaturesSubsystem::Get();
	for (UObject* Observer : Observers)
	{
		Subsystem.AddObserver(Observer, UGameFeaturesSubsystem::EObserverPluginStateUpdateMode::CurrentAndFuture);
	}

	Super::InitGameFeatureManager();
}

void UExperienceGameFeaturePolicy::ShutdownGameFeatureManager()
{
	Super::ShutdownGameFeatureManager();

	UGameFeaturesSubsystem& Subsystem = UGameFeaturesSubsystem::Get();
	for (UObject* Observer : Observers)
	{
		Subsystem.RemoveObserver(Observer);
	}
	Observers.Empty();
}

void UExperienceGameFeature_AddGameplayCuePaths::OnGameFeatureRegistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FString& PluginURL)
{
	const FString PluginRootPath = TEXT("/") + PluginName;
	for (const UGameFeatureAction* Action : GameFeatureData->GetActions())
	{
		if (const UExperienceGameFeatureAction_AddGameplayCuePath* AddGameplayCueAction = Cast<UExperienceGameFeatureAction_AddGameplayCuePath>(Action))
		{
			const TArray<FDirectoryPath>& PathsToAdd = AddGameplayCueAction->GetDirectoryPathsToAdd();
			if (UGameplayCueManager* GameplayCueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
			{
				for (const FDirectoryPath& Directory : PathsToAdd)
				{
					FString MutablePath = Directory.Path;
					UGameFeaturesSubsystem::FixPluginPackagePath(MutablePath, PluginRootPath, false);
					GameplayCueManager->AddGameplayCueNotifyPath(MutablePath, false);
				}

				if (!PathsToAdd.IsEmpty())
				{
					GameplayCueManager->InitializeRuntimeObjectLibrary();
				}
			}
		}
	}
}

void UExperienceGameFeature_AddGameplayCuePaths::OnGameFeatureUnregistering(const UGameFeatureData* GameFeatureData, const FString& PluginName, const FString& PluginURL)
{
	const FString PluginRootPath = TEXT("/") + PluginName;
	for (const UGameFeatureAction* Action : GameFeatureData->GetActions())
	{
		if (const UExperienceGameFeatureAction_AddGameplayCuePath* AddGameplayCueAction = Cast<UExperienceGameFeatureAction_AddGameplayCuePath>(Action))
		{
			const TArray<FDirectoryPath>& PathsToAdd = AddGameplayCueAction->GetDirectoryPathsToAdd();
			if (UGameplayCueManager* GameplayCueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
			{
				int32 NumRemoved = 0;
				for (const FDirectoryPath& Directory : PathsToAdd)
				{
					FString MutablePath = Directory.Path;
					UGameFeaturesSubsystem::FixPluginPackagePath(MutablePath, PluginRootPath, false);
					NumRemoved += GameplayCueManager->RemoveGameplayCueNotifyPath(MutablePath, false);
				}

				if (NumRemoved > 0)
				{
					GameplayCueManager->InitializeRuntimeObjectLibrary();
				}
			}
		}
	}
}
