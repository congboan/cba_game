#include "ViewModels/SettingValueDiscrete.h"

void USettingValueDiscrete::SetDiscreteOptionByIndex(int32 Index)
{
	OnSelectedOptionChanged(Index);
}

int32 USettingValueDiscrete::GetDiscreteOptionIndex() const
{
	return INDEX_NONE;
}

TArray<FText> USettingValueDiscrete::GetDiscreteOptions() const
{
	return TArray<FText>();
}

FString USettingValueDiscrete::GetDiscreteOptionValueAt(int32 Index) const
{
	return FString();
}

void USettingValueDiscrete::OnSelectedOptionChanged(int32 NewIndex)
{
}
