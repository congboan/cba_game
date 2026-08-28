#include "AbilityData/Execution/FAssemblyExecution_InstantGE.h"
#include "AbilitySystem/UGA_AssemblyDataDriven.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayCueManager.h"
#include "AssemblyAbilityFrameworkLog.h"
#include "Assembly/UAssemblyAbilitySource.h"
#include "Assembly/AssemblyAbilityStatics.h"
#include "AbilityData/FAssemblyAbilityRow.h"
#include "AbilityData/FAssemblyChainContext.h"
#include "StructUtils/InstancedStruct.h"

void FAssemblyExecution_InstantGE::Execute(
	UGA_AssemblyDataDriven& GA,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo& ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Step 1: apply OnCastGE
	if (OnCastGE.IsValid() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

		// Grant-time precache: hot path never loads soft refs.
		UGameplayEffect* LoadedGE = CachedOnCastGE.Get();
		if (!LoadedGE)
		{
			UE_LOG(LogAssemblyAbility, Warning,
				TEXT("[InstantGE] OnCastGE not precached, skip GE apply."));
		}
		else
		{
			// Resolve target (carrier-adapted: skips chain-context entries in <5.9)
			AActor* TargetActor = nullptr;
			if (TriggerEventData)
			{
				TArray<AActor*> Targets;
				UAssemblyAbilityStatics::GetTargetActors(*TriggerEventData, Targets);
				if (Targets.Num() > 0)
				{
					TargetActor = Targets[0];
				}
			}

			FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
			FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(LoadedGE->GetClass(), GA.GetAbilityLevel(), EffectContext);

			if (SpecHandle.IsValid())
			{
			// Apply SetByCaller values from Row
			if (const FAssemblyAbilityRow* Row = GA.GetRow())
				{
					for (const auto& [Tag, Value] : Row->Values)
					{
						if (Tag.IsValid())
						{
							SpecHandle.Data->SetSetByCallerMagnitude(Tag, Value);
						}
					}
				}

				// Apply DamageModifiers from chain context (trigger-injected corrections)
				if (TriggerEventData && TriggerEventData->InstancedEventData.IsValid())
				{
					if (const FAssemblyChainContext* Ctx = TriggerEventData->InstancedEventData.GetPtr<FAssemblyChainContext>())
					{
						for (const auto& [Tag, Value] : Ctx->DamageModifiers)
						{
							if (Tag.IsValid())
							{
								SpecHandle.Data->SetSetByCallerMagnitude(Tag, Value);
							}
						}
					}
				}

				// Apply to target or self
				if (TargetActor)
				{
					if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor, true))
					{
						ASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);
					}
				}
				else
				{
					ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
				}
			}
		}
	}

	// Step 2: execute GameplayCue
	if (CueTag.IsValid() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		FGameplayCueParameters CueParams(EffectContext);
		CueParams.Instigator = ActorInfo->AvatarActor.Get();

		if (TriggerEventData)
		{
			CueParams.AggregatedSourceTags.AppendTags(TriggerEventData->InstigatorTags);
			CueParams.AggregatedTargetTags.AppendTags(TriggerEventData->TargetTags);
		}

		ASC->ExecuteGameplayCue(CueTag, CueParams);
	}

	// Step 3: send chain event (copy parent event to inherit chain-context carrier)
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

// ── Grant-time precache ─────────────────────────

void FAssemblyExecution_InstantGE::Precache(UAssemblyAbilitySource* Owner)
{
	if (!OnCastGE.IsNull())
	{
		CachedOnCastGE = OnCastGE.LoadSynchronous();
	}
}
