#pragma once

#include "CoreMinimal.h"
#include "Components/ListView.h"
#include "SettingsListView.generated.h"

class USettingViewModelBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSettingEntryActivated, USettingViewModelBase*, Entry);

UCLASS(BlueprintType, Blueprintable)
class SETTINGSFRAMEWORKRUNTIME_API USettingsListView : public UListView
{
	GENERATED_BODY()

public:
	void SetItems(const TArray<USettingViewModelBase*>& Items);

	/** 仅在点击可点选条目（Page）时广播；分组（Group）点击不响应。 */
	UPROPERTY(BlueprintAssignable, Category = "Settings")
	FOnSettingEntryActivated OnEntryActivated;

protected:
	virtual void OnItemClickedInternal(UObject* Item) override;
};
