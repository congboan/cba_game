#pragma once

#include "ViewModels/SettingViewModelBase.h"
#include "SettingActionViewModel.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSettingActionExecuted);

UCLASS(BlueprintType, Blueprintable)
class SETTINGSFRAMEWORKRUNTIME_API USettingActionViewModel : public USettingViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void ExecuteAction();

	UPROPERTY(BlueprintAssignable)
	FOnSettingActionExecuted OnActionExecuted;
};
