#pragma once

#include "ModularGameState.h"
#include "ExperienceGameState.generated.h"

class UExperienceManagerComponent;

UCLASS(Config = Game)
class EXPERIENCEFRAMEWORKRUNTIME_API AExperienceGameState : public AModularGameState
{
	GENERATED_BODY()

public:
	AExperienceGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "Experience")
	UExperienceManagerComponent* GetExperienceManagerComponent() const { return ExperienceManagerComponent; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Experience")
	TObjectPtr<UExperienceManagerComponent> ExperienceManagerComponent;
};
