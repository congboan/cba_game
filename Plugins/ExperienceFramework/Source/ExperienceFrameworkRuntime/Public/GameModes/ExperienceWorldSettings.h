#pragma once

#include "GameFramework/WorldSettings.h"
#include "ExperienceWorldSettings.generated.h"

class UExperienceDefinition;

UCLASS()
class EXPERIENCEFRAMEWORKRUNTIME_API AExperienceWorldSettings : public AWorldSettings
{
	GENERATED_BODY()

public:
	AExperienceWorldSettings(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	FPrimaryAssetId GetDefaultGameplayExperience() const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "GameMode")
	TSoftClassPtr<UExperienceDefinition> DefaultGameplayExperience;

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditDefaultsOnly, Category = "PIE")
	bool bForceStandaloneNetMode = false;
#endif
};
