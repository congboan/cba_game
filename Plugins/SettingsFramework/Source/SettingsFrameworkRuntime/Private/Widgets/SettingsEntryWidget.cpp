#include "Widgets/SettingsEntryWidget.h"

void USettingsEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	IUserObjectListEntry::NativeOnListItemObjectSet(ListItemObject);

	USettingViewModelBase* VM = Cast<USettingViewModelBase>(ListItemObject);
	if (!VM) return;

	// Unsubscribe previous VM
	if (BoundVM != nullptr)
	{
		if (DisplayValueHandle.IsValid())
			BoundVM->RemoveFieldValueChangedDelegate(
				USettingViewModelBase::FFieldNotificationClassDescriptor::CurrentDisplayValue,
				DisplayValueHandle);
		if (EditableStateHandle.IsValid())
			BoundVM->RemoveFieldValueChangedDelegate(
				USettingViewModelBase::FFieldNotificationClassDescriptor::EditableStateFlags,
				EditableStateHandle);
	}

	BoundVM = VM;

	// Initial display
	if (NameText)
		NameText->SetText(VM->GetEntry() ? VM->GetEntry()->DisplayName : FText());
	if (ValueText)
		ValueText->SetText(VM->GetCurrentDisplayValue());

	// Subscribe to FieldNotify changes
	DisplayValueHandle = VM->AddFieldValueChangedDelegate(
		USettingViewModelBase::FFieldNotificationClassDescriptor::CurrentDisplayValue,
		FFieldValueChangedDelegate::CreateUObject(this, &USettingsEntryWidget::OnDisplayValueChanged));

	EditableStateHandle = VM->AddFieldValueChangedDelegate(
		USettingViewModelBase::FFieldNotificationClassDescriptor::EditableStateFlags,
		FFieldValueChangedDelegate::CreateUObject(this, &USettingsEntryWidget::OnEditableStateChanged));
}

void USettingsEntryWidget::NativeOnEntryReleased()
{
	if (BoundVM != nullptr)
	{
		if (DisplayValueHandle.IsValid())
			BoundVM->RemoveFieldValueChangedDelegate(
				USettingViewModelBase::FFieldNotificationClassDescriptor::CurrentDisplayValue,
				DisplayValueHandle);
		DisplayValueHandle.Reset();

		if (EditableStateHandle.IsValid())
			BoundVM->RemoveFieldValueChangedDelegate(
				USettingViewModelBase::FFieldNotificationClassDescriptor::EditableStateFlags,
				EditableStateHandle);
		EditableStateHandle.Reset();
	}

	BoundVM = nullptr;
	IUserObjectListEntry::NativeOnEntryReleased();
}

void USettingsEntryWidget::OnDisplayValueChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId)
{
	if (ValueText && BoundVM != nullptr)
	{
		ValueText->SetText(BoundVM->GetCurrentDisplayValue());
	}
}

void USettingsEntryWidget::OnEditableStateChanged(UObject* Object, UE::FieldNotification::FFieldId FieldId)
{
	if (!BoundVM) return;
	const int32 Flags = BoundVM->GetEditableStateFlags();
	const bool bVisible = (Flags & static_cast<int32>(ESettingEditableState::Visible)) != 0;
	const bool bEnabled = (Flags & static_cast<int32>(ESettingEditableState::Enabled)) != 0;
	SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	SetIsEnabled(bEnabled);
}
