#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Data/SettingEntry.h"
#include "EditCondition/SettingEditCondition.h"
#include "PropertyPathHelpers.h"
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

	/** 注册可组合编辑条件（生命周期由调用方管理）。 */
	void AddEditCondition(const TSharedRef<FSettingEditCondition>& InCondition);

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

	/** 是否为可点选进入的节点（Page=true / Group=false）。 */
	void SetSelectable(bool bInSelectable);
	bool IsSelectable() const { return bSelectable; }

	USettingEntry* GetEntry() const { return Entry; }
	UObject* GetHost() const { return Host; }

protected:
	void SetValueOnHost(const FString& ValueString);
	virtual void GetValueFromHost();

	/** 从宿主读取绑定路径的字符串值；路径未解析或不可读返回 false。 */
	bool GetHostValueAsString(FString& OutValue) const;
	bool SetHostValueFromString(const FString& ValueString);

	TArray<TSharedRef<FSettingEditCondition>> EditConditions;

	UPROPERTY()
	TObjectPtr<USettingEntry> Entry;

	UPROPERTY()
	TObjectPtr<UObject> Host;

	/** 缓存 PropertyPath（支持函数链，如 GetLocalSettings.MasterVolume）。 */
	FCachedPropertyPath PropertyPath;

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

	/** 可点选进入标记：Page=true（导航到子页），Group=false（仅分组标题）。 */
	UPROPERTY(BlueprintReadOnly, FieldNotify, Setter = SetSelectable, Getter = IsSelectable, meta=(AllowPrivateAccess))
	bool bSelectable = false;
};
