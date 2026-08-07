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
		const FString& Path = Entry->BindingPath;
		if (!Path.IsEmpty())
		{
			ResolvedProperty = FindFProperty<FProperty>(Host->GetClass(), *Path);
		}
	}
}

void USettingViewModelBase::StoreInitial()
{
	GetValueFromHost();
	SetDirty(false);
}

void USettingViewModelBase::ResetToDefault()
{
}

void USettingViewModelBase::RestoreToInitial()
{
}

void USettingViewModelBase::RefreshEditableState(FGameplayTagContainer Traits)
{
	int32 NewFlags = static_cast<int32>(ESettingEditableState::All);

	if (Entry && !Entry->PlatformTraits.IsEmpty())
	{
		if (!Traits.HasAny(Entry->PlatformTraits))
		{
			NewFlags &= ~static_cast<int32>(ESettingEditableState::Visible);
		}
	}

	if (Entry && Entry->EditConditionTag.IsValid() && !Traits.HasTag(Entry->EditConditionTag))
	{
		NewFlags &= ~static_cast<int32>(ESettingEditableState::Enabled);
	}

	SetEditableStateFlags(NewFlags);
}

void USettingViewModelBase::SetValueOnHost(const FString& ValueString)
{
	if (!Host || !ResolvedProperty || !Entry) return;

	FProperty* Prop = ResolvedProperty;
	void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Host);
	Prop->ImportText_Direct(*ValueString, ValuePtr, Host, PPF_None);

	SetDirty(true);
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
