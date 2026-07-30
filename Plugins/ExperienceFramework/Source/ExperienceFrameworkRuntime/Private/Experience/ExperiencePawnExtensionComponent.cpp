#include "Experience/ExperiencePawnExtensionComponent.h"

#include "AbilitySystemComponent.h"
#include "Components/GameFrameworkComponentDelegates.h"
#include "Components/GameFrameworkComponentManager.h"
#include "Experience/ExperiencePawnData.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "System/ExperienceFrameworkLog.h"
#include "System/ExperienceGameplayTags.h"

const FName UExperiencePawnExtensionComponent::NAME_ActorFeatureName("PawnExtension");

UExperiencePawnExtensionComponent::UExperiencePawnExtensionComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void UExperiencePawnExtensionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ThisClass, PawnData);
}

UExperiencePawnExtensionComponent* UExperiencePawnExtensionComponent::FindPawnExtensionComponent(const AActor* Actor)
{
	return Actor ? Actor->FindComponentByClass<UExperiencePawnExtensionComponent>() : nullptr;
}

void UExperiencePawnExtensionComponent::OnRegister()
{
	Super::OnRegister();

	const APawn* Pawn = GetPawn<APawn>();
	ensureAlwaysMsgf(Pawn != nullptr, TEXT("ExperiencePawnExtensionComponent on [%s] can only be added to Pawn actors."), *GetNameSafe(GetOwner()));

	if (Pawn)
	{
		TArray<UActorComponent*> PawnExtensionComponents;
		Pawn->GetComponents(UExperiencePawnExtensionComponent::StaticClass(), PawnExtensionComponents);
		ensureAlwaysMsgf(PawnExtensionComponents.Num() == 1, TEXT("Only one ExperiencePawnExtensionComponent should exist on [%s]."), *GetNameSafe(GetOwner()));
	}

	RegisterInitStateFeature();
}

void UExperiencePawnExtensionComponent::BeginPlay()
{
	Super::BeginPlay();

	BindOnActorInitStateChanged(NAME_None, FGameplayTag(), false);

	ensure(TryToChangeInitState(ExperienceGameplayTags::InitState_Spawned));
	CheckDefaultInitialization();
}

void UExperiencePawnExtensionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeAbilitySystem();
	UnregisterInitStateFeature();

	Super::EndPlay(EndPlayReason);
}

void UExperiencePawnExtensionComponent::SetPawnData(const UExperiencePawnData* InPawnData)
{
	check(InPawnData);

	APawn* Pawn = GetPawnChecked<APawn>();
	if (Pawn->GetLocalRole() != ROLE_Authority)
	{
		return;
	}

	if (PawnData)
	{
		UE_LOG(LogExperienceFramework, Error, TEXT("Trying to set PawnData [%s] on pawn [%s] that already has PawnData [%s]."),
			*GetNameSafe(InPawnData),
			*GetNameSafe(Pawn),
			*GetNameSafe(PawnData));
		return;
	}

	PawnData = InPawnData;
	Pawn->ForceNetUpdate();

	CheckDefaultInitialization();
}

void UExperiencePawnExtensionComponent::OnRep_PawnData()
{
	CheckDefaultInitialization();
}

void UExperiencePawnExtensionComponent::InitializeAbilitySystem(UAbilitySystemComponent* InAbilitySystemComponent, AActor* InOwnerActor)
{
	check(InAbilitySystemComponent);
	check(InOwnerActor);

	if (AbilitySystemComponent == InAbilitySystemComponent)
	{
		return;
	}

	if (AbilitySystemComponent)
	{
		UninitializeAbilitySystem();
	}

	APawn* Pawn = GetPawnChecked<APawn>();
	AActor* ExistingAvatar = InAbilitySystemComponent->GetAvatarActor();

	if (ExistingAvatar && ExistingAvatar != Pawn)
	{
		ensure(!ExistingAvatar->HasAuthority());

		if (UExperiencePawnExtensionComponent* OtherExtensionComponent = FindPawnExtensionComponent(ExistingAvatar))
		{
			OtherExtensionComponent->UninitializeAbilitySystem();
		}
	}

	AbilitySystemComponent = InAbilitySystemComponent;
	AbilitySystemComponent->InitAbilityActorInfo(InOwnerActor, Pawn);

	OnAbilitySystemInitialized.Broadcast();
}

void UExperiencePawnExtensionComponent::UninitializeAbilitySystem()
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	if (AbilitySystemComponent->GetAvatarActor() == GetOwner())
	{
		AbilitySystemComponent->CancelAbilities();
		AbilitySystemComponent->RemoveAllGameplayCues();

		if (AbilitySystemComponent->GetOwnerActor())
		{
			AbilitySystemComponent->SetAvatarActor(nullptr);
		}
		else
		{
			AbilitySystemComponent->ClearActorInfo();
		}

		OnAbilitySystemUninitialized.Broadcast();
	}

	AbilitySystemComponent = nullptr;
}

void UExperiencePawnExtensionComponent::HandleControllerChanged()
{
	if (AbilitySystemComponent && AbilitySystemComponent->GetAvatarActor() == GetPawnChecked<APawn>())
	{
		ensure(AbilitySystemComponent->AbilityActorInfo->OwnerActor == AbilitySystemComponent->GetOwnerActor());
		if (!AbilitySystemComponent->GetOwnerActor())
		{
			UninitializeAbilitySystem();
		}
		else
		{
			AbilitySystemComponent->RefreshAbilityActorInfo();
		}
	}

	CheckDefaultInitialization();
}

void UExperiencePawnExtensionComponent::HandlePlayerStateReplicated()
{
	CheckDefaultInitialization();
}

void UExperiencePawnExtensionComponent::SetupPlayerInputComponent()
{
	CheckDefaultInitialization();
}

void UExperiencePawnExtensionComponent::CheckDefaultInitialization()
{
	CheckDefaultInitializationForImplementers();

	static const TArray<FGameplayTag> StateChain =
	{
		ExperienceGameplayTags::InitState_Spawned,
		ExperienceGameplayTags::InitState_DataAvailable,
		ExperienceGameplayTags::InitState_DataInitialized,
		ExperienceGameplayTags::InitState_GameplayReady
	};

	ContinueInitStateChain(StateChain);
}

bool UExperiencePawnExtensionComponent::CanChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState) const
{
	check(Manager);

	APawn* Pawn = GetPawn<APawn>();
	if (!CurrentState.IsValid() && DesiredState == ExperienceGameplayTags::InitState_Spawned)
	{
		return Pawn != nullptr;
	}

	if (CurrentState == ExperienceGameplayTags::InitState_Spawned && DesiredState == ExperienceGameplayTags::InitState_DataAvailable)
	{
		if (!PawnData || !Pawn)
		{
			return false;
		}

		if (Pawn->HasAuthority() || Pawn->IsLocallyControlled())
		{
			if (!GetController<AController>())
			{
				return false;
			}
		}

		return true;
	}

	if (CurrentState == ExperienceGameplayTags::InitState_DataAvailable && DesiredState == ExperienceGameplayTags::InitState_DataInitialized)
	{
		return Manager->HaveAllFeaturesReachedInitState(Pawn, ExperienceGameplayTags::InitState_DataAvailable);
	}

	if (CurrentState == ExperienceGameplayTags::InitState_DataInitialized && DesiredState == ExperienceGameplayTags::InitState_GameplayReady)
	{
		return true;
	}

	return false;
}

void UExperiencePawnExtensionComponent::HandleChangeInitState(UGameFrameworkComponentManager* Manager, FGameplayTag CurrentState, FGameplayTag DesiredState)
{
}

void UExperiencePawnExtensionComponent::OnActorInitStateChanged(const FActorInitStateChangedParams& Params)
{
	if (Params.FeatureName != NAME_ActorFeatureName && Params.FeatureState == ExperienceGameplayTags::InitState_DataAvailable)
	{
		CheckDefaultInitialization();
	}
}

void UExperiencePawnExtensionComponent::OnAbilitySystemInitialized_RegisterAndCall(FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (!OnAbilitySystemInitialized.IsBoundToObject(Delegate.GetUObject()))
	{
		OnAbilitySystemInitialized.Add(Delegate);
	}

	if (AbilitySystemComponent)
	{
		Delegate.Execute();
	}
}

void UExperiencePawnExtensionComponent::OnAbilitySystemUninitialized_Register(FSimpleMulticastDelegate::FDelegate Delegate)
{
	if (!OnAbilitySystemUninitialized.IsBoundToObject(Delegate.GetUObject()))
	{
		OnAbilitySystemUninitialized.Add(Delegate);
	}
}
