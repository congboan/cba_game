#pragma once

#include "ViewModels/SettingValueDiscrete.h"
#include "SettingEnumViewModel.generated.h"

UCLASS(BlueprintType, Blueprintable)
class SETTINGSFRAMEWORKRUNTIME_API USettingEnumViewModel : public USettingValueDiscrete
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
	virtual void SetDiscreteOptionByIndex(int32 Index) override;
	virtual int32 GetDiscreteOptionIndex() const override;
	virtual TArray<FText> GetDiscreteOptions() const override;
	virtual FString GetDiscreteOptionValueAt(int32 Index) const override;

protected:
	TArray<FSettingOption> Options;
	int32 InitialIndex = 0;
	int32 CurrentIndex = 0;
};
