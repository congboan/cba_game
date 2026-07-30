#include "GameModes/ExperienceAsyncAction_WaitExperienceReady.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Experience/ExperienceManagerComponent.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

UExperienceAsyncAction_WaitExperienceReady* UExperienceAsyncAction_WaitExperienceReady::WaitForExperienceReady(UObject* WorldContextObject)
{
	UExperienceAsyncAction_WaitExperienceReady* Action = NewObject<UExperienceAsyncAction_WaitExperienceReady>();
	Action->WorldContextObject = WorldContextObject;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UExperienceAsyncAction_WaitExperienceReady::Activate()
{
	Step();
}

void UExperienceAsyncAction_WaitExperienceReady::Step()
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (World == nullptr)
	{
		SetReadyToDestroy();
		return;
	}

	AGameStateBase* GameState = World->GetGameState();
	UExperienceManagerComponent* ExperienceComponent = GameState ? GameState->FindComponentByClass<UExperienceManagerComponent>() : nullptr;
	if (ExperienceComponent == nullptr)
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::Step);
		return;
	}

	if (ExperienceComponent->IsExperienceLoaded())
	{
		OnReady.Broadcast();
		SetReadyToDestroy();
		return;
	}

	ExperienceComponent->CallOrRegister_OnExperienceLoaded(
		FOnExperienceLoaded::FDelegate::CreateWeakLambda(this, [this](const UExperienceDefinition*)
		{
			OnReady.Broadcast();
			SetReadyToDestroy();
		}));
}
