#include "AbilityData/Validation/UAssemblyAbilityRowValidator.h"
#include "AbilityData/FAssemblyAbilityRow.h"
#include "AbilityData/Execution/FAssemblyExecutionBase.h"
#include "GameplayEffect.h"

bool UAssemblyAbilityRowValidator::ValidateRow(const FAssemblyAbilityRow& Row, FName RowName, TArray<FString>& OutErrors)
{
	bool bValid = true;

	// 1. AbilityTag must be valid
	if (!Row.AbilityTag.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("[%s] AbilityTag invalid (empty). Each row needs a valid tag as its DataRegistry key."), *RowName.ToString()));
		bValid = false;
	}

	// 2. Execution strategy check
	if (!Row.ExecutionStrategy.IsValid())
	{
		// No execution strategy — valid for pure passive/modifier abilities, warning only
	}
	else
	{
		const UScriptStruct* ExecType = Row.ExecutionStrategy.GetScriptStruct();
		if (!ExecType || !ExecType->IsChildOf(FAssemblyExecutionBase::StaticStruct()))
		{
			OutErrors.Add(FString::Printf(TEXT("[%s] ExecutionStrategy type invalid '%s'. Must be a FAssemblyExecutionBase subclass."),
				*RowName.ToString(), ExecType ? *ExecType->GetName() : TEXT("null")));
			bValid = false;
		}
	}

	// 3. Legacy cooldown override check retired: settlement lives in Row.Fragments
	if (false)
	{
		OutErrors.Add(FString::Printf(TEXT("[%s] CooldownOverrideTags configured but CooldownGE empty."),
			*RowName.ToString()));
		bValid = false;
	}

	// 4. Override entries must have valid tag + strategy
	for (int32 Idx = 0; Idx < Row.ExecutionOverrides.Num(); ++Idx)
	{
		const FNamedExecutionOverride& Pair = Row.ExecutionOverrides[Idx];
		const FGameplayTag& Tag = Pair.HolderTag;
		const auto& OverrideExec = Pair.Strategy;

		if (!Tag.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("[%s] ExecutionOverrides contains invalid override tag."), *RowName.ToString()));
			bValid = false;
		}

		if (!OverrideExec.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("[%s] ExecutionOverrides tag '%s' has empty strategy."),
				*RowName.ToString(), *Tag.ToString()));
			bValid = false;
		}
	}

	// 5. Fragment instance validity
	for (int32 FragmentIndex = 0; FragmentIndex < Row.Fragments.Num(); ++FragmentIndex)
	{
		if (!Row.Fragments[FragmentIndex].IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("[%s] Fragments[%d] invalid instance."),
				*RowName.ToString(), FragmentIndex));
			bValid = false;
		}
	}

	return bValid;
}
