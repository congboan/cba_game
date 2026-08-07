#pragma once

#include "ViewModels/SettingViewModelBase.h"
#include "SettingEnumViewModel.generated.h"

UCLASS(BlueprintType, Blueprintable)
class SETTINGSFRAMEWORKRUNTIME_API USettingEnumViewModel : public USettingViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetValue(int32 InIndex);

	UFUNCTION(BlueprintPure, FieldNotify)
	int32 GetCurrentIndex() const { return CurrentIndex; }

	UFUNCTION(BlueprintPure, FieldNotify)
	TArray<FSettingOption> GetOptions() const { return Options; }

	virtual void Initialize(USettingEntry* InEntry, UObject* InHost) override;
	virtual void StoreInitial() override;
	virtual void ResetToDefault() override;
	virtual void RestoreToInitial() override;

protected:
	TArray<FSettingOption> Options;
	int32 InitialIndex = 0;
	int32 CurrentIndex = 0;
};
