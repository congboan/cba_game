#pragma once

#include "CoreMinimal.h"
#include "Experimental/AbilityData/FAssemblyExecutionBase.h"
#include "FAssemblyExecution_Projectile.generated.h"

/**
 * 投射物执行策略 — 请求生成投射物，由 BallisticsFramework 或外部系统处理。
 *
 * 核心运行时不负责投射物的实际生成和碰撞检测，
 * 只发出请求（通过事件/委托），具体的投射物系统监听并执行。
 * ProjectileDefId 是投射物定义的索引键。
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyExecution_Projectile : public FAssemblyExecutionBase
{
	GENERATED_BODY()

public:
	/** 投射物定义 ID（0 = 无效）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute|Projectile")
	int32 ProjectileDefId = 0;

	/** 生成的投射物数量。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute|Projectile", meta = (ClampMin = "1"))
	int32 ProjectileCount = 1;

	/** 投射物之间的散布角度（度）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Execute|Projectile")
	float SpreadAngle = 0.0f;

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
