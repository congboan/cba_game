#pragma once

#include "ViewModels/SettingViewModelBase.h"
#include "SettingScalarViewModel.generated.h"

UCLASS(BlueprintType, Blueprintable)
class SETTINGSFRAMEWORKRUNTIME_API USettingScalarViewModel : public USettingViewModelBase
{
	GENERATED_BODY()

public:
	virtual void Initialize(USettingEntry* InEntry, UObject* InHost) override;
	virtual void StoreInitial() override;
	virtual void ResetToDefault() override;
	virtual void RestoreToInitial() override;
	virtual void GetValueFromHost() override;

	UFUNCTION(BlueprintCallable)
	void SetValue(float InValue);

	UFUNCTION(BlueprintPure, FieldNotify)
	float GetValue() const { return CurrentValue; }

	UFUNCTION(BlueprintPure, FieldNotify)
	float GetMin() const { return MinValue; }

	UFUNCTION(BlueprintPure, FieldNotify)
	float GetMax() const { return MaxValue; }

	UFUNCTION(BlueprintPure, FieldNotify)
	float GetStep() const { return StepValue; }

protected:
	float InitialValue = 0.0f;
	float CurrentValue = 0.0f;
	float DefaultValue = 0.0f;
	float MinValue = 0.0f;
	float MaxValue = 100.0f;
	float StepValue = 1.0f;
};
