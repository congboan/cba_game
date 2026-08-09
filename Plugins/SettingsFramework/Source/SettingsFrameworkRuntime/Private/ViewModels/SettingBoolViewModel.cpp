#include "ViewModels/SettingBoolViewModel.h"

void USettingBoolViewModel::SetValue(bool bInValue)
{
	bCurrentValue = bInValue;
	SetValueOnHost(bCurrentValue ? TEXT("true") : TEXT("false"));
	SetCurrentDisplayValue(bCurrentValue
		? NSLOCTEXT("Settings", "On", "On")
		: NSLOCTEXT("Settings", "Off", "Off"));
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetValue);
}

void USettingBoolViewModel::StoreInitial()
{
	GetValueFromHost();
	bInitialValue = bCurrentValue;
	Super::StoreInitial();
}

void USettingBoolViewModel::GetValueFromHost()
{
	FString ValueStr;
	if (GetHostValueAsString(ValueStr))
	{
		bCurrentValue = ValueStr == TEXT("true") || ValueStr == TEXT("1");
	}
}

void USettingBoolViewModel::ResetToDefault()
{
	bool Default = false;
	if (Entry && !Entry->DefaultValue.IsEmpty())
	{
		Default = Entry->DefaultValue == TEXT("true") || Entry->DefaultValue == TEXT("1");
	}
	SetValue(Default);
}

void USettingBoolViewModel::RestoreToInitial()
{
	SetValue(bInitialValue);
}
