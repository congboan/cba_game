#include "AbilityData/FAssemblyAbilityRow.h"
#include "AbilityData/Validation/UAssemblyAbilityRowValidator.h"

#if WITH_EDITOR

// ── DataTable 编辑期自动校验（引擎钩子）──────────
//
// UDataTable::IsDataValid 会逐行调用 FTableRowBase::IsDataValid。
// 行名在该签名中不可得，错误消息以 AbilityTag 定位行。

EDataValidationResult FAssemblyAbilityRow::IsDataValid(FDataValidationContext& Context) const
{
	const FString RowLabel = FString::Printf(TEXT("AbilityTag:%s"), *AbilityTag.GetTagName().ToString());

	TArray<FString> Errors;
	if (!UAssemblyAbilityRowValidator::ValidateRow(*this, FName(*RowLabel), Errors))
	{
		for (const FString& Error : Errors)
		{
			Context.AddError(FText::FromString(Error));
		}
		return EDataValidationResult::Invalid;
	}

	return EDataValidationResult::Valid;
}

#endif // WITH_EDITOR
