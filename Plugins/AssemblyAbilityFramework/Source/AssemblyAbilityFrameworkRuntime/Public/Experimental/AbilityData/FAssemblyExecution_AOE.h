#pragma once

#include "CoreMinimal.h"
#include "Experimental/AbilityData/FAssemblyExecutionBase.h"
#include "Engine/HitResult.h"
#include "FAssemblyExecution_AOE.generated.h"

class UGameplayEffect;

/**
 * AOE 区域伤害执行策略 — 以施法者为中心在球形区域内施加 GE。
 *
 * 执行流程：
 *   1. 如果 TriggerEventData 包含 TargetData，直接对其中目标施加 GE
 *   2. 否则以施法者为中心 SphereTrace，对命中 Actor 施加 GE
 *   3. 执行 Cue（RawMagnitude = Radius）
 *   4. 发送链事件
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyExecution_AOE : public FAssemblyExecutionBase
{
	GENERATED_BODY()

public:
	/** AOE 半径（厘米）。<= 0 时退化为仅对 TargetData 目标施加。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute|AOE", meta = (ClampMin = "0"))
	float Radius = 0.0f;

	/** 激活时施加的 GameplayEffect 资产。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute")
	TSoftObjectPtr<UGameplayEffect> OnCastGE;

	/** 激活时执行的 GameplayCue 标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute|Cue")
	FGameplayTag CueTag;

	/** 激活后发送的链事件标签。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute|Chain")
	FGameplayTag ChainEventTag;

	/** 碰撞检测通道，默认 WorldStatic。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute|AOE")
	TEnumAsByte<ECollisionChannel> CollisionChannel = ECC_WorldDynamic;

	/** 是否将施法者自身也纳入 AOE 目标。默认 false。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute|AOE")
	bool bIncludeSelf = false;

	virtual void Execute(
		UGA_AssemblyDataDriven& GA,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo& ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
