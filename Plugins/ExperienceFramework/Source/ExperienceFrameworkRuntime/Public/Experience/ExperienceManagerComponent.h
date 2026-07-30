#pragma once

#include "Components/ActorComponent.h"
#include "ExperienceManagerComponent.generated.h"

namespace UE::GameFeatures
{
	struct FResult;
}

class UExperienceDefinition;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnExperienceLoaded, const UExperienceDefinition* /*Experience*/);

UENUM(BlueprintType)
enum class EExperienceLoadState : uint8
{
	Unloaded,
	Loading,
	LoadingGameFeatures,
	ExecutingActions,
	Loaded,
	Deactivating
};

UCLASS(BlueprintType, ClassGroup = "Experience", meta = (BlueprintSpawnableComponent))
class EXPERIENCEFRAMEWORKRUNTIME_API UExperienceManagerComponent final : public UActorComponent
{
	GENERATED_BODY()

public:
	UExperienceManagerComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void SetCurrentExperience(FPrimaryAssetId ExperienceId);

	void CallOrRegister_OnExperienceLoaded_HighPriority(FOnExperienceLoaded::FDelegate&& Delegate);
	void CallOrRegister_OnExperienceLoaded(FOnExperienceLoaded::FDelegate&& Delegate);
	void CallOrRegister_OnExperienceLoaded_LowPriority(FOnExperienceLoaded::FDelegate&& Delegate);

	UFUNCTION(BlueprintCallable, Category = "Experience")
	bool IsExperienceLoaded() const;

	UFUNCTION(BlueprintCallable, Category = "Experience")
	bool ShouldShowLoadingScreen(FString& OutReason) const;

	const UExperienceDefinition* GetCurrentExperienceChecked() const;
	const UExperienceDefinition* GetCurrentExperience() const { return CurrentExperience; }

private:
	UFUNCTION()
	void OnRep_CurrentExperience();

	void StartExperienceLoad();
	void OnExperienceLoadComplete();
	void OnGameFeaturePluginLoadComplete(const UE::GameFeatures::FResult& Result);
	void OnExperienceFullLoadCompleted();
	void OnActionDeactivationCompleted();
	void OnAllActionsDeactivated();

	static const UExperienceDefinition* ResolveExperienceFromPrimaryAssetId(FPrimaryAssetId ExperienceId);

private:
	UPROPERTY(ReplicatedUsing = OnRep_CurrentExperience)
	TObjectPtr<const UExperienceDefinition> CurrentExperience;

	EExperienceLoadState LoadState = EExperienceLoadState::Unloaded;

	int32 NumGameFeaturePluginsLoading = 0;
	TArray<FString> GameFeaturePluginURLs;

	int32 NumObservedPausers = 0;
	int32 NumExpectedPausers = 0;

	FOnExperienceLoaded OnExperienceLoaded_HighPriority;
	FOnExperienceLoaded OnExperienceLoaded;
	FOnExperienceLoaded OnExperienceLoaded_LowPriority;
};
