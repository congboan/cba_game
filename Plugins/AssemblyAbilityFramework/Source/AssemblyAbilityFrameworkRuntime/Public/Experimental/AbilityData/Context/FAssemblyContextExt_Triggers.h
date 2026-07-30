#pragma once

#include "CoreMinimal.h"
#include "Experimental/AbilityData/Context/FAssemblyContextExtension.h"
#include "AbilityData/FAssemblyAbilityTrigger.h"
#include "FAssemblyContextExt_Triggers.generated.h"

/**
 * Trigger 扩展 — 让 GE 携带要注册的触发器列表。
 *
 * 用法：
 *   FAssemblyEffectContext Ctx;
 *   FAssemblyContextExt_Triggers TriggersExt;
 *   TriggersExt.Triggers.Add({ListenTag, TriggerTag, {}});
 *   Ctx.Extension = TInstancedStruct<FAssemblyContextExtension>::Make(TriggersExt);
 *
 * GE 应用时由 UGETriggerComponent_Assembly 读取并注册，
 * GE 移除时由 ActiveGE.EventSet.OnEffectRemoved 自动解注册。
 */
USTRUCT(BlueprintType)
struct ASSEMBLYABILITYFRAMEWORKRUNTIME_API FAssemblyContextExt_Triggers : public FAssemblyContextExtension
{
	GENERATED_BODY()

	/** 要伴随 GE 一起注册的触发器列表。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AssemblyContext|Triggers")
	TArray<FAssemblyAbilityTrigger> Triggers;
};
