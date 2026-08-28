#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilityData/Fragment/FAssemblyFragmentBase.h"
#include "FAssemblyFragment_Tags.generated.h"

/**
 * Tag 配置片 — 引擎 UGameplayAbility Tags 组的 9 字段行级承载（唯一 Tag 片）。
 *
 * 字段命名与引擎一致（GameplayAbility.h:738-771 实证）；片类型即语义族（无需基类区分）：
 * GA 判定/编排统一在 UGA_AssemblyDataDriven 内部处理（引擎 DoesAbilitySatisfyTagRequirements
 * 同构：判定在 GA、数据在片）。每字段一个 Collect 函数（片自描述如何贡献，调用方不挖字段，
 * AppendTags 到 Out）；CollectTags 聚合全集（展示/审计/调试用）。
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyFragment_Tags : public FAssemblyFragmentBase
{
	GENERATED_BODY()

	// ── 引擎字段（GameplayAbility.h Tags 组，9 字段）──

	/** 激活时取消拥有这些标签的技能（引擎 CancelAbilitiesWithTag :738）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer CancelAbilitiesWithTag;

	/** 激活期间屏蔽拥有这些标签的技能（引擎 BlockAbilitiesWithTag :742）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer BlockAbilitiesWithTag;

	/** 激活期间给拥有者挂上的标签（引擎 ActivationOwnedTags :746，复制走 ReplicateActivationOwnedTags）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer ActivationOwnedTags;

	/** 拥有者缺失任一标签时拒绝激活（引擎 ActivationRequiredTags :750）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer ActivationRequiredTags;

	/** 拥有者持有任一标签时拒绝激活（引擎 ActivationBlockedTags :754）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer ActivationBlockedTags;

	/** 来源方缺失任一标签时拒绝激活（引擎 SourceRequiredTags :758）。SourceTags 为空时不判定。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer SourceRequiredTags;

	/** 来源方持有任一标签时拒绝激活（引擎 SourceBlockedTags :762）。SourceTags 为空时不判定。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer SourceBlockedTags;

	/** 目标方缺失任一标签时拒绝激活（引擎 TargetRequiredTags :766）。TargetTags 为空时不判定。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer TargetRequiredTags;

	/** 目标方持有任一标签时拒绝激活（引擎 TargetBlockedTags :770）。TargetTags 为空时不判定。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer TargetBlockedTags;

	/** 行身份 AssetTags 参与 ASC 全局屏蔽判定（原 B1 开关，:432 语义）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	bool bCheckAssetBlock = true;

	/** 行级身份标签（引擎 AbilityTags/AssetTags :475 语义；实例激活时直写 AbilityTags，block/cancel 副作用可读）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tags")
	FGameplayTagContainer AssetTags;

	// ── 聚合入口（每字段一个 Collect，GA 判定/编排按需取用）──

	void CollectCancelAbilitiesWithTag(FGameplayTagContainer& Out) const { Out.AppendTags(CancelAbilitiesWithTag); }
	void CollectBlockAbilitiesWithTag(FGameplayTagContainer& Out) const { Out.AppendTags(BlockAbilitiesWithTag); }
	void CollectActivationOwnedTags(FGameplayTagContainer& Out) const { Out.AppendTags(ActivationOwnedTags); }
	void CollectActivationRequiredTags(FGameplayTagContainer& Out) const { Out.AppendTags(ActivationRequiredTags); }
	void CollectActivationBlockedTags(FGameplayTagContainer& Out) const { Out.AppendTags(ActivationBlockedTags); }
	void CollectSourceRequiredTags(FGameplayTagContainer& Out) const { Out.AppendTags(SourceRequiredTags); }
	void CollectSourceBlockedTags(FGameplayTagContainer& Out) const { Out.AppendTags(SourceBlockedTags); }
	void CollectTargetRequiredTags(FGameplayTagContainer& Out) const { Out.AppendTags(TargetRequiredTags); }
	void CollectTargetBlockedTags(FGameplayTagContainer& Out) const { Out.AppendTags(TargetBlockedTags); }
	void CollectAssetTags(FGameplayTagContainer& Out) const { Out.AppendTags(AssetTags); }

	/** 全集聚合（展示/审计/调试）。 */
	void CollectTags(FGameplayTagContainer& Out) const
	{
		CollectCancelAbilitiesWithTag(Out);
		CollectBlockAbilitiesWithTag(Out);
		CollectActivationOwnedTags(Out);
		CollectActivationRequiredTags(Out);
		CollectActivationBlockedTags(Out);
		CollectSourceRequiredTags(Out);
		CollectSourceBlockedTags(Out);
		CollectTargetRequiredTags(Out);
		CollectTargetBlockedTags(Out);
		CollectAssetTags(Out);
	}
};
