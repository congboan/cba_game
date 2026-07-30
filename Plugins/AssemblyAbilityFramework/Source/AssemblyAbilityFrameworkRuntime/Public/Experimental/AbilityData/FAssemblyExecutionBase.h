#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "FAssemblyExecutionBase.generated.h"

class UGameplayEffect;
class UGA_AssemblyDataDriven;
struct FGameplayAbilityActorInfo;
struct FGameplayAbilityActivationInfo;
struct FGameplayEventData;

/**
 * 执行策略基类 — 既是编辑器配置对象，又是运行时执行器（二合一）。
 *
 * 直接放在 FAssemblyAbilityRow::ExecutionStrategy 中，编辑器可选子类。
 * UGA_AssemblyDataDriven::ActivateAbility 通过一次虚调用 Execute() 完成执行。
 *
 * 新增执行模式 = 加一个 FAssemblyExecution_Xxx 子类，无需改基类或 GA。
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyExecutionBase
{
	GENERATED_BODY()

public:
	virtual ~FAssemblyExecutionBase() = default;

	/**
	 * 执行此策略的完整行为。
	 * @param GA         持有此执行策略的 GA 实例
	 * @param ActorInfo  当前 Actor 信息
	 * @param ActivationInfo 激活信息
	 * @param TriggerEventData 触发事件数据（可能为空）
	 */
	virtual void Execute(
		UGA_AssemblyDataDriven& GA,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo& ActivationInfo,
		const FGameplayEventData* TriggerEventData)
	{
		checkNoEntry(); // 必须由子类覆写
	};
};
