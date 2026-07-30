#pragma once

#include "Engine/AssetManagerTypes.h"
#include "Engine/DeveloperSettings.h"
#include "ExperienceDeveloperSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Experience Framework"))
class EXPERIENCEFRAMEWORKRUNTIME_API UExperienceDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Experience", meta = (AllowedTypes = "ExperienceDefinition"))
	FPrimaryAssetId DefaultExperience;

	UPROPERTY(Config, EditAnywhere, Category = "Experience|PIE", meta = (AllowedTypes = "ExperienceDefinition"))
	FPrimaryAssetId ExperienceOverride;
};
