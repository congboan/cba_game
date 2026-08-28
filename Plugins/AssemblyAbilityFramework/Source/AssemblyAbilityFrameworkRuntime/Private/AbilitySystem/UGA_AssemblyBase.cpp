#include "AbilitySystem/UGA_AssemblyBase.h"
#include "AbilitySystemComponent.h"
#include "AssemblyAbilityFrameworkLog.h"
#include "Assembly/AssemblyAbilityStatics.h"
#include "StructUtils/InstancedStruct.h"

UGA_AssemblyBase::UGA_AssemblyBase()
{
	// Default instancing: one instance per actor, supports LocalPredicted net model (Lyra default)
	InstancingPolicy    = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy  = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	NetSecurityPolicy   = EGameplayAbilityNetSecurityPolicy::ClientOrServer;
}

// ── OnAvatarSet ──────────────────────────────────

void UGA_AssemblyBase::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	// OnSpawn/OnAvatarSet policy: auto-activate after grant (Lyra TryActivateAbilityOnSpawn quadruple guard)
	if (ActivationPolicy != EAssemblyAbilityActivationPolicy::OnSpawn
	 && ActivationPolicy != EAssemblyAbilityActivationPolicy::OnAvatarSet)
	{
		return;
	}
	if (Spec.IsActive())  // anti-reentry
	{
		return;
	}
	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	const AActor* AvatarActor = ActorInfo->AvatarActor.Get();
	// Avatar torn off or lifespan set (dying): do not activate, wait for new Avatar
	if (!AvatarActor || AvatarActor->GetTearOff() || AvatarActor->GetLifeSpan() > 0.0f)
	{
		return;
	}

	// Split by net role to avoid double-sided activation
	const bool bIsLocalExecution  = NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalPredicted
	                             || NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::LocalOnly;
	const bool bIsServerExecution = NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerOnly
	                             || NetExecutionPolicy == EGameplayAbilityNetExecutionPolicy::ServerInitiated;
	const bool bClientShouldActivate = ActorInfo->IsLocallyControlled() && bIsLocalExecution;
	const bool bServerShouldActivate = ActorInfo->IsNetAuthority() && bIsServerExecution;

	if (bClientShouldActivate || bServerShouldActivate)
	{
		ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
	}
}

// ── NativeOnAbilityFailedToActivate ──────────────

void UGA_AssemblyBase::NativeOnAbilityFailedToActivate(const FGameplayTagContainer& FailedReason) const
{
	if (FailureTagToUserFacingMessages.IsEmpty())
	{
		return;
	}

	for (const FGameplayTag& Reason : FailedReason)
	{
		if (const FText* Message = FailureTagToUserFacingMessages.Find(Reason))
		{
			UE_LOG(LogAssemblyAbility, Log,
				TEXT("[AssemblyAbility] '%s' activation failed: %s -> \"%s\""),
				*GetName(), *Reason.ToString(), *Message->ToString());
		}
	}
}

// ── CancelAbility ────────────────────────────────

void UGA_AssemblyBase::CancelAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateCancelAbility)
{
	if (bLogCancelation)
	{
		UE_LOG(LogAssemblyAbility, Warning,
			TEXT("[AssemblyAbility] '%s' cancelled (CancelAbility)."),
			*GetName());
	}

	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}
