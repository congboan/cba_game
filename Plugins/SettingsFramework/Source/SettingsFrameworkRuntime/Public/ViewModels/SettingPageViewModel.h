#pragma once

#include "ViewModels/SettingViewModelBase.h"
#include "SettingPageViewModel.generated.h"

UCLASS(BlueprintType, Blueprintable)
class SETTINGSFRAMEWORKRUNTIME_API USettingPageViewModel : public USettingViewModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, FieldNotify)
	TArray<USettingViewModelBase*> ChildViewModels;
};
