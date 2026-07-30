#include "Experimental/AbilityData/FAssemblyExecution_InstantGE.h"
#include "Experimental/AbilitySystem/UGA_AssemblyDataDriven.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayCueManager.h"
#include "Experimental/UAssemblyAbilitySource.h"
#include "Experimental/AbilityData/FAssemblyAbilityRow.h"
#include "AbilityData/FAssemblyChainContext.h"
#include "StructUtils/InstancedStruct.h"

void FAssemblyExecution_InstantGE::Execute(
	UGA_AssemblyDataDriven& GA,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo& ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// 步骤 1：施加 OnCastGE
	if (OnCastGE.IsValid() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		UGameplayEffect* GECDO = OnCastGE.LoadSynchronous();
		if (GECDO)
		{
			// 确定目标
			AActor* TargetActor = nullptr;
			if (TriggerEventData && TriggerEventData->TargetData.Num() > 0)
			{
				const FGameplayAbilityTargetData* TargetData = TriggerEventData->TargetData.Get(0);
				if (TargetData)
				{
					for (TWeakObjectPtr<AActor> Actor : TargetData->GetActors())
					{
						if (Actor.IsValid())
						{
							TargetActor = Actor.Get();
							break;
						}
					}
				}
			}

			FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
			FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(GECDO->GetClass(), GA.GetAbilityLevel(), EffectContext);

			if (SpecHandle.IsValid())
			{
			// 从 Row 应用 SetByCaller 数值
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

				// 从链上下文覆盖 DamageModifiers（触发器注入的修正，如 30% 伤害）
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

				// 对目标或自身应用
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

	// 步骤 2：执行 GameplayCue
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

	// 步骤 3：发送链事件
	if (ChainEventTag.IsValid() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		GA.SendChainEvent(ChainEventTag, ActorInfo, TriggerEventData);
	}
}
