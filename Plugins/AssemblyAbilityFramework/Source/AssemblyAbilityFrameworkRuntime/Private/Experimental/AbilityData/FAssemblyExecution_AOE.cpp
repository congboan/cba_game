#include "Experimental/AbilityData/FAssemblyExecution_AOE.h"
#include "Experimental/AbilitySystem/UGA_AssemblyDataDriven.h"
#include "Experimental/AbilityData/FAssemblyAbilityRow.h"
#include "AbilityData/FAssemblyChainContext.h"
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
	// AOE 执行策略：
	// 1. 收集目标 Actor 集合（优先 TargetData，否则 SphereTrace）
	// 2. 对每个拥有 ASC 的目标施加 OnCastGE（含 SetByCaller + ChainContext 修正）
	// 3. 执行 Cue（RawMagnitude = Radius）
	// 4. 发送链事件

	UAbilitySystemComponent* ASC = (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!ASC) return;

	// ── 收集目标 ──────────────────────────────────
	TArray<AActor*> Targets;

	// 优先使用 TriggerEventData 中的 TargetData
	if (TriggerEventData && TriggerEventData->TargetData.Num() > 0)
	{
		for (int32 i = 0; i < TriggerEventData->TargetData.Num(); ++i)
		{
			if (const FGameplayAbilityTargetData* TargetData = TriggerEventData->TargetData.Get(i))
			{
				for (TWeakObjectPtr<AActor> Actor : TargetData->GetActors())
				{
					if (Actor.IsValid())
					{
						Targets.AddUnique(Actor.Get());
					}
				}
			}
		}
	}

	// 无 TargetData 或补充 SphereTrace
	AActor* Avatar = ActorInfo->AvatarActor.Get();
	if (Avatar && Radius > 0.0f)
	{
		const FVector Center = Avatar->GetActorLocation();

		// 球形重叠检测（Overlap，不需要命中点）
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
			// 可视化 AOE 范围（仅开发构建）
			DrawDebugSphere(World, Center, Radius, 16, FColor::Red, false, 1.0f, 0, 1.0f);
#endif
		}
	}

	// 是否包含施法者自身
	if (bIncludeSelf && Avatar)
	{
		Targets.AddUnique(Avatar);
	}

	// ── 施加 GE ──────────────────────────────────
	if (OnCastGE.IsValid())
	{
		UGameplayEffect* GECDO = OnCastGE.LoadSynchronous();
		if (GECDO)
		{
			// Spec 在循环外创建一次，SetByCaller 注入一次，对每个目标复用同一 Spec
			FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
			FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(GECDO->GetClass(), GA.GetAbilityLevel(), EffectContext);
			if (SpecHandle.IsValid())
			{
				// 应用 SetByCaller 数值（来自 Row）
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

				// 应用链上下文伤害修正（来自触发器 Modifiers）
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

				// 对每个有效目标施加同一 Spec
				for (AActor* Target : Targets)
				{
					if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Target, true))
					{
						ASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data, TargetASC);
					}
				}
			}

			UE_LOG(LogAssemblyAbility, Log,
				TEXT("[AOE] 施加 %s 到 %d 个目标 (Radius=%.1f)"),
				*OnCastGE.GetAssetName(), Targets.Num(), Radius);
		}
	}

	// ── 执行 Cue ─────────────────────────────────
	if (CueTag.IsValid())
	{
		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		FGameplayCueParameters CueParams(EffectContext);
		CueParams.Instigator = ActorInfo->AvatarActor.Get();
		CueParams.RawMagnitude = Radius; // AOE 半径作为 Cue 参数

		ASC->ExecuteGameplayCue(CueTag, CueParams);
	}

	// ── 发送链事件 ─────────────────────────────────
	if (ChainEventTag.IsValid())
	{
		GA.SendChainEvent(ChainEventTag, ActorInfo, TriggerEventData);
	}
}
