#include "AbilitySystem/UGA_AssemblyBase.h"
#include "AbilitySystemComponent.h"
#include "AssemblyAbilityFrameworkLog.h"
#include "StructUtils/InstancedStruct.h"

UGA_AssemblyBase::UGA_AssemblyBase()
{
	// 默认实例化：每 Actor 实例一份，支持 LocalPredicted 网络模型
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

// ── OnAvatarSet ──────────────────────────────────

void UGA_AssemblyBase::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);

	// OnSpawn 策略：Give 完立即激活（OnAvatarSet 是引擎在 Give 时也会调用一次的钩子，与 Lyra 一致）
	if (ActivationPolicy == EAssemblyAbilityActivationPolicy::OnSpawn
	 || ActivationPolicy == EAssemblyAbilityActivationPolicy::OnAvatarSet)
	{
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			ActorInfo->AbilitySystemComponent->TryActivateAbility(Spec.Handle);
		}
	}
}

// ── NativeOnAbilityFailedToActivate ──────────────

void UGA_AssemblyBase::NativeOnAbilityFailedToActivate(const FGameplayTagContainer& FailedReason) const
{
	if (FailureTagToUserFacingMessages.IsEmpty())
	{
		return;
	}

	for (const FGameplayTag& Reason : FailedReason)
	{
		if (const FText* Message = FailureTagToUserFacingMessages.Find(Reason))
		{
			UE_LOG(LogAssemblyAbility, Log,
				TEXT("[AssemblyAbility] '%s' 激活失败: %s -> \"%s\""),
				*GetName(), *Reason.ToString(), *Message->ToString());
		}
	}
}

// ── CancelAbility ────────────────────────────────

void UGA_AssemblyBase::CancelAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateCancelAbility)
{
	if (bLogCancelation)
	{
		UE_LOG(LogAssemblyAbility, Warning,
			TEXT("[AssemblyAbility] '%s' 被取消（CancelAbility）。"),
			*GetName());
	}

	Super::CancelAbility(Handle, ActorInfo, ActivationInfo, bReplicateCancelAbility);
}

// ── 链式触发上下文收发 ───────────────────────────

void UGA_AssemblyBase::SendChainEvent(
	const FGameplayTag& EventTag,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!EventTag.IsValid() || !ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	// 从 InstancedEventData 读取链上下文
	FAssemblyChainContext Ctx;
	if (TriggerEventData && TriggerEventData->InstancedEventData.IsValid())
	{
		if (const FAssemblyChainContext* ParentCtx = TriggerEventData->InstancedEventData.GetPtr<FAssemblyChainContext>())
		{
			Ctx = *ParentCtx;
		}
	}

	// 安全检查：链深度限制
	if (Ctx.ChainDepth >= MaxChainDepth)
	{
		UE_LOG(LogAssemblyAbility, Warning,
			TEXT("[AssemblyAbility] 链事件 '%s' 在深度 %d 处被阻止（最大 %d）。"),
			*EventTag.ToString(), Ctx.ChainDepth, MaxChainDepth);
		return;
	}

	// 推进链深度
	Ctx.ChainDepth++;
	if (!Ctx.RootEventId.IsValid())
	{
		Ctx.RootEventId = FGuid::NewGuid();
	}
	Ctx.InstigatorActor = ActorInfo->AvatarActor.Get();

	FGameplayEventData ChainData;
	if (TriggerEventData)
	{
		ChainData = *TriggerEventData;
	}

	ChainData.EventTag   = EventTag;
	ChainData.Instigator = ActorInfo->AvatarActor.Get();
	ChainData.Target     = ActorInfo->AvatarActor.Get();

	ChainData.InstancedEventData = FInstancedStruct::Make(Ctx);

	ASC->HandleGameplayEvent(EventTag, &ChainData);
}

void UGA_AssemblyBase::K2_SendChainEvent(FGameplayTag EventTag, const FAssemblyChainContext& ParentContext)
{
	const FGameplayAbilityActorInfo* ActorInfo = GetCurrentActorInfo();

	// 用调用方传入的 ParentContext 组装一个临时 EventData，复用 C++ 全保真路径
	FGameplayEventData Seed;
	Seed.InstancedEventData = FInstancedStruct::Make(ParentContext);

	SendChainEvent(EventTag, ActorInfo, &Seed);
}

FAssemblyChainContext UGA_AssemblyBase::GetChainContext(const FGameplayEventData& TriggerEventData, bool& bFound) const
{
	if (TriggerEventData.InstancedEventData.IsValid())
	{
		if (const FAssemblyChainContext* Ctx = TriggerEventData.InstancedEventData.GetPtr<FAssemblyChainContext>())
		{
			bFound = true;
			return *Ctx;
		}
	}

	bFound = false;
	return FAssemblyChainContext();
}
