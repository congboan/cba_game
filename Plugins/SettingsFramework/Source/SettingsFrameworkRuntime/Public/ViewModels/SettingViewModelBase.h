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

	/** 注册依赖 VM：其值变化时刷新本项可编辑状态（对应 Lyra AddEditDependency）。 */
	void AddEditDependency(USettingViewModelBase* DependencyVM);

	/** 依赖项值变化回调：用缓存 Traits 重算可编辑状态。 */
	void OnDependencyValueChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId);

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

	/** 已注册的依赖 VM（弱引用，生命周期由 Registry 管理）。 */
	TArray<TWeakObjectPtr<USettingViewModelBase>> EditDependencies;

	/** 最近一次 RefreshEditableState 的 Traits（依赖值变化时重算用）。 */
	FGameplayTagContainer CachedTraits;

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
