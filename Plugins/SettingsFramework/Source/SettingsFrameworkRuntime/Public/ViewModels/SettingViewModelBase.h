#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Data/SettingEntry.h"
#include "SettingViewModelBase.generated.h"

UCLASS(BlueprintType, Blueprintable)
class SETTINGSFRAMEWORKRUNTIME_API USettingViewModelBase : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	virtual void Initialize(USettingEntry* InEntry, UObject* InHost);

	UFUNCTION(BlueprintCallable)
	virtual void StoreInitial();

	UFUNCTION(BlueprintCallable)
	virtual void ResetToDefault();

	UFUNCTION(BlueprintCallable)
	virtual void RestoreToInitial();

	UFUNCTION(BlueprintCallable)
	virtual void RefreshEditableState(FGameplayTagContainer Traits);

	void SetDisplayName(const FText& InText);
	FText GetDisplayName() const { return DisplayName; }

	void SetDescription(const FText& InText);
	FText GetDescription() const { return Description; }

	void SetCurrentDisplayValue(const FText& InText);
	FText GetCurrentDisplayValue() const { return CurrentDisplayValue; }

	void SetDirty(bool bInDirty);
	bool IsDirty() const { return bIsDirty; }

	void SetEditableStateFlags(int32 InFlags);
	int32 GetEditableStateFlags() const { return EditableStateFlags; }

	USettingEntry* GetEntry() const { return Entry; }
	UObject* GetHost() const { return Host; }

protected:
	void SetValueOnHost(const FString& ValueString);
	virtual void GetValueFromHost();

	UPROPERTY()
	TObjectPtr<USettingEntry> Entry;

	UPROPERTY()
	TObjectPtr<UObject> Host;

	FProperty* ResolvedProperty = nullptr;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter, Getter, meta=(AllowPrivateAccess))
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter, Getter, meta=(AllowPrivateAccess))
	FText Description;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter, Getter, meta=(AllowPrivateAccess))
	FText CurrentDisplayValue;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetDirty, Getter = IsDirty, meta=(AllowPrivateAccess))
	bool bIsDirty = false;

	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter, Getter, meta=(AllowPrivateAccess))
	int32 EditableStateFlags = 0;
};
