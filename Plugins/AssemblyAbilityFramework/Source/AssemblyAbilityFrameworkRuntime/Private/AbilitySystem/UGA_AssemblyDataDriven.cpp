#include "AbilitySystem/UGA_AssemblyDataDriven.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AssemblyAbilityFrameworkLog.h"
#include "GameplayEffect.h"
#include "GameplayCueManager.h"
#include "GameplayEffectTypes.h"
#include "Assembly/UAssemblyAbilitySource.h"
#include "AbilityData/FAssemblyAbilityRow.h"
#include "AbilityData/Fragment/FAssemblyFragmentBase.h"
#include "AbilityData/Execution/FAssemblyExecutionBase.h"
#include "AbilityData/Fragment/FAssemblyFragment_Trigger.h"
#include "AbilityData/Fragment/FAssemblyFragment_Tags.h"
#include "AbilityData/Context/FAssemblyEffectContext.h"

UGA_AssemblyDataDriven::UGA_AssemblyDataDriven()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

// ── EffectContext / AssemblySource ──────────────────

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
		if (Source->CompiledRow.Row.AbilityTag.IsValid())
		{
			return Source->CompiledRow.Row.AbilityTag;
		}
	}

	return FGameplayTag::EmptyTag;
}

// ── Data access ─────────────────────────────────────

const FAssemblyAbilityRow* UGA_AssemblyDataDriven::GetRow() const
{
	const FGameplayAbilitySpec* Spec = GetCurrentAbilitySpec();
	if (!Spec)
	{
		return nullptr;
	}

	if (const UAssemblyAbilitySource* Source = Cast<UAssemblyAbilitySource>(Spec->SourceObject))
	{
		return &Source->CompiledRow.Row;
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

	// Non-wrapped path: SourceObject is the origin source directly
	return Spec->SourceObject.Get();
}

FGameplayTagContainer UGA_AssemblyDataDriven::GetIdentityTags() const
{
	FGameplayTagContainer IdentityTags;
	if (const UAssemblyAbilitySource* Source = GetAssemblySource())
	{
		// Aggregate all Tags fragments via CollectAssetTags (multi-fragment stackable)
		ForEachHook<FAssemblyFragment_Tags>(Source, [&IdentityTags](const FAssemblyFragment_Tags& Fragment)
		{
			Fragment.CollectAssetTags(IdentityTags);
		});
	}
	return IdentityTags;
}

FGameplayTagContainer UGA_AssemblyDataDriven::GetActivationOwnedTags() const
{
	FGameplayTagContainer OwnedTags;
	if (const UAssemblyAbilitySource* Source = GetAssemblySource())
	{
		// Aggregate all Tags fragments via CollectActivationOwnedTags (multi-fragment stackable)
		ForEachHook<FAssemblyFragment_Tags>(Source, [&OwnedTags](const FAssemblyFragment_Tags& Fragment)
		{
			Fragment.CollectActivationOwnedTags(OwnedTags);
		});
	}
	return OwnedTags;
}

FAssemblyExecutionBase* UGA_AssemblyDataDriven::ResolveExecutionData()
{
	const FAssemblyAbilityRow* Row = GetRow();
	if (!Row)
	{
		return nullptr;
	}

	// Check overrides: first matching holder tag wins (first-match-wins)
	if (!Row->ExecutionOverrides.IsEmpty())
	{
		const FGameplayAbilityActorInfo& Info = GetActorInfo();
		if (Info.AbilitySystemComponent.IsValid())
		{
			UAbilitySystemComponent* ASC = Info.AbilitySystemComponent.Get();
			for (int32 OI = 0; OI < Row->ExecutionOverrides.Num(); ++OI)
			{
				const FNamedExecutionOverride& Entry = Row->ExecutionOverrides[OI];
				const FGameplayTag& Tag = Entry.HolderTag;
				const auto& OverrideExec = Entry.Strategy;
				if (Entry.HolderTag.IsValid() && ASC->HasMatchingGameplayTag(Entry.HolderTag))
				{
					UE_LOG(LogAssemblyAbility, Verbose,
						TEXT("[AssemblyAbility] Override: owner has '%s', using alternate strategy."),
						*Tag.ToString());
					return const_cast<FAssemblyExecutionBase*>(&OverrideExec.Get());
				}
			}
		}
	}

	// Default execution strategy
	if (Row->ExecutionStrategy.IsValid())
	{
		return const_cast<FAssemblyExecutionBase*>(&Row->ExecutionStrategy.Get());
	}

	return nullptr;
}

// ── Overrides (engine native Cost/Cooldown channels retired; settlement lives in Fragments) ──

bool UGA_AssemblyDataDriven::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	const FAssemblyAbilityRow* Row = GetRow();
	UAssemblyAbilitySource* Source = GetAssemblySource();

	// Unified failure exit: broadcast OnFailed hooks then reject
	auto Fail = [&](FGameplayTagContainer* OutTags) -> bool
	{
		if (Source)
		{
			FGameplayTagContainer FailTags;
			if (OutTags)
			{
				FailTags = *OutTags;
			}
			ForEachHook<FAssemblyFragmentOnFailedBase>(Source,
				[&](const FAssemblyFragmentOnFailedBase& Hook)
				{
					Hook.OnFailed(*this, FailTags);
				});
		}
		return false;
	};

	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return Fail(OptionalRelevantTags);
	}

	// Tag judgement (engine DoesAbilitySatisfyTagRequirements equivalent, GameplayAbility.cpp:349-443):
	// data lives in Tags fragment, judgement unified inside GA. Exact HasAny/HasAll matching
	// (not HasAnyMatching/HasAllMatching parent-expansion variants). All blocked -> all required -> AssetBlock.
	if (Source && ActorInfo)
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		const FGameplayTagContainer& OwnedTags = ASC->GetOwnedGameplayTags();
		bool bFailed = false;

		// Engine :353-376 equivalent: any hit writes global blocked tag + matching tags
		auto FailBlocked = [&](const FGameplayTagContainer& A, const FGameplayTagContainer& B)
		{
			if (A.IsEmpty() || B.IsEmpty() || !A.HasAny(B))
			{
				return;
			}
			if (OptionalRelevantTags)
			{
				if (!bFailed)
				{
					OptionalRelevantTags->AddTag(UAbilitySystemGlobals::Get().ActivateFailTagsBlockedTag);
				}
				OptionalRelevantTags->AppendMatchingTags(A, B);
			}
			bFailed = true;
		};

		// Engine :380-404 equivalent: missing writes global missing tag + missing set
		auto FailRequired = [&](const FGameplayTagContainer& TagsToCheck, const FGameplayTagContainer& RequiredTags)
		{
			if (RequiredTags.IsEmpty() || TagsToCheck.HasAll(RequiredTags))
			{
				return;
			}
			if (OptionalRelevantTags)
			{
				if (!bFailed)
				{
					OptionalRelevantTags->AddTag(UAbilitySystemGlobals::Get().ActivateFailTagsMissingTag);
				}
				FGameplayTagContainer MissingTags = RequiredTags;
				MissingTags.RemoveTags(TagsToCheck.GetGameplayTagParents());
				OptionalRelevantTags->AppendTags(MissingTags);
			}
			bFailed = true;
		};

		ForEachHook<FAssemblyFragment_Tags>(Source,
			[&](const FAssemblyFragment_Tags& Fragment)
			{
				if (bFailed)
				{
					return;
				}
				// Blocked sequence (engine :406-416 order)
				{
					FGameplayTagContainer Local;
					Fragment.CollectActivationBlockedTags(Local);
					FailBlocked(OwnedTags, Local);
				}
				if (SourceTags)
				{
					FGameplayTagContainer Local;
					Fragment.CollectSourceBlockedTags(Local);
					FailBlocked(*SourceTags, Local);
				}
				if (TargetTags)
				{
					FGameplayTagContainer Local;
					Fragment.CollectTargetBlockedTags(Local);
					FailBlocked(*TargetTags, Local);
				}
				// Required sequence (engine :418-427 order)
				{
					FGameplayTagContainer Local;
					Fragment.CollectActivationRequiredTags(Local);
					FailRequired(OwnedTags, Local);
				}
				if (SourceTags)
				{
					FGameplayTagContainer Local;
					Fragment.CollectSourceRequiredTags(Local);
					FailRequired(*SourceTags, Local);
				}
				if (TargetTags)
				{
					FGameplayTagContainer Local;
					Fragment.CollectTargetRequiredTags(Local);
					FailRequired(*TargetTags, Local);
				}
				// Row identity global block (engine :432 equivalent, checked only when no blocked/missing)
				if (!bFailed && Fragment.bCheckAssetBlock)
				{
					const FGameplayTagContainer IdentityTags = GetIdentityTags();
					if (!IdentityTags.IsEmpty() && ASC->AreAbilityTagsBlocked(IdentityTags))
					{
						if (OptionalRelevantTags)
						{
							OptionalRelevantTags->AddTag(UAbilitySystemGlobals::Get().ActivateFailTagsBlockedTag);
						}
						bFailed = true;
					}
				}
			});
		if (bFailed)
		{
			return Fail(OptionalRelevantTags);
		}
	}

	return true;
}

// ── CheckCost / ApplyCost — cost narrow-base table dispatch ──

bool UGA_AssemblyDataDriven::CheckCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
	{
		return false;
	}

	UAssemblyAbilitySource* Source = GetAssemblySource();
	if (!Source)
	{
		return true; // No row data: no fragment gates
	}

	// Cost narrow-base gate: first-fail
	bool bCanAfford = true;
	ForEachHook<FAssemblyFragmentCostBase>(Source,
		[&](const FAssemblyFragmentCostBase& Fragment)
		{
			if (bCanAfford && !Fragment.Check(*this, Handle, ActorInfo, OptionalRelevantTags))
			{
				bCanAfford = false;
			}
		});
	return bCanAfford;
}

void UGA_AssemblyDataDriven::ApplyCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

	UAssemblyAbilitySource* Source = GetAssemblySource();
	if (!Source)
	{
		return;
	}

	// Cost narrow-base: settle each fragment
	ForEachHook<FAssemblyFragmentCostBase>(Source,
		[&](const FAssemblyFragmentCostBase& Fragment)
		{
			Fragment.Apply(*this, Handle, ActorInfo, ActivationInfo);
		});
}

// ── ActivateAbility — execution pipeline ────────────

void UGA_AssemblyDataDriven::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	// Row identity injection (instance-level, not CDO): engine GetAssetTags() non-virtual reads CDO,
	// dynamic row identity cannot enter CDO (class-shared + SetAssetTags construction lock proven).
	// This instance is a per-actor object; direct-write public AbilityTags so engine block/cancel/
	// cancelable side effects read row identity. AbilityTags deprecated 5.5, compatibility path.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	AbilityTags = GetIdentityTags();
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		if (UAssemblyAbilitySource* FailSource = GetAssemblySource())
		{
			ForEachHook<FAssemblyFragmentOnFailedBase>(FailSource,
				[&](const FAssemblyFragmentOnFailedBase& Hook)
				{
					Hook.OnFailed(*this, FGameplayTagContainer());
				});
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Activation established hook (Commit success)
	if (UAssemblyAbilitySource* Source = GetAssemblySource())
	{
		ForEachHook<FAssemblyFragmentOnActivatedBase>(Source,
			[&](const FAssemblyFragmentOnActivatedBase& Hook)
			{
				Hook.OnActivated(*this, *ActorInfo);
			});
	}

	const FAssemblyAbilityRow* Row = GetRow();
	if (!Row)
	{
		UE_LOG(LogAssemblyAbility, Warning,
			TEXT("[AssemblyAbility] DataDriven: Row data not found. Aborting."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// Step 1a: activation-owned tags (engine :990 equivalent: AddLooseGameplayTags,
	// replication param via ShouldReplicateActivationOwnedTags() — default true = CountToOwner)
	const FGameplayTagContainer OwnedTags = GetActivationOwnedTags();
	if (!OwnedTags.IsEmpty())
	{
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			ActorInfo->AbilitySystemComponent->AddLooseGameplayTags(OwnedTags, 1,
				UAbilitySystemGlobals::Get().ShouldReplicateActivationOwnedTags()
					? EGameplayTagReplicationState::CountToOwner
					: EGameplayTagReplicationState::None);
		}
	}

	// Step 1: Block/Cancel side effects (engine :999 equivalent) — AAF custom path: engine matches CDO
	// identity, ineffective for dynamic rows; pass row identity explicitly as this ability's marker.
	// Config in Tags fragment (ForEachHook aggregation, multi-fragment stackable);
	// Block side is counter-based (BlockedAbilityTags +1), symmetric recycle in EndAbility.
	FGameplayTagContainer BlockTags, CancelTags;
	if (UAssemblyAbilitySource* BlockSource = GetAssemblySource())
	{
		ForEachHook<FAssemblyFragment_Tags>(BlockSource,
			[&](const FAssemblyFragment_Tags& Fragment)
			{
				Fragment.CollectBlockAbilitiesWithTag(BlockTags);
				Fragment.CollectCancelAbilitiesWithTag(CancelTags);
			});
	}
	if (!BlockTags.IsEmpty() || !CancelTags.IsEmpty())
	{
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			ActorInfo->AbilitySystemComponent->ApplyAbilityBlockAndCancelTags(
				GetAssetTags(), this,
				/*bEnableBlockTags=*/!BlockTags.IsEmpty(), BlockTags,
				/*bExecuteCancelTags=*/!CancelTags.IsEmpty(), CancelTags);
		}
	}

	// Step 2: execute strategy (Trigger fragment payload natively delivered by engine TriggerAbilityFromGameplayEvent)
	FAssemblyExecutionBase* Exec = ResolveExecutionData();
	if (Exec)
	{
		Exec->Execute(*this, ActorInfo, ActivationInfo, TriggerEventData);
	}
	else
	{
		UE_LOG(LogAssemblyAbility, Warning,
			TEXT("[AssemblyAbility] DataDriven: No execution strategy. Ability '%s' only committed."),
			*Row->AbilityTag.ToString());
	}

	EndAbility(Handle, ActorInfo, ActivationInfo, false, false);
}

// ── Lifecycle hook forwarding ───────────────────────
// Fragments dispatched by narrow-base IsChildOf (ForEachHook); DataDriven only forwards, no behavior.

void UGA_AssemblyDataDriven::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);
	if (UAssemblyAbilitySource* Source = GetAssemblySource())
	{
		ForEachHook<FAssemblyFragmentOnGiveAbilityBase>(Source,
			[&](const FAssemblyFragmentOnGiveAbilityBase& Hook)
			{
				Hook.OnGiveAbility(*ActorInfo, Spec);
			});
	}
}

void UGA_AssemblyDataDriven::OnRemoveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	if (UAssemblyAbilitySource* Source = GetAssemblySource())
	{
		ForEachHook<FAssemblyFragmentOnRemoveAbilityBase>(Source,
			[&](const FAssemblyFragmentOnRemoveAbilityBase& Hook)
			{
				Hook.OnRemoveAbility(*ActorInfo, Spec);
			});
	}
	Super::OnRemoveAbility(ActorInfo, Spec);
}

void UGA_AssemblyDataDriven::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnAvatarSet(ActorInfo, Spec);
	if (UAssemblyAbilitySource* Source = GetAssemblySource())
	{
		ForEachHook<FAssemblyFragmentOnAvatarSetBase>(Source,
			[&](const FAssemblyFragmentOnAvatarSetBase& Hook)
			{
				Hook.OnAvatarSet(*ActorInfo, Spec);
			});
	}
}

void UGA_AssemblyDataDriven::PreActivate(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	FOnGameplayAbilityEnded::FDelegate* OnGameplayAbilityEndedDelegate,
	const FGameplayEventData* TriggerEventData)
{
	Super::PreActivate(Handle, ActorInfo, ActivationInfo, OnGameplayAbilityEndedDelegate, TriggerEventData);
	if (UAssemblyAbilitySource* Source = GetAssemblySource())
	{
		ForEachHook<FAssemblyFragmentOnPreActivateBase>(Source,
			[&](const FAssemblyFragmentOnPreActivateBase& Hook)
			{
				Hook.OnPreActivate(*this, *ActorInfo);
			});
	}
}

void UGA_AssemblyDataDriven::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Block side symmetric recycle (engine :888 equivalent): activation +1, here -1.
	// Unpaired leak would permanently block other abilities. Config in Tags fragment
	// (ForEachHook aggregation, same container as apply side for counter pairing).
	FGameplayTagContainer BlockTags;
	if (UAssemblyAbilitySource* BlockSource = GetAssemblySource())
	{
		ForEachHook<FAssemblyFragment_Tags>(BlockSource,
			[&](const FAssemblyFragment_Tags& Fragment)
			{
				Fragment.CollectBlockAbilitiesWithTag(BlockTags);
			});
	}
	if (!BlockTags.IsEmpty())
	{
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			ActorInfo->AbilitySystemComponent->ApplyAbilityBlockAndCancelTags(
				GetAssetTags(), this,
				/*bEnableBlockTags=*/false, BlockTags,
				/*bExecuteCancelTags=*/false, FGameplayTagContainer());
		}
	}

	// Activation-owned tags symmetric recycle (engine :870 equivalent: RemoveLooseGameplayTags)
	const FGameplayTagContainer OwnedTags = GetActivationOwnedTags();
	if (!OwnedTags.IsEmpty())
	{
		if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
		{
			ActorInfo->AbilitySystemComponent->RemoveLooseGameplayTags(OwnedTags, 1,
				UAbilitySystemGlobals::Get().ShouldReplicateActivationOwnedTags()
					? EGameplayTagReplicationState::CountToOwner
					: EGameplayTagReplicationState::None);
		}
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
	if (UAssemblyAbilitySource* Source = GetAssemblySource())
	{
		ForEachHook<FAssemblyFragmentOnEndAbilityBase>(Source,
			[&](const FAssemblyFragmentOnEndAbilityBase& Hook)
			{
				Hook.OnEndAbility(*this, bWasCancelled);
			});
	}
}
