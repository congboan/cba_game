#include "Experimental/AbilityData/FAssemblyExecution_Projectile.h"
#include "Experimental/AbilitySystem/UGA_AssemblyDataDriven.h"
#include "AbilitySystemComponent.h"
#include "AssemblyAbilityFrameworkLog.h"
#include "GameplayCueManager.h"

void FAssemblyExecution_Projectile::Execute(
	UGA_AssemblyDataDriven& GA,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo& ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// ── 投射物执行策略（占位实现）──────────────────
	//
	// 当前为占位：AAF 保持零耦合，不依赖 BallisticsFramework。
	// 投射物的实际生成和碰撞检测由外部系统（如 BallisticsFramework）负责。
	//
	// 接入方式（二选一）：
	//   A. 外部系统监听 ChainEventTag（推荐，零耦合）
	//      → 外部系统注册一个 EventRouter trigger，监听此 ChainEventTag，
	//        在 trigger 回调中根据 ProjectileDefId/Count/Spread 生成投射物
	//   B. 项目自定义 FAssemblyExecution_Projectile_Subclass
	//      → 继承本类，override Execute()，直接调用项目投射物 API
	//
	// 当前 Execute 只执行 Cue + 发送 ChainEventTag，
	// ChainEventTag 携带 ProjectileDefId/Count/Spread 作为事件上下文。

	if (ProjectileDefId <= 0)
	{
		UE_LOG(LogAssemblyAbility, Warning,
			TEXT("[Projectile] ProjectileDefId=%d 无效，跳过投射物请求。"),
			ProjectileDefId);
	}
	else
	{
		UE_LOG(LogAssemblyAbility, Log,
			TEXT("[Projectile] 投射物请求: DefId=%d, Count=%d, Spread=%.1f "
			     "(实际生成由外部系统监听 ChainEventTag 完成)"),
			ProjectileDefId, ProjectileCount, SpreadAngle);
	}

	// 执行 Cue（如发射音效/视觉）
	if (CueTag.IsValid() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		FGameplayCueParameters CueParams(EffectContext);
		CueParams.Instigator = ActorInfo->AvatarActor.Get();

		ASC->ExecuteGameplayCue(CueTag, CueParams);
	}

	// 发送链事件 — 外部系统监听此事件来生成投射物
	if (ChainEventTag.IsValid() && ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		GA.SendChainEvent(ChainEventTag, ActorInfo, TriggerEventData);
	}
}
