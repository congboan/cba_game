#include "AbilitySystem/UGA_AssemblyEventRouter.h"
#include "AbilitySystem/UGETriggerComponent_Assembly.h"
#include "AbilityData/FAssemblyChainContext.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AssemblyAbilityFrameworkLog.h"
#include "GameplayEffect.h"
#include "StructUtils/InstancedStruct.h"

UGA_AssemblyEventRouter::UGA_AssemblyEventRouter()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
}

// ── ActivateAbility — 监听 ASC 上所有 ActiveGE 的添加 + 历史补扫 ──

void UGA_AssemblyEventRouter::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	// 监听后续所有 ActiveGE 的添加
	GEAddedDelegateHandle = ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
		this, &UGA_AssemblyEventRouter::OnAnyGEAdded);

	// 历史补扫：EventRouter 可能后于其它 GA 激活，把 ASC 上已存在的 ActiveGE 全扫一遍
	{
		FGameplayEffectQuery Query;
		TArray<FActiveGameplayEffectHandle> ExistingHandles = ASC->GetActiveEffects(Query);
		for (const FActiveGameplayEffectHandle& ExistingHandle : ExistingHandles)
		{
			const FActiveGameplayEffect* ActiveGE = ASC->GetActiveGameplayEffect(ExistingHandle);
			if (ActiveGE)
			{
				RegisterTriggersFromActiveGE(ASC, ActiveGE->Spec, ExistingHandle);
			}
		}
	}

	UE_LOG(LogAssemblyAbility, Log, TEXT("EventRouter activated for %s (DispatchTable=%d entries after backfill)."),
		*GetNameSafe(ASC->GetOwner()), DispatchTable.Num());
}

// ── EndAbility — 清理所有委托 + DispatchTable ──────

void UGA_AssemblyEventRouter::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (ASC)
	{
		if (GEAddedDelegateHandle.IsValid())
		{
			ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(GEAddedDelegateHandle);
		}

		for (const TPair<FGameplayTag, FDelegateHandle>& Pair : ActiveDelegates)
		{
			if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(Pair.Key))
			{
				Delegate->Remove(Pair.Value);
			}
		}
	}

	GEAddedDelegateHandle.Reset();
	ActiveDelegates.Empty();
	DispatchTable.Empty();
	GEHandleIndex.Empty();

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ── ASC 上任何 ActiveGE 添加时回调 ─────────────────

void UGA_AssemblyEventRouter::OnAnyGEAdded(
	UAbilitySystemComponent* OwningASC,
	const FGameplayEffectSpec& Spec,
	FActiveGameplayEffectHandle GEHandle)
{
	RegisterTriggersFromActiveGE(OwningASC, Spec, GEHandle);
}

// ── 核心：从 GE Spec 抽 Triggers 注册 ───────────────

void UGA_AssemblyEventRouter::RegisterTriggersFromActiveGE(
	UAbilitySystemComponent* OwningASC,
	const FGameplayEffectSpec& Spec,
	FActiveGameplayEffectHandle GEHandle)
{
	if (!OwningASC || !GEHandle.IsValid())
	{
		return;
	}

	// 从 GE 定义上的 UGETriggerComponent_Assembly 读取 StaticTriggers。
	// 主框架 editor-first 路径：Trigger 在效果资产（GE）上静态配置，
	// 不依赖运行时 EffectContext。GE 不挂该组件时零开销跳过。
	const UGameplayEffect* GEDef = Spec.Def;
	if (!GEDef)
	{
		return;
	}

	const UGETriggerComponent_Assembly* TriggerComp =
		GEDef->FindComponent<UGETriggerComponent_Assembly>();
	if (!TriggerComp || TriggerComp->StaticTriggers.IsEmpty())
	{
		return;
	}

	const TArray<FAssemblyAbilityTrigger>& Triggers = TriggerComp->StaticTriggers;

	// Instant GE 拒绝：瞬时移除无法提供持续监听窗口
	if (GEDef->DurationPolicy == EGameplayEffectDurationType::Instant)
	{
		UE_LOG(LogAssemblyAbility, Warning,
			TEXT("EventRouter: 拒绝在 Instant GE 上注册 Trigger（GE=%s, Triggers=%d）。请改用 Duration/Infinite GE。"),
			*GetNameSafe(GEDef), Triggers.Num());
		return;
	}

	TArray<FTriggerLocation>& Locations = GEHandleIndex.FindOrAdd(GEHandle);

	bool bAnyRegistered = false;
	for (const FAssemblyAbilityTrigger& Trigger : Triggers)
	{
		if (!Trigger.ListenTag.IsValid() || !Trigger.TriggerTag.IsValid())
		{
			UE_LOG(LogAssemblyAbility, Warning,
				TEXT("EventRouter: 跳过非法 Trigger (ListenTag=%s, TriggerTag=%s)."),
				*Trigger.ListenTag.ToString(), *Trigger.TriggerTag.ToString());
			continue;
		}

		FAssemblyTriggerSpec& Entry = DispatchTable.FindOrAdd(Trigger.ListenTag).AddDefaulted_GetRef();
		Entry.Trigger  = Trigger;
		Entry.GEHandle = GEHandle;

		Locations.Add({ Trigger.ListenTag });
		bAnyRegistered = true;

		if (!ActiveDelegates.Contains(Trigger.ListenTag))
		{
			FGameplayEventMulticastDelegate& DispatchDelegate =
				OwningASC->GenericGameplayEventCallbacks.FindOrAdd(Trigger.ListenTag);
			FDelegateHandle Handle = DispatchDelegate.AddUObject(this, &UGA_AssemblyEventRouter::OnDispatchEvent);
			ActiveDelegates.Add(Trigger.ListenTag, Handle);
		}

		UE_LOG(LogAssemblyAbility, Verbose, TEXT("EventRouter: registered '%s' -> '%s' (GE=%s)."),
			*Trigger.ListenTag.ToString(), *Trigger.TriggerTag.ToString(), *GetNameSafe(Spec.Def));
	}

	if (!bAnyRegistered)
	{
		GEHandleIndex.Remove(GEHandle);
		return;
	}

	// 绑定 GE 移除回调（按 GEHandle 自动反向清理）
	const FActiveGameplayEffect* ActiveGE = OwningASC->GetActiveGameplayEffect(GEHandle);
	if (ActiveGE)
	{
		const_cast<FActiveGameplayEffect*>(ActiveGE)->EventSet.OnEffectRemoved.AddUObject(
			this, &UGA_AssemblyEventRouter::OnGERemoved);
	}
}

// ── GE 移除回调 — 按 GEHandle 反查索引清理 ─────────

void UGA_AssemblyEventRouter::OnGERemoved(const FGameplayEffectRemovalInfo& Info)
{
	if (!Info.ActiveEffect)
	{
		return;
	}
	UnregisterByGEHandle(Info.ActiveEffect->Handle);
}

void UGA_AssemblyEventRouter::UnregisterByGEHandle(FActiveGameplayEffectHandle GEHandle)
{
	if (!GEHandle.IsValid())
	{
		return;
	}

	const TArray<FTriggerLocation>* Locations = GEHandleIndex.Find(GEHandle);
	if (!Locations)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetASC();

	// 同一 GE 可能在多个 ListenTag 下注册条目，先去重
	TSet<FGameplayTag> AffectedTags;
	for (const FTriggerLocation& Loc : *Locations)
	{
		AffectedTags.Add(Loc.ListenTag);
	}

	for (const FGameplayTag& ListenTag : AffectedTags)
	{
		TArray<FAssemblyTriggerSpec>* Entries = DispatchTable.Find(ListenTag);
		if (!Entries)
		{
			continue;
		}

		Entries->RemoveAll([GEHandle](const FAssemblyTriggerSpec& Entry)
		{
			return Entry.GEHandle == GEHandle;
		});

		// 数组空了：移除 Map 条目并解绑 ListenTag 派发委托
		if (Entries->IsEmpty())
		{
			if (ASC)
			{
				if (FDelegateHandle* DelHandle = ActiveDelegates.Find(ListenTag))
				{
					if (FGameplayEventMulticastDelegate* Delegate = ASC->GenericGameplayEventCallbacks.Find(ListenTag))
					{
						Delegate->Remove(*DelHandle);
					}
				}
			}
			ActiveDelegates.Remove(ListenTag);
			DispatchTable.Remove(ListenTag);
		}
	}

	GEHandleIndex.Remove(GEHandle);
}

// ── 派发路径：监听 ListenTag 的 GameplayEvent ──────

void UGA_AssemblyEventRouter::OnDispatchEvent(const FGameplayEventData* Payload)
{
	if (!Payload)
	{
		return;
	}

	if (!TryConsumeEventSlot())
	{
		return;
	}

	DispatchEvent(Payload->EventTag, *Payload);
}

void UGA_AssemblyEventRouter::DispatchEvent(FGameplayTag EventTag, const FGameplayEventData& Payload)
{
	UAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}

	TArray<FAssemblyTriggerSpec> EntriesCopy;
	if (const TArray<FAssemblyTriggerSpec>* Entries = DispatchTable.Find(EventTag))
	{
		EntriesCopy = *Entries;
	}

	int32 DispatchCount = 0;
	for (const FAssemblyTriggerSpec& Entry : EntriesCopy)
	{
		if (MaxFanOutCount > 0 && DispatchCount >= MaxFanOutCount)
		{
			UE_LOG(LogAssemblyAbility, Warning, TEXT("Event '%s' reached fan-out limit %d."), *EventTag.ToString(), MaxFanOutCount);
			break;
		}

		AActor* EventTarget = ASC->GetAvatarActor();
		if (!EventTarget)
		{
			EventTarget = ASC->GetOwnerActor();
		}

		FGameplayEventData RoutedPayload = Payload;
		RoutedPayload.EventTag = Entry.Trigger.TriggerTag;

		// 注入 Modifiers 到链上下文
		if (!Entry.Trigger.Modifiers.IsEmpty())
		{
			FAssemblyChainContext Ctx;
			if (RoutedPayload.InstancedEventData.IsValid())
			{
				if (const FAssemblyChainContext* ExistingCtx = RoutedPayload.InstancedEventData.GetPtr<FAssemblyChainContext>())
				{
					Ctx = *ExistingCtx;
				}
			}
			for (const auto& [Tag, Value] : Entry.Trigger.Modifiers)
			{
				if (Tag.IsValid())
				{
					Ctx.DamageModifiers.Add(Tag, Value);
				}
			}
			RoutedPayload.InstancedEventData = FInstancedStruct::Make(Ctx);
		}

		if (EventTarget)
		{
			UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(EventTarget, Entry.Trigger.TriggerTag, RoutedPayload);
			++DispatchCount;
			UE_LOG(LogAssemblyAbility, Verbose, TEXT("Routed event '%s' as '%s'."), *EventTag.ToString(), *Entry.Trigger.TriggerTag.ToString());
		}
	}
}

// ── 调试与限频 ──────────────────────────────────

void UGA_AssemblyEventRouter::DebugDumpTable() const
{
	UE_LOG(LogAssemblyAbility, Log, TEXT("EventRouter: %d listen tags."), DispatchTable.Num());
	for (const TPair<FGameplayTag, TArray<FAssemblyTriggerSpec>>& Pair : DispatchTable)
	{
		UE_LOG(LogAssemblyAbility, Log, TEXT("  %s -> %d entries"), *Pair.Key.ToString(), Pair.Value.Num());
		for (const FAssemblyTriggerSpec& Entry : Pair.Value)
		{
			UE_LOG(LogAssemblyAbility, Log, TEXT("    ListenTag: %s, TriggerTag: %s, GE: %s"),
				*Entry.Trigger.ListenTag.ToString(), *Entry.Trigger.TriggerTag.ToString(),
				Entry.GEHandle.IsValid() ? TEXT("valid") : TEXT("invalid"));
		}
	}
}

bool UGA_AssemblyEventRouter::CheckChainDepth(int32 CurrentDepth) const
{
	return MaxChainDepth <= 0 || CurrentDepth < MaxChainDepth;
}

bool UGA_AssemblyEventRouter::TryConsumeEventSlot()
{
	if (MaxEventsPerFrame <= 0)
	{
		return true;
	}

	const uint64 CurrentFrame = GFrameCounter;
	if (CurrentFrame != LastFrameNumber)
	{
		FrameEventCount = 0;
		LastFrameNumber = CurrentFrame;
	}

	if (FrameEventCount >= MaxEventsPerFrame)
	{
		UE_LOG(LogAssemblyAbility, Warning, TEXT("Per-frame event limit reached: %d."), MaxEventsPerFrame);
		return false;
	}

	++FrameEventCount;
	return true;
}

void UGA_AssemblyEventRouter::ResetFrameEventCounter()
{
	FrameEventCount = 0;
	LastFrameNumber = GFrameCounter;
}

UAbilitySystemComponent* UGA_AssemblyEventRouter::GetASC() const
{
	return GetActorInfo().AbilitySystemComponent.Get();
}
