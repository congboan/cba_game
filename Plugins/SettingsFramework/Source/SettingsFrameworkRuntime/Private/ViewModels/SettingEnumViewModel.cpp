#include "ViewModels/SettingEnumViewModel.h"

void USettingEnumViewModel::Initialize(USettingEntry* InEntry, UObject* InHost)
{
	Super::Initialize(InEntry, InHost);
	if (Entry) { Options = Entry->Options; }
	StoreInitial();
}

void USettingEnumViewModel::SetValue(int32 InIndex)
{
	if (!Options.IsValidIndex(InIndex)) return;
	CurrentIndex = InIndex;
	SetValueOnHost(Options[InIndex].Value);
	SetCurrentDisplayValue(Options[InIndex].Label);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCurrentIndex);
}

void USettingEnumViewModel::StoreInitial()
{
	if (Host && ResolvedProperty)
	{
		void* ValPtr = ResolvedProperty->ContainerPtrToValuePtr<void>(Host);
		FString CurrentStr;
		ResolvedProperty->ExportTextItem_Direct(CurrentStr, ValPtr, nullptr, Host, PPF_None);
		for (int32 i = 0; i < Options.Num(); ++i)
		{
			if (Options[i].Value == CurrentStr) { CurrentIndex = i; break; }
		}
	}
	InitialIndex = CurrentIndex;
	Super::StoreInitial();
}

void USettingEnumViewModel::ResetToDefault()
{
	if (Options.Num() > 0) SetValue(0);
}

void USettingEnumViewModel::RestoreToInitial()
{
	SetValue(InitialIndex);
}
