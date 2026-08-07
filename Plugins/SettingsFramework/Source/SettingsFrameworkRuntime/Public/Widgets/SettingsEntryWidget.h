#pragma once

#include "CoreMinimal.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "ViewModel/MVVMFieldNotificationDelegates.h"
#include "ViewModels/SettingViewModelBase.h"
#include "SettingsEntryWidget.generated.h"

UCLASS(BlueprintType, Blueprintable)
class SETTINGSFRAMEWORKRUNTIME_API USettingsEntryWidget : public UUserWidget, public IUserObjectListEntry
{
	GENERATED_BODY()

public:
	virtual void NativeOnListItemObjectSet(UObject* ListItemObject) override;
	virtual void NativeOnEntryReleased() override;

protected:
	void OnDisplayValueChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId);
	void OnEditableStateChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId);

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> ValueText;

	UPROPERTY()
	TObjectPtr<USettingViewModelBase> BoundVM;

	FDelegateHandle DisplayValueHandle;
	FDelegateHandle EditableStateHandle;
};
