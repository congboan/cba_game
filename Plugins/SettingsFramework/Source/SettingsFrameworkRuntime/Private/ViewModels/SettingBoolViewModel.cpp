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
	if (Host && ResolvedProperty)
	{
		void* ValPtr = ResolvedProperty->ContainerPtrToValuePtr<void>(Host);
		if (const FBoolProperty* BP = CastField<FBoolProperty>(ResolvedProperty))
		{
			bCurrentValue = BP->GetPropertyValue(ValPtr);
		}
	}
	bInitialValue = bCurrentValue;
	Super::StoreInitial();
}

void USettingBoolViewModel::ResetToDefault()
{
	SetValue(false);
}

void USettingBoolViewModel::RestoreToInitial()
{
	SetValue(bInitialValue);
}
