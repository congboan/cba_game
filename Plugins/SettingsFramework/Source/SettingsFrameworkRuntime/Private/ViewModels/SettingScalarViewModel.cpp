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
	SetValue(DefaultValue);
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
	if (Host && ResolvedProperty)
	{
		void* ValPtr = ResolvedProperty->ContainerPtrToValuePtr<void>(Host);
		if (const FFloatProperty* FP = CastField<FFloatProperty>(ResolvedProperty))
		{
			CurrentValue = FP->GetFloatingPointPropertyValue(ValPtr);
		}
	}
}
