#include "AbilityData/Fragment/FAssemblyFragment_Cooldown.h"
#include "AbilitySystem/UGA_AssemblyDataDriven.h"
#include "Assembly/UAssemblyAbilitySource.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "AssemblyAbilityFrameworkLog.h"

void FAssemblyFragment_Cooldown::Precache(UAssemblyAbilitySource* Owner)
{
	if (!CooldownGE.IsNull())
	{
		CachedGE = CooldownGE.LoadSynchronous();
		if (!CachedGE && Owner)
		{
			UE_LOG(LogAssemblyAbility, Error,
				TEXT("[CooldownFragment] Grant preload failed for '%s'."),
				*Owner->CompiledRow.Row.AbilityTag.ToString());
		}
	}
}

bool FAssemblyFragment_Cooldown::Check(
	const UGA_AssemblyDataDriven& GA,
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
	{
		UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
		if (!CooldownTags.IsEmpty() && ASC->HasAnyMatchingGameplayTags(CooldownTags))
		{
			UE_LOG(LogAssemblyAbility, Verbose,
				TEXT("[AA] '%s' blocked by cooldown fragment."),
				*GA.GetOriginAbilityTag().ToString());
			return false;
		}
	}
	return true;
}

void FAssemblyFragment_Cooldown::Apply(
	const UGA_AssemblyDataDriven& GA,
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo& ActivationInfo) const
{
	if (!CachedGE || !ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();

	const FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(CachedGE->GetClass(), GA.GetAbilityLevel(), Context);
	if (!Spec.IsValid())
	{
		return;
	}

	Spec.Data->DynamicGrantedTags.AppendTags(CooldownTags);

	for (const FAssemblyNamedValue& Override : DurationOverrides)
	{
		if (Override.Key.IsValid() && ASC->HasMatchingGameplayTag(Override.Key))
		{
			Spec.Data->SetSetByCallerMagnitude(Override.Key, Override.Value);
			UE_LOG(LogAssemblyAbility, Verbose, TEXT("[AA] cd override %s -> %.2fs"),
				*Override.Key.ToString(), Override.Value);
		}
	}

	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
}
