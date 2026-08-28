#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTriggerType.h"
#include "AbilityData/Fragment/FAssemblyFragmentBase.h"
#include "FAssemblyFragment_Trigger.generated.h"

class UAbilitySystemComponent;
class UAssemblyAbilitySource;
struct FAssemblyAbilityRow;

/**
 * 触发声明片 — 引擎 FAbilityTriggerData 的行级数据化（与引擎字段一一对应）。
 *
 * 引擎 FGameplayAbilitySpec 提供公开字段 DynamicAbilityTriggers
 * （GameplayAbilitySpec.h:254）：GiveAbility 时引擎原生注册（:578）、移除时原生注销
 * （:637，AbilitySystemComponent_Abilities.cpp 实证），事件型由 TriggerAbilityFromGameplayEvent
 * 原生投递 payload，OwnedTag 型自动联动取消。
 *
 * 本片职责 = 授予管线前的纯数据汇总（CollectTriggerData → Spec.DynamicAbilityTriggers），
 * 无任何自建注册/订阅机制。
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyFragment_Trigger : public FAssemblyFragmentBase
{
	GENERATED_BODY()

public:
	/** 触发身份标签（引擎 FAbilityTriggerData::TriggerTag 同义）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	FGameplayTag TriggerTag;

	/** 触发来源（引擎 FAbilityTriggerData::TriggerSource 同义）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trigger")
	TEnumAsByte<EGameplayAbilityTriggerSource::Type> TriggerSource = EGameplayAbilityTriggerSource::GameplayEvent;

	/**
	 * 授予期贡献：把本片声明写入 Spec.DynamicAbilityTriggers（GameplayAbilitySpec.h:254）。
	 * 引擎在 GiveAbility 时原生注册（:578）、移除时原生注销（:637），事件型原生投递 payload，
	 * OwnedTag 型自动联动取消 —— 本片不做任何自建注册。
	 */
	virtual void ContributeToSpec(FGameplayAbilitySpec& Spec) override;
};
