#include "AbilityData/Execution/FAssemblyExecution_AOE.h"
#include "AbilitySystem/UGA_AssemblyDataDriven.h"
#include "AbilityData/FAssemblyAbilityRow.h"
#include "AbilityData/FAssemblyChainContext.h"
#include "Assembly/AssemblyAbilityStatics.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AssemblyAbilityFrameworkLog.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayCueManager.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"

void FAssemblyExecution_AOE::Execute(
	UGA_AssemblyDataDriven& GA,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo& ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// AOE execution: 1) collect targets (TargetData preferred, else SphereTrace)
	// 2) apply OnCastGE per ASC-bearing target (SetByCaller + ChainContext modifiers)
	// 3) execute Cue (RawMagnitude = Radius) 4) send chain event

	UAbilitySystemComponent* ASC = (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC) return;

	// ── Collect targets ─────────────────────────────
	TArray<AActor*> Targets;

	// Prefer TargetData from TriggerEventData (carrier-adapted: skips chain-context entries in <5.9)
	if (TriggerEventData)
	{
		UAssemblyAbilityStatics::GetTargetActors(*TriggerEventData, Targets);
	}

	// No TargetData or supplement with SphereTrace
	AActor* Avatar = ActorInfo->AvatarActor.Get();
	if (Avatar && Radius > 0.0f)
	{
		const FVector Center = Avatar->GetActorLocation();

		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(Avatar);
		FCollisionShape SphereShape = FCollisionShape::MakeSphere(Radius);

		UWorld* World = Avatar->GetWorld();
		if (World)
		{
			World->OverlapMultiByChannel(
				Overlaps, Center, FQuat::Identity,
				CollisionChannel, SphereShape, QueryParams);

			for (const FOverlapResult& Result : Overlaps)
			{
				if (AActor* HitActor = Result.GetActor())
				{
					Targets.AddUnique(HitActor);
				}
			}

#if ENABLE_DRAW_DEBUG
			DrawDebugSphere(World, Center, Radius, 16, FColor::Red, false, 1.0f, 0, 1.0f);
#endif
		}
	}

	// Include caster self?
	if (bIncludeSelf && Avatar)
	{
		Targets.AddUnique(Avatar);
	}

	// ── Apply GE ────────────────────────────────────
	if (OnCastGE.IsValid())
	{
		UGameplayEffect* LoadedGE = CachedOnCastGE.Get();
		if (!LoadedGE)
		{
			UE_LOG(LogAssemblyAbility, Warning,
				TEXT("[AOE] OnCastGE not precached, skip GE apply."));
		}
		else
		{
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

				// Apply chain-context damage modifiers (trigger-injected)
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

				// Apply the same Spec to every valid target
				for (AActor* Target : Targets)
				{
					if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target, true))
					{
						ASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);
					}
				}
			}

			UE_LOG(LogAssemblyAbility, Log,
				TEXT("[AOE] Applied %s to %d targets (Radius=%.1f)"),
				*OnCastGE.GetAssetName(), Targets.Num(), Radius);
		}
	}

	// ── Execute Cue ─────────────────────────────────
	if (CueTag.IsValid())
	{
		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		FGameplayCueParameters CueParams(EffectContext);
		CueParams.Instigator = ActorInfo->AvatarActor.Get();
		CueParams.RawMagnitude = Radius; // AOE radius as cue parameter

		ASC->ExecuteGameplayCue(CueTag, CueParams);
	}

	// ── Send chain event (copy parent event to inherit chain-context carrier) ──
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

void FAssemblyExecution_AOE::Precache(UAssemblyAbilitySource* Owner)
{
	if (!OnCastGE.IsNull())
	{
		CachedOnCastGE = OnCastGE.LoadSynchronous();
	}
}
