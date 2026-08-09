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
	FString CurrentStr;
	if (GetHostValueAsString(CurrentStr))
	{
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
	int32 DefaultIndex = 0;
	if (Entry && !Entry->DefaultValue.IsEmpty())
	{
		for (int32 i = 0; i < Options.Num(); ++i)
		{
			if (Options[i].Value == Entry->DefaultValue) { DefaultIndex = i; break; }
		}
	}
	SetValue(DefaultIndex);
}

void USettingEnumViewModel::RestoreToInitial()
{
	SetValue(InitialIndex);
}

void USettingEnumViewModel::SetDiscreteOptionByIndex(int32 Index)
{
	SetValue(Index);
}

int32 USettingEnumViewModel::GetDiscreteOptionIndex() const
{
	return CurrentIndex;
}

TArray<FText> USettingEnumViewModel::GetDiscreteOptions() const
{
	TArray<FText> Labels;
	for (const FSettingOption& Opt : Options)
	{
		Labels.Add(Opt.Label);
	}
	return Labels;
}

FString USettingEnumViewModel::GetDiscreteOptionValueAt(int32 Index) const
{
	return Options.IsValidIndex(Index) ? Options[Index].Value : FString();
}
