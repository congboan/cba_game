#pragma once

#include "Components/GameFrameworkInitStateInterface.h"
#include "Components/PawnComponent.h"
#include "Delegates/Delegate.h"
#include "ExperiencePawnExtensionComponent.generated.h"

class UAbilitySystemComponent;
class UExperiencePawnData;
class UGameFrameworkComponentManager;
struct FActorInitStateChangedParams;
struct FGameplayTag;

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class EXPERIENCEFRAMEWORKRUNTIME_API UExperiencePawnExtensionComponent : public UPawnComponent, public IGameFrameworkInitStateInterface
{
	GENERATED_BODY()

public:
	UExperiencePawnExtensionComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static const FName NAME_ActorFeatureName;

	virtual FName GetFeatureName() const override { return NAME_ActorFeatureName; }
	virtual bool CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const override;
	virtual void HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) override;
	virtual void OnActorInitStateChanged(const FActorInitStateChangedParams& Params) override;
	virtual void CheckDefaultInitialization() override;

	UFUNCTION(BlueprintPure, Category = "Experience|Pawn")
	static UExperiencePawnExtensionComponent* FindPawnExtensionComponent(const AActor* Actor);

	template <class T>
	const T* GetPawnData() const { return Cast<T>(PawnData); }

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "Experience|Pawn")
	void SetPawnData(const UExperiencePawnData* InPawnData);

	UFUNCTION(BlueprintPure, Category = "Experience|Pawn")
	UAbilitySystemComponent* GetAbilitySystemComponent() const { return AbilitySystemComponent; }

	void InitializeAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent, AActor* InOwnerActor);
	void UninitializeAbilitySystem();
	void HandleControllerChanged();
	void HandlePlayerStateReplicated();
	void SetupPlayerInputComponent();

	void OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate Delegate);
	void OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate Delegate);

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION()
	void OnRep_PawnData();

	FSimpleMulticastDelegate OnAbilitySystemInitialized;
	FSimpleMulticastDelegate OnAbilitySystemUninitialized;

	UPROPERTY(EditInstanceOnly, ReplicatedUsing = OnRep_PawnData, Category = "Experience|Pawn")
	TObjectPtr<const UExperiencePawnData> PawnData;

	UPROPERTY(Transient)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
};
