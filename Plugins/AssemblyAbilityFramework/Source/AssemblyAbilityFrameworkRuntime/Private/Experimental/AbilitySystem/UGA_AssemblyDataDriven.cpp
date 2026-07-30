#include "Experimental/AbilitySystem/UGA_AssemblyDataDriven.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AssemblyAbilityFrameworkLog.h"
#include "GameplayEffect.h"
#include "GameplayCueManager.h"
#include "GameplayEffectTypes.h"
#include "Experimental/UAssemblyAbilitySource.h"
#include "Experimental/AbilityData/FAssemblyAbilityRow.h"
#include "Experimental/AbilityData/FAssemblyExecutionBase.h"
#include "Experimental/AbilityData/Context/FAssemblyEffectContext.h"

UGA_AssemblyDataDriven::UGA_AssemblyDataDriven()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

// ── EffectContext / AssemblySource（自基类下沉）─────

FGameplayEffectContextHandle UGA_AssemblyDataDriven::MakeEffectContext(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo) const
{
	check(ActorInfo);

	UAssemblyAbilitySource* Source = nullptr;
	if (const FGameplayAbilitySpec* Spec = ActorInfo->AbilitySystemComponent.IsValid()
		? ActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(Handle)
		: nullptr)
	{
		Source = Cast<UAssemblyAbilitySource>(Spec->SourceObject);
	}

	AActor* Instigator   = ActorInfo->OwnerActor.Get();
	AActor* EffectCauser = ActorInfo->AvatarActor.Get();

	return FAssemblyEffectContext::MakeForAssembly(Source, GetOriginAbilityTag(), Instigator, EffectCauser);
}

UAssemblyAbilitySource* UGA_AssemblyDataDriven::GetAssemblySource() const
{
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	if (!Spec)
	{
		return nullptr;
	}

	return Cast<UAssemblyAbilitySource>(Spec->SourceObject);
}

const FAssemblyEffectContext* UGA_AssemblyDataDriven::GetAssemblyContextFromSpec(const FGameplayEffectSpec& Spec)
{
	return GetAssemblyContextFromHandle(Spec.GetEffectContext());
}

const FAssemblyEffectContext* UGA_AssemblyDataDriven::GetAssemblyContextFromHandle(const FGameplayEffectContextHandle& Handle)
{
	const FGameplayEffectContext* RawCtx = Handle.Get();
	if (!RawCtx)
	{
		return nullptr;
	}

	if (RawCtx->GetScriptStruct() != FAssemblyEffectContext::StaticStruct())
	{
		return nullptr;
	}

	return static_cast<const FAssemblyEffectContext*>(RawCtx);
}

FGameplayTag UGA_AssemblyDataDriven::GetOriginAbilityTag() const
{
	if (UAssemblyAbilitySource* Source = GetAssemblySource())
	{
		if (Source->Row.AbilityTag.IsValid())
		{
			return Source->Row.AbilityTag;
		}
	}

	return FGameplayTag::EmptyTag;
}

// ── 数据访问 ──────────────────────────────────────

const FAssemblyAbilityRow* UGA_AssemblyDataDriven::GetRow() const
{
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	if (!Spec)
	{
		return nullptr;
	}

	if (const UAssemblyAbilitySource* Source = Cast<UAssemblyAbilitySource>(Spec->SourceObject))
	{
		return &Source->Row;
	}

	return nullptr;
}

UObject* UGA_AssemblyDataDriven::GetOriginSource() const
{
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	if (!Spec)
	{
		return nullptr;
	}

	if (const UAssemblyAbilitySource* Source = Cast<UAssemblyAbilitySource>(Spec->SourceObject))
	{
		return Source->OriginSource;
	}

	// 兼容非包装路径：直接就是来源对象
	return Spec->SourceObject.Get();
}

FAssemblyExecutionBase* UGA_AssemblyDataDriven::ResolveExecutionData()
{
	const FAssemblyAbilityRow* Row = GetRow();
	if (!Row)
	{
		return nullptr;
	}

	// 检查覆写：如果持有者拥有匹配的标签，则使用替代的执行策略
	if (!Row->ExecutionOverrides.IsEmpty())
	{
		const FGameplayAbilityActorInfo& Info = GetActorInfo();
		if (Info.AbilitySystemComponent.IsValid())
		{
			UAbilitySystemComponent* ASC = Info.AbilitySystemComponent.Get();
			for (const auto& [Tag, OverrideExec] : Row->ExecutionOverrides)
			{
				if (Tag.IsValid() && ASC->HasMatchingGameplayTag(Tag))
				{
					UE_LOG(LogAssemblyAbility, Verbose,
						TEXT("[AssemblyAbility] 覆写：持有者拥有 '%s'，使用替代的执行策略。"),
						*Tag.ToString());
					return const_cast<FAssemblyExecutionBase*>(&OverrideExec.Get());
				}
			}
		}
	}

	// 使用默认执行策略
	if (Row->ExecutionStrategy.IsValid())
	{
		return const_cast<FAssemblyExecutionBase*>(&Row->ExecutionStrategy.Get());
	}

	return nullptr;
}

// ── 虚函数覆写 ────────────────────────────────────

UGameplayEffect* UGA_AssemblyDataDriven::GetCooldownGameplayEffect() const
{
	const FAssemblyAbilityRow* Row = GetRow();
	if (Row && !Row->CooldownGE.IsNull())
	{
		return Row->CooldownGE.LoadSynchronous();
	}

	return Super::GetCooldownGameplayEffect();
}

UGameplayEffect* UGA_AssemblyDataDriven::GetCostGameplayEffect() const
{
	const FAssemblyAbilityRow* Row = GetRow();
	if (Row && !Row->CostGE.IsNull())
	{
		return Row->CostGE.LoadSynchronous();
	}

	return Super::GetCostGameplayEffect();
}

bool UGA_AssemblyDataDriven::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const FAssemblyAbilityRow* Row = GetRow();
	if (Row && !Row->ActivationBlockedTags.IsEmpty())
	{
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			if (ActorInfo->AbilitySystemComponent->HasAnyMatchingGameplayTags(Row->ActivationBlockedTags))
			{
				return false;
			}
		}
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

// ── ApplyCooldown — 注入 CooldownOverride SetByCaller ──

void UGA_AssemblyDataDriven::ApplyCooldown(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
	if (!CooldownGE || !ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CooldownGE->GetClass(), GetAbilityLevel());
	if (!SpecHandle.IsValid())
	{
		return;
	}

	// 获取冷却标签并附加到 Spec
	if (const FGameplayTagContainer* CooldownTagsPtr = GetCooldownTags())
	{
		SpecHandle.Data->DynamicGrantedTags.AppendTags(*CooldownTagsPtr);
	}

	// 注入 CooldownOverride — 通过 SetByCaller 覆写 Duration
	const FAssemblyAbilityRow* Row = GetRow();
	if (Row && !Row->CooldownOverrideTags.IsEmpty())
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		for (const FGameplayTag& OverrideTag : Row->CooldownOverrideTags)
		{
			if (ASC->HasMatchingGameplayTag(OverrideTag))
			{
				if (const float* OverrideCD = Row->PerTagCooldown.Find(OverrideTag))
				{
					SpecHandle.Data->SetSetByCallerMagnitude(OverrideTag, *OverrideCD);
					UE_LOG(LogAssemblyAbility, Log,
						TEXT("[AssemblyAbility] 冷却覆写：标签 '%s' → %.1f 秒"),
						*OverrideTag.ToString(), *OverrideCD);
				}
			}
		}
	}

	// Apply 到自身 — 用 ASC 直接 Apply Spec
	ActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
}

// ── ActivateAbility — 执行管线 ──────────────────

void UGA_AssemblyDataDriven::ActivateAbility(
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

	const FAssemblyAbilityRow* Row = GetRow();
	if (!Row)
	{
		UE_LOG(LogAssemblyAbility, Warning,
			TEXT("[AssemblyAbility] DataDriven: 未找到 Row 数据。终止。"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 步骤 1：取消冲突技能
	if (!Row->CancelAbilitiesWithTags.IsEmpty())
	{
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			ActorInfo->AbilitySystemComponent->CancelAbilities(&Row->CancelAbilitiesWithTags);
		}
	}

	// 步骤 2：执行策略
	FAssemblyExecutionBase* Exec = ResolveExecutionData();
	if (Exec)
	{
		Exec->Execute(*this, ActorInfo, ActivationInfo, TriggerEventData);
	}
	else
	{
		UE_LOG(LogAssemblyAbility, Warning,
			TEXT("[AssemblyAbility] DataDriven: 无执行策略。技能 '%s' 仅执行了 Commit。"),
			*Row->AbilityTag.ToString());
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}
