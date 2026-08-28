#include "AbilityData/Execution/FAssemblyExecution_Projectile.h"
#include "AbilitySystem/UGA_AssemblyDataDriven.h"
#include "AbilitySystemComponent.h"
#include "AssemblyAbilityFrameworkLog.h"
#include "GameplayCueManager.h"

void FAssemblyExecution_Projectile::Execute(
	UGA_AssemblyDataDriven& GA,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo& ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Projectile execution (placeholder): AAF stays decoupled from BallisticsFramework.
	// External systems listen ChainEventTag (or subclass this strategy) to spawn projectiles.
	// Current Execute only runs Cue + sends ChainEventTag.

	if (ProjectileDefId <= 0)
	{
		UE_LOG(LogAssemblyAbility, Warning,
			TEXT("[Projectile] ProjectileDefId=%d invalid, skip projectile request."),
			ProjectileDefId);
	}
	else
	{
		UE_LOG(LogAssemblyAbility, Log,
			TEXT("[Projectile] Request: DefId=%d, Count=%d, Spread=%.1f "
			     "(spawn handled by external system listening ChainEventTag)"),
			ProjectileDefId, ProjectileCount, SpreadAngle);
	}

	// Execute Cue (e.g. cast sound/VFX)
	if (CueTag.IsValid() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		FGameplayCueParameters CueParams(EffectContext);
		CueParams.Instigator = ActorInfo->AvatarActor.Get();

		ASC->ExecuteGameplayCue(CueTag, CueParams);
	}

	// Send chain event — external system listens to spawn projectiles
	// (copy parent event to inherit chain-context carrier)
	if (ChainEventTag.IsValid() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		FGameplayEventData ChainData;
		if (TriggerEventData)
		{
			ChainData = *TriggerEventData;
		}
		ChainData.EventTag   = ChainEventTag;
		ChainData.Instigator = ActorInfo->AvatarActor.Get();
		ChainData.Target     = ActorInfo->AvatarActor.Get();
		ActorInfo->AbilitySystemComponent->HandleGameplayEvent(ChainEventTag, &ChainData);
	}
}
