#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "FAssemblyChainContext.generated.h"

/**
 * 链事件上下文 — 通过 FGameplayEventData::InstancedEventData 传递。
 *
 * 与 FGameplayEventData::OptionalObject 不同，InstancedEventData 是
 * 值语义的 FInstancedStruct，可在链中直接读写累积，无需 NewObject 或 CloneForChild。
 *
 * 典型流程：
 *   1. 链上第一个技能创建 FAssemblyChainContext{Depth=1, RootId=...}
 *   2. 通过 ChainData.InstancedEventData = FInstancedStruct::Make(Ctx) 写入
 *   3. HandleGameplayEvent → 下一跳技能从 TriggerEventData->InstancedEventData 读取
 *   4. 下一跳修改 ChainDepth/DamageModifiers → 继续传递
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyChainContext
{
	GENERATED_BODY()

	/** 当前链深度（根 = 0，每次跳转 +1）。 */
	UPROPERTY(BlueprintReadWrite, Category = "Chain")
	int32 ChainDepth = 0;

	/** 此链的根事件唯一 ID。 */
	UPROPERTY(BlueprintReadWrite, Category = "Chain")
	FGuid RootEventId;

	/** 发起此事件链的 Actor。 */
	UPROPERTY(BlueprintReadWrite, Category = "Chain")
	TWeakObjectPtr<AActor> InstigatorActor;

	/** 整条链中累积的伤害修正映射（Tag → 倍率）。 */
	UPROPERTY(BlueprintReadWrite, Category = "Chain")
	TMap<FGameplayTag, float> DamageModifiers;

	/** 此链中已应用的规则 ID 追踪。 */
	UPROPERTY(BlueprintReadWrite, Category = "Chain")
	FGameplayTagContainer AppliedRuleIds;

	/** 规则应用次数追踪。 */
	UPROPERTY(BlueprintReadWrite, Category = "Chain")
	TMap<FGameplayTag, int32> RuleApplyCounts;

	/** 当前被屏蔽的触发器标签。 */
	UPROPERTY(BlueprintReadWrite, Category = "Chain")
	FGameplayTagContainer TriggerMask;

	/** 整条链中被阻止的触发器组。 */
	UPROPERTY(BlueprintReadWrite, Category = "Chain")
	FGameplayTagContainer BlockedTriggerGroups;

	/** 检查给定标签是否被屏蔽。 */
	bool IsTriggerMasked(FGameplayTag TriggerTag) const
	{
		return TriggerMask.HasTag(TriggerTag) || BlockedTriggerGroups.HasTag(TriggerTag);
	}
};
