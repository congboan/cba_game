#pragma once

#include "ModularGameMode.h"
#include "ExperienceGameMode.generated.h"

class AController;
class APawn;
class UExperienceDefinition;
class UExperiencePawnData;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnExperienceGameModePlayerInitialized, AGameModeBase* /*GameMode*/, AController* /*NewPlayer*/);

UCLASS(Config = Game, meta = (ShortTooltip = "Base game mode for Experience-driven projects."))
class EXPERIENCEFRAMEWORKRUNTIME_API AExperienceGameMode : public AModularGameMode
{
	GENERATED_BODY()

public:
	AExperienceGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UFUNCTION(BlueprintCallable, Category = "Experience|Pawn")
	virtual const UExperiencePawnData* GetPawnDataForController(const AController* InController) const;

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void InitGameState() override;
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
	virtual APawn* SpawnDefaultPawnAtTransform_Implementation(AController* NewPlayer, const FTransform& SpawnTransform) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void GenericPlayerInitialization(AController* NewPlayer) override;
	virtual void FailedToRestartPlayer(AController* NewPlayer) override;

	UFUNCTION(BlueprintCallable, Category = "Experience|Player")
	void RequestPlayerRestartNextFrame(AController* Controller, bool bForceReset = false);

	FOnExperienceGameModePlayerInitialized OnGameModePlayerInitialized;

protected:
	virtual void HandleMatchAssignmentIfNotExpectingOne();
	virtual void OnMatchAssignmentGiven(FPrimaryAssetId ExperienceId, const FString& ExperienceIdSource);
	virtual void OnExperienceLoaded(const UExperienceDefinition* CurrentExperience);
	virtual bool IsExperienceLoaded() const;
};
