#include "Experimental/AbilityData/Validation/UAssemblyAbilityRowValidator.h"
#include "Experimental/AbilityData/FAssemblyAbilityRow.h"
#include "Experimental/AbilityData/FAssemblyExecutionBase.h"
#include "GameplayEffect.h"

bool UAssemblyAbilityRowValidator::ValidateRow(const FAssemblyAbilityRow& Row, FName RowName, TArray<FString>& OutErrors)
{
	bool bValid = true;

	// 1. AbilityTag 必须有效
	if (!Row.AbilityTag.IsValid())
	{
		OutErrors.Add(FString::Printf(TEXT("[%s] AbilityTag 无效（为空）。每行必须有一个有效标签作为其 DataRegistry 键。"), *RowName.ToString()));
		bValid = false;
	}

	// 2. 执行策略检查
	if (!Row.ExecutionStrategy.IsValid())
	{
		// 无执行策略 — 对于纯被动/修饰技能来说这是有效的，仅警告
	}
	else
	{
		// 检查执行策略类型是否合法（FAssemblyExecutionBase 子类）
		const UScriptStruct* ExecType = Row.ExecutionStrategy.GetScriptStruct();
		if (!ExecType || !ExecType->IsChildOf(FAssemblyExecutionBase::StaticStruct()))
		{
			OutErrors.Add(FString::Printf(TEXT("[%s] ExecutionStrategy 类型无效 '%s'。必须为 FAssemblyExecutionBase 的子类。"),
				*RowName.ToString(), ExecType ? *ExecType->GetName() : TEXT("null")));
			bValid = false;
		}
	}

	// 3. 冷却覆写一致性检查
	if (!Row.CooldownOverrideTags.IsEmpty() && Row.CooldownGE.IsNull())
	{
		OutErrors.Add(FString::Printf(TEXT("[%s] 配置了 CooldownOverrideTags 但 CooldownGE 为空。覆写需要 CooldownGE 的 Duration 配置为 SetByCaller。"),
			*RowName.ToString()));
		bValid = false;
	}

	// 4. 检查覆写标签是否有效
	for (const auto& [Tag, OverrideExec] : Row.ExecutionOverrides)
	{
		if (!Tag.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("[%s] ExecutionOverrides 包含无效的覆写标签。"), *RowName.ToString()));
			bValid = false;
		}

		if (!OverrideExec.IsValid())
		{
			OutErrors.Add(FString::Printf(TEXT("[%s] ExecutionOverrides 标签 '%s' 的执行策略为空。"),
				*RowName.ToString(), *Tag.ToString()));
			bValid = false;
		}
	}

	return bValid;
}
