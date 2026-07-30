#include "GameModes/ExperienceGameMode.h"

#include "AssetRegistry/AssetData.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "Experience/ExperienceDefinition.h"
#include "Experience/ExperienceManagerComponent.h"
#include "Experience/ExperiencePawnData.h"
#include "Experience/ExperiencePawnExtensionComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameModes/ExperienceGameState.h"
#include "GameModes/ExperienceWorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "System/ExperienceDeveloperSettings.h"
#include "System/ExperienceFrameworkLog.h"
#include "TimerManager.h"

AExperienceGameMode::AExperienceGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AExperienceGameState::StaticClass();
}

const UExperiencePawnData* AExperienceGameMode::GetPawnDataForController(const AController* InController) const
{
	if (const AGameStateBase* GameStateBase = GameState)
	{
		if (const UExperienceManagerComponent* ExperienceComponent = GameStateBase->FindComponentByClass<UExperienceManagerComponent>())
		{
			if (ExperienceComponent->IsExperienceLoaded())
			{
				const UExperienceDefinition* Experience = ExperienceComponent->GetCurrentExperienceChecked();
				return Experience->DefaultPawnData;
			}
		}
	}

	return nullptr;
}

void AExperienceGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	GetWorldTimerManager().SetTimerForNextTick(this, &ThisClass::HandleMatchAssignmentIfNotExpectingOne);
}

void AExperienceGameMode::InitGameState()
{
	Super::InitGameState();

	UExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<UExperienceManagerComponent>();
	check(ExperienceComponent);

	ExperienceComponent->CallOrRegister_OnExperienceLoaded(
		FOnExperienceLoaded::FDelegate::CreateUObject(this, &ThisClass::OnExperienceLoaded));
}

void AExperienceGameMode::HandleMatchAssignmentIfNotExpectingOne()
{
	FPrimaryAssetId ExperienceId;
	FString ExperienceIdSource;

	if (!ExperienceId.IsValid() && UGameplayStatics::HasOption(OptionsString, TEXT("Experience")))
	{
		const FString ExperienceFromOptions = UGameplayStatics::ParseOption(OptionsString, TEXT("Experience"));
		ExperienceId = FPrimaryAssetId(FPrimaryAssetType(UExperienceDefinition::StaticClass()->GetFName()), FName(*ExperienceFromOptions));
		ExperienceIdSource = TEXT("OptionsString");
	}

	if (!ExperienceId.IsValid() && GetWorld()->IsPlayInEditor())
	{
		ExperienceId = GetDefault<UExperienceDeveloperSettings>()->ExperienceOverride;
		ExperienceIdSource = TEXT("DeveloperSettings");
	}

	if (!ExperienceId.IsValid())
	{
		FString ExperienceFromCommandLine;
		if (FParse::Value(FCommandLine::Get(), TEXT("Experience="), ExperienceFromCommandLine))
		{
			ExperienceId = FPrimaryAssetId::ParseTypeAndName(ExperienceFromCommandLine);
			if (!ExperienceId.PrimaryAssetType.IsValid())
			{
				ExperienceId = FPrimaryAssetId(FPrimaryAssetType(UExperienceDefinition::StaticClass()->GetFName()), FName(*ExperienceFromCommandLine));
			}
			ExperienceIdSource = TEXT("CommandLine");
		}
	}

	if (!ExperienceId.IsValid())
	{
		if (const AExperienceWorldSettings* TypedWorldSettings = Cast<AExperienceWorldSettings>(GetWorldSettings()))
		{
			ExperienceId = TypedWorldSettings->GetDefaultGameplayExperience();
			ExperienceIdSource = TEXT("WorldSettings");
		}
	}

	if (!ExperienceId.IsValid())
	{
		ExperienceId = GetDefault<UExperienceDeveloperSettings>()->DefaultExperience;
		ExperienceIdSource = TEXT("DefaultSettings");
	}

	FAssetData AssetData;
	if (ExperienceId.IsValid() && !UAssetManager::Get().GetPrimaryAssetData(ExperienceId, AssetData))
	{
		UE_LOG(LogExperienceFramework, Error, TEXT("Experience %s was selected from %s but was not found by AssetManager."),
			*ExperienceId.ToString(),
			*ExperienceIdSource);
		ExperienceId = FPrimaryAssetId();
	}

	OnMatchAssignmentGiven(ExperienceId, ExperienceIdSource);
}

void AExperienceGameMode::OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId, const FString& ExperienceIdSource)
{
	if (!ExperienceId.IsValid())
	{
		UE_LOG(LogExperienceFramework, Error, TEXT("No valid Experience could be selected. Players will not spawn."));
		return;
	}

	UE_LOG(LogExperienceFramework, Log, TEXT("Selected Experience %s from %s."), *ExperienceId.ToString(), *ExperienceIdSource);

	UExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<UExperienceManagerComponent>();
	check(ExperienceComponent);
	ExperienceComponent->SetCurrentExperience(ExperienceId);
}

void AExperienceGameMode::OnExperienceLoaded(const UExperienceDefinition* CurrentExperience)
{
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Cast<APlayerController>(*Iterator);
		if (PlayerController != nullptr && PlayerController->GetPawn() == nullptr && PlayerCanRestart(PlayerController))
		{
			RestartPlayer(PlayerController);
		}
	}
}

bool AExperienceGameMode::IsExperienceLoaded() const
{
	check(GameState);
	const UExperienceManagerComponent* ExperienceComponent = GameState->FindComponentByClass<UExperienceManagerComponent>();
	check(ExperienceComponent);

	return ExperienceComponent->IsExperienceLoaded();
}

UClass* AExperienceGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (const UExperiencePawnData* PawnData = GetPawnDataForController(InController))
	{
		if (PawnData->PawnClass != nullptr)
		{
			return PawnData->PawnClass;
		}
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

APawn* AExperienceGameMode::SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform)
{
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.Instigator = GetInstigator();
	SpawnInfo.ObjectFlags |= RF_Transient;
	SpawnInfo.bDeferConstruction = true;

	UClass* PawnClass = GetDefaultPawnClassForController(NewPlayer);
	if (PawnClass == nullptr)
	{
		UE_LOG(LogExperienceFramework, Error, TEXT("Unable to spawn pawn because no pawn class was resolved."));
		return nullptr;
	}

	APawn* SpawnedPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnInfo);
	if (SpawnedPawn == nullptr)
	{
		UE_LOG(LogExperienceFramework, Error, TEXT("Unable to spawn pawn class %s at %s."),
			*GetNameSafe(PawnClass),
			*SpawnTransform.ToHumanReadableString());
		return nullptr;
	}

	if (const UExperiencePawnData* PawnData = GetPawnDataForController(NewPlayer))
	{
		if (UExperiencePawnExtensionComponent* PawnExtComponent = UExperiencePawnExtensionComponent::FindPawnExtensionComponent(SpawnedPawn))
		{
			PawnExtComponent->SetPawnData(PawnData);
		}
		else
		{
			UE_LOG(LogExperienceFramework, Error, TEXT("Game mode was unable to set PawnData on spawned pawn [%s]. Add UExperiencePawnExtensionComponent to the pawn class."), *GetNameSafe(SpawnedPawn));
		}
	}
	else
	{
		UE_LOG(LogExperienceFramework, Error, TEXT("Game mode was unable to resolve PawnData for controller [%s]."), *GetNameSafe(NewPlayer));
	}

	SpawnedPawn->FinishSpawning(SpawnTransform);
	return SpawnedPawn;
}

void AExperienceGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (IsExperienceLoaded())
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	}
}

void AExperienceGameMode::GenericPlayerInitialization(AController* NewPlayer)
{
	Super::GenericPlayerInitialization(NewPlayer);
	OnGameModePlayerInitialized.Broadcast(this, NewPlayer);
}

void AExperienceGameMode::RequestPlayerRestartNextFrame(AController* Controller, bool bForceReset)
{
	if (Controller == nullptr)
	{
		return;
	}

	if (bForceReset)
	{
		Controller->Reset();
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		GetWorldTimerManager().SetTimerForNextTick(PlayerController, &APlayerController::ServerRestartPlayer_Implementation);
	}
}

void AExperienceGameMode::FailedToRestartPlayer(AController* NewPlayer)
{
	Super::FailedToRestartPlayer(NewPlayer);

	if (GetDefaultPawnClassForController(NewPlayer) != nullptr)
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(NewPlayer))
		{
			if (PlayerCanRestart(PlayerController))
			{
				RequestPlayerRestartNextFrame(NewPlayer);
			}
		}
	}
}
