#pragma once

#include "GameFramework/HUD.h"
#include "ExperienceHUD.generated.h"

UCLASS(Config = Game)
class EXPERIENCEFRAMEWORKRUNTIME_API AExperienceHUD : public AHUD
{
	GENERATED_BODY()

public:
	AExperienceHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void PreInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};
