#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "FAssemblyAbilityTrigger.generated.h"

/**
 * 触发器定义 — 将 ListenTag（要监听的事件）与 TriggerTag（要广播的触发身份）配对。
 *
 * 流程：事件携带 ListenTag 到达 → EventRouter 匹配 DispatchTable → 以 TriggerTag 广播
 * GameplayEvent 到目标 Actor（由监听该 TriggerTag 的 GA 响应）。
 *
 * 注册路径（主框架 editor-first）：在 GE 资产上挂 UGETriggerComponent_Assembly 并配置其
 * StaticTriggers。GE 应用到 ASC 时，EventRouter 从 Spec.Def 上 FindComponent 直读该组件的
 * StaticTriggers 注册到 DispatchTable；GE 移除时按 GEHandle 反查索引清理。
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyAbilityTrigger
{
	GENERATED_BODY()

	/** 此触发器监听的事件标签（例如 Event.Ability.Cast、Event.Attack.Hit）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	FGameplayTag ListenTag;

	/** 广播的触发身份标签 — 监听该 tag 的 GA 会被激活。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	FGameplayTag TriggerTag;

	/** 触发时附加的数值修正（写入 FAssemblyChainContext::DamageModifiers，例如 {Damage.Multiplier: 0.3}）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trigger")
	TMap<FGameplayTag, float> Modifiers;
};
