#include "ViewModels/SettingScalarViewModel.h"

void USettingScalarViewModel::Initialize(USettingEntry* InEntry, UObject* InHost)
{
	Super::Initialize(InEntry, InHost);
	if (Entry)
	{
		MinValue = Entry->ScalarRange.Min;
		MaxValue = Entry->ScalarRange.Max;
		StepValue = Entry->ScalarRange.Step;
	}
	StoreInitial();
}

void USettingScalarViewModel::StoreInitial()
{
	GetValueFromHost();
	InitialValue = CurrentValue;
	Super::StoreInitial();
}

void USettingScalarViewModel::ResetToDefault()
{
	float Default = DefaultValue;
	if (Entry && !Entry->DefaultValue.IsEmpty())
	{
		Default = FCString::Atof(*Entry->DefaultValue);
	}
	SetValue(Default);
}

void USettingScalarViewModel::RestoreToInitial()
{
	SetValue(InitialValue);
}

void USettingScalarViewModel::SetValue(float InValue)
{
	InValue = FMath::Clamp(InValue, MinValue, MaxValue);
	if (StepValue > 0.0f)
	{
		InValue = FMath::RoundToFloat(InValue / StepValue) * StepValue;
	}
	CurrentValue = InValue;
	SetValueOnHost(FString::SanitizeFloat(CurrentValue));
	SetCurrentDisplayValue(FText::AsNumber(CurrentValue));
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetValue);
}

void USettingScalarViewModel::GetValueFromHost()
{
	FString ValueStr;
	if (GetHostValueAsString(ValueStr))
	{
		CurrentValue = FCString::Atof(*ValueStr);
	}
}
