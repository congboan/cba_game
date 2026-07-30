#pragma once

#include "CoreMinimal.h"
#include "Experimental/AbilityData/FAssemblyExecutionBase.h"
#include "FAssemblyExecution_InstantGE.generated.h"

class UGameplayEffect;

/**
 * 瞬发 GE 执行策略 — 激活时立即施加 GE，可选执行 Cue 和链事件。
 *
 * 这是最通用的执行模式：
 * - 对自身施加 Buff/Debuff
 * - 对目标施加伤害/治疗
 * - 触发 Cue 和后续链事件
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyExecution_InstantGE : public FAssemblyExecutionBase
{
	GENERATED_BODY()

public:
	/** 激活时施加的 GameplayEffect 资产。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute")
	TSoftObjectPtr<UGameplayEffect> OnCastGE;

	/** 激活时执行的 GameplayCue 标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute|Cue")
	FGameplayTag CueTag;

	/** 激活后发送的链事件标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute|Chain")
	FGameplayTag ChainEventTag;

	virtual void Execute(
		UGA_AssemblyDataDriven& GA,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo& ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
