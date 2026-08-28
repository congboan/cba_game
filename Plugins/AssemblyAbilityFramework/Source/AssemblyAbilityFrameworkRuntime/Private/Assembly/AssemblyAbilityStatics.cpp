#include "Assembly/AssemblyAbilityStatics.h"
#include "AbilityData/FAssemblyAbilityRow.h"
#include "AbilityData/Fragment/FAssemblyFragment_Trigger.h"
#include "AbilityData/Execution/FAssemblyExecutionBase.h"
#include "Assembly/UAssemblyAbilitySource.h"
#include "AbilitySystem/UGA_AssemblyDataDriven.h"
#include "AbilitySystem/UGA_AssemblyEventRouter.h"
#include "AbilitySystemComponent.h"
#include "AssemblyAbilityFrameworkLog.h"
#include "DataRegistrySubsystem.h"
#include "GameplayEffect.h"
#include "AbilityData/FAssemblyChainContext.h"
#include "AbilityData/FAssemblyChainTargetData.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "StructUtils/InstancedStruct.h"

// ── GiveAbilityFromRow — 核心授权入口 ──────────────

FGameplayAbilitySpecHandle UAssemblyAbilityStatics::GiveAbilityFromRow(
	const FAssemblyAbilityRow& Row,
	UAbilitySystemComponent* ASC,
	int32 Level,
	UObject* OriginSource)
{
	if (!ASC)
	{
		return FGameplayAbilitySpecHandle();
	}

	if (!ensureMsgf(ASC->GetOwnerActor() && ASC->GetOwnerActor()->HasAuthority(),
		TEXT("[AssemblyAbility] GiveAbilityFromRow must be called on server (ASC owner: %s)."),
		*GetNameSafe(ASC->GetOwner())))
	{
		return FGameplayAbilitySpecHandle();
	}

	const FGameplayTag AbilityTag = Row.AbilityTag;
	if (!AbilityTag.IsValid())
	{
		return FGameplayAbilitySpecHandle();
	}

	TSubclassOf<UGameplayAbility> AbilityClass = UGA_AssemblyDataDriven::StaticClass();
	if (Row.AbilityClass.Get())
	{
		AbilityClass = Row.AbilityClass;
	}

	// 步骤 1：创建 Spec（保持类构造：InstancedPerActor 下 Spec.Ability 必须是 CDO，
	// CreateNewInstanceOfAbility check RF_ClassDefaultObject 实证；行身份经实例直写注入）
	FGameplayAbilitySpec Spec(AbilityClass, Level);

	// 步骤 2：创建 Source 包装对象，持有 Row 副本
	UObject* SourceOuter = ASC->GetOwner();
	UAssemblyAbilitySource* AbilitySource = NewObject<UAssemblyAbilitySource>(SourceOuter);
	AbilitySource->CompiledRow.Row = Row;  // 值拷贝
	AbilitySource->OriginSource = OriginSource;

	// ── 授予期预载（runtime bake at grant time）：激活热路径零同步 IO ──
	{
		auto& SrcRow = AbilitySource->CompiledRow.Row;

		auto PrecacheOneFragment = [AbilitySource](TInstancedStruct<FAssemblyFragmentBase>& Fragment)
		{
			if (FAssemblyFragmentBase* FragmentPtr = Fragment.GetMutablePtr<FAssemblyFragmentBase>())
			{
				FragmentPtr->Precache(AbilitySource);
			}
		};
		for (TInstancedStruct<FAssemblyFragmentBase>& SrcFragment : SrcRow.Fragments)
		{
			PrecacheOneFragment(SrcFragment);
		}

		auto PrecacheOne = [AbilitySource](TInstancedStruct<FAssemblyExecutionBase>& Exec)
		{
			if (FAssemblyExecutionBase* ExecPtr = Exec.GetMutablePtr<FAssemblyExecutionBase>())
			{
				ExecPtr->Precache(AbilitySource);
			}
		};
		PrecacheOne(SrcRow.ExecutionStrategy);
		for (FNamedExecutionOverride& OverrideEntry : SrcRow.ExecutionOverrides)
		{
			PrecacheOne(OverrideEntry.Strategy);
		}
	}

	// 步骤 4：挂到 Spec.SourceObject
	Spec.SourceObject = AbilitySource;

	// 步骤 4b：授予期贡献（泛化编排 —— 片段自写 Spec，如 Trigger 片 → DynamicAbilityTriggers，
	// 引擎在 GiveAbility 时原生注册/注销，AbilitySystemComponent_Abilities.cpp:578/:637）
	for (TInstancedStruct<FAssemblyFragmentBase>& GrantFragment : AbilitySource->CompiledRow.Row.Fragments)
	{
		if (FAssemblyFragmentBase* GrantFragmentPtr = GrantFragment.GetMutablePtr<FAssemblyFragmentBase>())
		{
			GrantFragmentPtr->ContributeToSpec(Spec);
		}
	}

	// 步骤 5：授予技能
	FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);
	if (Handle.IsValid())
	{
		// 步骤 6：确保此 ASC 上 EventRouter 正在运行
		GetOrCreateEventRouter(ASC);

		UE_LOG(LogAssemblyAbility, Log,
			TEXT("[AssemblyAbility] GiveAbilityFromRow: 已授予 '%s' (Handle: %s, Level: %d, OriginSource: %s)"),
			*AbilityTag.ToString(), *Handle.ToString(), Level,
			OriginSource ? *OriginSource->GetName() : TEXT("none"));
	}

	return Handle;
}

// GiveAbilityFromDataRegistry - DataRegistry lookup

FGameplayAbilitySpecHandle UAssemblyAbilityStatics::GiveAbilityFromDataRegistry(
	FDataRegistryId AbilityId,
	UAbilitySystemComponent* ASC,
	int32 Level,
	UObject* OriginSource)
{
	if (!ASC || !AbilityId.IsValid())
	{
		return FGameplayAbilitySpecHandle();
	}

	if (!ensureMsgf(ASC->GetOwnerActor() && ASC->GetOwnerActor()->HasAuthority(),
		TEXT("[AssemblyAbility] GiveAbilityFromDataRegistry must be called on server (ASC owner: %s)."),
		*GetNameSafe(ASC->GetOwner())))
	{
		return FGameplayAbilitySpecHandle();
	}

	const UDataRegistrySubsystem* DRSubsystem = UDataRegistrySubsystem::Get();
	if (!DRSubsystem)
	{
		return FGameplayAbilitySpecHandle();
	}

	const FAssemblyAbilityRow* Row = DRSubsystem->GetCachedItem<FAssemblyAbilityRow>(AbilityId);
	if (!Row)
	{
		return FGameplayAbilitySpecHandle();
	}

	return GiveAbilityFromRow(*Row, ASC, Level, OriginSource);
}

// GiveAbilityFromTag - convenience wrapper

FGameplayAbilitySpecHandle UAssemblyAbilityStatics::GiveAbilityFromTag(
	FGameplayTag AbilityTag,
	UAbilitySystemComponent* ASC,
	int32 Level,
	UObject* OriginSource)
{
	if (!AbilityTag.IsValid() || !ASC)
	{
		return FGameplayAbilitySpecHandle();
	}

	const FDataRegistryId RegistryId(TEXT("AssemblyAbility"), AbilityTag.GetTagName());
	return GiveAbilityFromDataRegistry(RegistryId, ASC, Level, OriginSource);
}

// FindAbilityByAbilityTag - search granted specs by AbilityTag

FGameplayAbilitySpecHandle UAssemblyAbilityStatics::FindAbilityByAbilityTag(
	UAbilitySystemComponent* ASC,
	FGameplayTag AbilityTag)
{
	if (!ASC || !AbilityTag.IsValid())
	{
		return FGameplayAbilitySpecHandle();
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.GetDynamicSpecSourceTags().HasTag(AbilityTag))
		{
			return Spec.Handle;
		}
	}

	return FGameplayAbilitySpecHandle();
}

// Internal: EventRouter get-or-create

UGA_AssemblyEventRouter* UAssemblyAbilityStatics::GetOrCreateEventRouter(UAbilitySystemComponent* ASC)
{
	if (!ASC)
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
	{
		if (Spec.Ability && Spec.Ability->IsA<UGA_AssemblyEventRouter>())
		{
			return Cast<UGA_AssemblyEventRouter>(Spec.GetPrimaryInstance());
		}
	}

	FGameplayAbilitySpec RouterSpec(UGA_AssemblyEventRouter::StaticClass(), 1);
	FGameplayAbilitySpecHandle RouterHandle = ASC->GiveAbility(RouterSpec);

	bool bActivated = ASC->TryActivateAbility(RouterHandle);

	UGA_AssemblyEventRouter* Router = nullptr;
	if (bActivated)
	{
		FGameplayAbilitySpec* GrantedSpec = ASC->FindAbilitySpecFromHandle(RouterHandle);
		Router = GrantedSpec
			? Cast<UGA_AssemblyEventRouter>(GrantedSpec->GetPrimaryInstance())
			: nullptr;
	}

	if (!Router)
	{
		UE_LOG(LogAssemblyAbility, Error,
			TEXT("[AssemblyAbility] EventRouter activation failed (ASC: %s, Handle: %s)."),
			*GetNameSafe(ASC->GetOwner()), *RouterHandle.ToString());
		return nullptr;
	}

	return Router;
}

// ── Chain context carrier adaptation (5.9+ InstancedEventData / <5.9 TargetData) ──

void UAssemblyAbilityStatics::SetChainContext(FGameplayEventData& EventData, const FAssemblyChainContext& Ctx)
{
#if AAF_HAS_INSTANCED_EVENT_DATA
	EventData.InstancedEventData = FInstancedStruct::Make(Ctx);
#else
	EventData.TargetData.Add(new FAssemblyChainTargetData(Ctx));
#endif
}

const FAssemblyChainContext* UAssemblyAbilityStatics::GetChainContextFrom(const FGameplayEventData& EventData)
{
#if AAF_HAS_INSTANCED_EVENT_DATA
	if (EventData.InstancedEventData.IsValid())
	{
		return EventData.InstancedEventData.GetPtr<FAssemblyChainContext>();
	}
	return nullptr;
#else
	const FAssemblyChainTargetData* TD = EventData.TargetData.FindFirst<FAssemblyChainTargetData>();
	return TD ? &TD->Context : nullptr;
#endif
}

void UAssemblyAbilityStatics::GetTargetActors(const FGameplayEventData& EventData, TArray<AActor*>& OutActors)
{
#if !AAF_HAS_INSTANCED_EVENT_DATA
	const UScriptStruct* ChainType = FAssemblyChainTargetData::StaticStruct();
#endif
	for (const TSharedPtr<FGameplayAbilityTargetData>& TD : EventData.TargetData.Data)
	{
		if (!TD.IsValid())
		{
			continue;
		}
#if !AAF_HAS_INSTANCED_EVENT_DATA
		// <5.9: skip chain-context carrier (not target data)
		if (TD->GetScriptStruct() && TD->GetScriptStruct()->IsChildOf(ChainType))
		{
			continue;
		}
#endif
		for (TWeakObjectPtr<AActor> Actor : TD->GetActors())
		{
			if (Actor.IsValid())
			{
				OutActors.AddUnique(Actor.Get());
			}
		}
	}
}
