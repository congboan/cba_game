#include "GameModes/AsyncAction_ExperienceReady.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Experience/ExperienceManagerComponent.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

UAsyncAction_ExperienceReady* UAsyncAction_ExperienceReady::WaitForExperienceReady(UObject* WorldContextObject)
{
	UAsyncAction_ExperienceReady* Action = NewObject<UAsyncAction_ExperienceReady>();
	Action->WorldContextObject = WorldContextObject;
	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UAsyncAction_ExperienceReady::Activate()
{
	Step();
}

void UAsyncAction_ExperienceReady::Step()
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
