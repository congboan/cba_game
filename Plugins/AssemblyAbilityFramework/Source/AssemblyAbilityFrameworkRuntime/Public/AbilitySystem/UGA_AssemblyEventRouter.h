#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayTagContainer.h"
#include "AbilityData/FAssemblyAbilityTrigger.h"
#include "UGA_AssemblyEventRouter.generated.h"

class UAbilitySystemComponent;
struct FActiveGameplayEffect;
struct FGameplayEffectRemovalInfo;
struct FGameplayEffectSpec;

/**
 * 调度表条目 — 自包含全部触发信息。
 * Trigger 字段已包含 ListenTag / TriggerTag / Modifiers，可脱离 Map Key 独立使用。
 */
USTRUCT()
struct FAssemblyTriggerSpec
{
	GENERATED_BODY()

	/** 完整的触发器数据（ListenTag, TriggerTag, Modifiers）。 */
	UPROPERTY()
	FAssemblyAbilityTrigger Trigger;

	/** 关联的 GE 句柄（用于 GE 过期时精确清理）。 */
	UPROPERTY()
	FActiveGameplayEffectHandle GEHandle;
};

/**
 * AssemblyAbilityFramework 事件路由器 — 直读 GE 组件的单向流。
 *
 * 工作原理：
 *   - ActivateAbility 时挂 OnActiveGameplayEffectAddedDelegateToSelf，
 *     并补扫 ASC 上已存在的所有 ActiveGE（防漏注册）。
 *   - 每个新 ActiveGE 到达时：从 Spec.Def 上查找 UGETriggerComponent_Assembly，
 *     读取其 StaticTriggers，把每条 Trigger 注册到 DispatchTable，并通过
 *     ActiveGE.EventSet.OnEffectRemoved 绑定生命周期清理。
 *   - Instant GE 不能用于注册 Trigger（瞬时移除无监听窗口） — 直接拒绝并 Warning。
 *   - 监听到 ListenTag 触发的 GameplayEvent 时，按 DispatchTable 派发为 TriggerTag。
 *
 * 设计要点：
 *   - Trigger 在效果资产（GE）上静态配置，路由器不依赖运行时 EffectContext。
 *   - 反查索引 GEHandle → DispatchEntry 实现 O(1) 注销。
 *   - 历史 ActiveGE 补扫：EventRouter 后于 ActiveGE 激活时不会漏。
 *   - GE 应用即注册（读组件），GE 移除即注销。
 */
UCLASS()
class ASSEMBLYABILITYFRAMEWORKRUNTIME_API UGA_AssemblyEventRouter : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_AssemblyEventRouter();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	void DebugDumpTable() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AssemblyAbility|Safety")
	int32 MaxChainDepth = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AssemblyAbility|Safety")
	int32 MaxFanOutCount = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AssemblyAbility|Safety")
	int32 MaxEventsPerFrame = 256;

	bool CheckChainDepth(int32 CurrentDepth) const;
	bool TryConsumeEventSlot();
	void ResetFrameEventCounter();
	int32 GetFrameEventCount() const { return FrameEventCount; }

protected:
	/** 任何 ActiveGE 加入 ASC 时回调（含历史补扫）。 */
	void OnAnyGEAdded(UAbilitySystemComponent* OwningASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle GEHandle);

	/** 派发路径专用回调：监听 ListenTag 的 GameplayEvent 后路由到 TriggerTag。 */
	void OnDispatchEvent(const FGameplayEventData* Payload);

	/** 单条 GE 的移除回调（按 GEHandle 反查索引清理）。 */
	void OnGERemoved(const FGameplayEffectRemovalInfo& Info);

private:
	/** 把一个 ActiveGE 上的所有 Triggers 注册进表 + 绑生命周期。 */
	void RegisterTriggersFromActiveGE(UAbilitySystemComponent* OwningASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle GEHandle);

	/** 按 GEHandle 反查索引清理 DispatchTable。 */
	void UnregisterByGEHandle(FActiveGameplayEffectHandle GEHandle);

	void DispatchEvent(FGameplayTag EventTag, const FGameplayEventData& Payload);
	UAbilitySystemComponent* GetASC() const;

	/** 调度表：ListenTag → 该 Tag 下所有的 TriggerSpec。 */
	TMap<FGameplayTag, TArray<FAssemblyTriggerSpec>> DispatchTable;

	/** 反查索引：GEHandle → 它注册过的 (ListenTag, ArrayIndex) 列表，用于 O(1) 注销。 */
	struct FTriggerLocation
	{
		FGameplayTag ListenTag;
	};
	TMap<FActiveGameplayEffectHandle, TArray<FTriggerLocation>> GEHandleIndex;

	/** 派发委托：每个 ListenTag 一条委托句柄。 */
	TMap<FGameplayTag, FDelegateHandle> ActiveDelegates;

	/** OnActiveGameplayEffectAddedDelegateToSelf 句柄，用于 EndAbility 时解绑。 */
	FDelegateHandle GEAddedDelegateHandle;

	int32 FrameEventCount = 0;
	uint64 LastFrameNumber = 0;
};
