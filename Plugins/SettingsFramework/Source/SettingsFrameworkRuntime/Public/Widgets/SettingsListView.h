#pragma once

#include "CoreMinimal.h"
#include "Components/ListView.h"
#include "SettingsListView.generated.h"

class USettingViewModelBase;

UCLASS(BlueprintType, Blueprintable)
class SETTINGSFRAMEWORKRUNTIME_API USettingsListView : public UListView
{
	GENERATED_BODY()

public:
	void SetItems(const TArray<USettingViewModelBase*>& Items);
};
