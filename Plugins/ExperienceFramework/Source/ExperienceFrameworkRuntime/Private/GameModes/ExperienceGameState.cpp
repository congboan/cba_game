#include "GameModes/ExperienceGameState.h"

#include "Experience/ExperienceManagerComponent.h"

AExperienceGameState::AExperienceGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ExperienceManagerComponent = CreateDefaultSubobject<UExperienceManagerComponent>(TEXT("ExperienceManagerComponent"));
}
