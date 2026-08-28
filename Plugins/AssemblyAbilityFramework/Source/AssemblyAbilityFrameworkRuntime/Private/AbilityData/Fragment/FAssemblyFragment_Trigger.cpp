#include "AbilityData/Fragment/FAssemblyFragment_Trigger.h"
#include "Abilities/GameplayAbilityTypes.h"

// 行级触发声明的职责 = 授予期贡献：把本片字段写入 Spec.DynamicAbilityTriggers。
// 注册/注销/payload/联动全部由引擎原生生命周期托管
// （GiveAbility 内部 :578 Register / :637 Unregister，AbilitySystemComponent_Abilities.cpp 实证）。

void FAssemblyFragment_Trigger::ContributeToSpec(FGameplayAbilitySpec& Spec)
{
	if (!TriggerTag.IsValid())
	{
		return;
	}

	FAbilityTriggerData Data;
	Data.TriggerTag = TriggerTag;
	Data.TriggerSource = TriggerSource.GetValue();
	Spec.DynamicAbilityTriggers.Add(Data);
}
