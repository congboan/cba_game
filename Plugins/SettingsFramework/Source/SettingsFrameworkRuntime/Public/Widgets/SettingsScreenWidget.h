#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Registry/SettingRegistry.h"
#include "SettingsScreenWidget.generated.h"

class USettingsListView;
class USettingRegistry;

UCLASS(BlueprintType, Blueprintable)
class SETTINGSFRAMEWORKRUNTIME_API USettingsScreenWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetupRegistry(USettingRegistry* InRegistry);

	UFUNCTION(BlueprintCallable)
	void NavigateToPage(USettingViewModelBase* PageVM);

	UFUNCTION(BlueprintCallable)
	void NavigateBack();

	UFUNCTION(BlueprintCallable)
	void SaveAndClose();

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	USettingsListView* SettingsList;

protected:
	UPROPERTY()
	USettingRegistry* Registry;

	void RefreshDisplay();
};
