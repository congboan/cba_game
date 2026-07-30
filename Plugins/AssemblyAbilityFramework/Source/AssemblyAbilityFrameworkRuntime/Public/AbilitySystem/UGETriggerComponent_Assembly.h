#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectComponent.h"
#include "AbilityData/FAssemblyAbilityTrigger.h"
#include "UGETriggerComponent_Assembly.generated.h"

struct FActiveGameplayEffect;
struct FActiveGameplayEffectsContainer;

/**
 * GE Component — 让 GameplayEffect 资产侧静态配置一批 Triggers。
 *
 * 单一职责：
 *   在 GE 资产上配置 StaticTriggers（编辑器）。EventRouter 在 GE 添加时
 *   从 Spec.Def 上 FindComponent 直读本组件的 StaticTriggers 注册到 DispatchTable。
 *
 * Component 不知道 EventRouter / DispatchTable 的存在；
 * EventRouter 自驱动监听 ASC 上的 ActiveGE 添加事件，直读本组件。
 *
 * 数据流：
 *   1. GE 资产配置 StaticTriggers
 *   2. GE 应用 → ASC OnActiveGameplayEffectAddedDelegateToSelf →
 *      EventRouter.OnAnyGEAdded 从 Spec.Def 上 FindComponent 读本组件 StaticTriggers
 *   3. Router 注册 Triggers → GE 移除 → EventRouter 反查索引清理
 *
 * 注意：本组件是纯静态配置容器 — 不读写 EffectContext、不做任何运行时注入、
 * 不发 GameplayEvent。属于主框架（editor-first），不依赖 Experimental/ 下的任何类型。
 */
UCLASS(BlueprintType, EditInlineNew, DisplayName = "Assembly Trigger Component")
class ASSEMBLYABILITYFRAMEWORKRUNTIME_API UGETriggerComponent_Assembly : public UGameplayEffectComponent
{
	GENERATED_BODY()

public:
	UGETriggerComponent_Assembly();

	/** 静态触发器列表 — 编辑器在 GE 资产上配置。所有 Apply 实例都会注册。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "AssemblyTriggers")
	TArray<FAssemblyAbilityTrigger> StaticTriggers;
};
