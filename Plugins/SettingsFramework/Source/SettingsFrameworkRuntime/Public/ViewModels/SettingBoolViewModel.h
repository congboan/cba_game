#pragma once

#include "ViewModels/SettingViewModelBase.h"
#include "SettingBoolViewModel.generated.h"

UCLASS(BlueprintType, Blueprintable)
class SETTINGSFRAMEWORKRUNTIME_API USettingBoolViewModel : public USettingViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetValue(bool bInValue);

	UFUNCTION(BlueprintPure, FieldNotify)
	bool GetValue() const { return bCurrentValue; }

	virtual void StoreInitial() override;
	virtual void ResetToDefault() override;
	virtual void RestoreToInitial() override;
	virtual void GetValueFromHost() override;

protected:
	bool bInitialValue = false;
	bool bCurrentValue = false;
};
