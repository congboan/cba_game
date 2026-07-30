#include "GameModes/ExperienceWorldSettings.h"

#include "Engine/AssetManager.h"
#include "Experience/ExperienceDefinition.h"
#include "System/ExperienceFrameworkLog.h"

AExperienceWorldSettings::AExperienceWorldSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FPrimaryAssetId AExperienceWorldSettings::GetDefaultGameplayExperience() const
{
	if (DefaultGameplayExperience.IsNull())
	{
		return FPrimaryAssetId();
	}

	const FPrimaryAssetId ExperienceId = UAssetManager::Get().GetPrimaryAssetIdForPath(DefaultGameplayExperience.ToSoftObjectPath());
	if (!ExperienceId.IsValid())
	{
		UE_LOG(LogExperienceFramework, Error, TEXT("%s.DefaultGameplayExperience failed to resolve to a primary asset id: %s"),
			*GetPathNameSafe(this),
			*DefaultGameplayExperience.ToString());
	}

	return ExperienceId;
}
