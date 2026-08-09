#include "ViewModels/SettingViewModelBase.h"

void USettingViewModelBase::Initialize(USettingEntry* InEntry, UObject* InHost)
{
	Entry = InEntry;
	Host = InHost;

	if (Entry)
	{
		SetDisplayName(Entry->DisplayName);
		SetDescription(Entry->Description);
	}

	if (Host && Entry)
	{
		const FString& Path = Entry->BindingPath.Path;
		if (!Path.IsEmpty())
		{
			PropertyPath = FCachedPropertyPath(Path);
			PropertyPath.Resolve(Host);
		}
	}
}

void USettingViewModelBase::StoreInitial()
{
	GetValueFromHost();
	SetDirty(false);
}

bool USettingViewModelBase::GetHostValueAsString(FString& OutValue) const
{
	if (!Host || !PropertyPath.IsValid()) return false;
	return PropertyPathHelpers::GetPropertyValueAsString(Host, PropertyPath, OutValue);
}

bool USettingViewModelBase::SetHostValueFromString(const FString& ValueString)
{
	if (!Host || !PropertyPath.IsValid()) return false;
	const bool bSuccess = PropertyPathHelpers::SetPropertyValueFromString(Host, PropertyPath, ValueString);
	if (bSuccess)
	{
		SetDirty(true);
	}
	return bSuccess;
}

void USettingViewModelBase::ResetToDefault()
{
}

void USettingViewModelBase::RestoreToInitial()
{
}

void USettingViewModelBase::AddEditCondition(const TSharedRef<FSettingEditCondition>& InCondition)
{
	EditConditions.Add(InCondition);
}

void USettingViewModelBase::RefreshEditableState(FGameplayTagContainer Traits)
{
	FSettingEditableState State;

	if (Entry && !Entry->PlatformTraits.IsEmpty())
	{
		if (!Traits.HasAny(Entry->PlatformTraits))
		{
			State.Hide();
		}
	}

	if (Entry && Entry->EditConditionTag.IsValid() && !Traits.HasTag(Entry->EditConditionTag))
	{
		State.Disable();
	}

	for (const TSharedRef<FSettingEditCondition>& Condition : EditConditions)
	{
		Condition->GatherEditState(State);
	}

	int32 NewFlags = 0;
	if (State.bVisible) NewFlags |= static_cast<int32>(ESettingEditableState::Visible);
	if (State.bEnabled) NewFlags |= static_cast<int32>(ESettingEditableState::Enabled);
	if (State.bResetable) NewFlags |= static_cast<int32>(ESettingEditableState::Resetable);
	SetEditableStateFlags(NewFlags);
}

void USettingViewModelBase::SetValueOnHost(const FString& ValueString)
{
	if (!Host || !Entry) return;

	SetHostValueFromString(ValueString);
	GetValueFromHost();
}

void USettingViewModelBase::GetValueFromHost()
{
}

void USettingViewModelBase::SetDisplayName(const FText& InText)
{
	UE_MVVM_SET_PROPERTY_VALUE(DisplayName, InText);
}

void USettingViewModelBase::SetDescription(const FText& InText)
{
	UE_MVVM_SET_PROPERTY_VALUE(Description, InText);
}

void USettingViewModelBase::SetCurrentDisplayValue(const FText& InText)
{
	UE_MVVM_SET_PROPERTY_VALUE(CurrentDisplayValue, InText);
}

void USettingViewModelBase::SetDirty(bool bInDirty)
{
	UE_MVVM_SET_PROPERTY_VALUE(bIsDirty, bInDirty);
}

void USettingViewModelBase::SetEditableStateFlags(int32 InFlags)
{
	UE_MVVM_SET_PROPERTY_VALUE(EditableStateFlags, InFlags);
}

void USettingViewModelBase::SetSelectable(bool bInSelectable)
{
	UE_MVVM_SET_PROPERTY_VALUE(bSelectable, bInSelectable);
}
