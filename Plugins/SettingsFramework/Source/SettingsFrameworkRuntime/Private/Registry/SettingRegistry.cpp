#include "Registry/SettingRegistry.h"
#include "ViewModels/SettingViewModelBase.h"
#include "ViewModels/SettingScalarViewModel.h"
#include "ViewModels/SettingBoolViewModel.h"
#include "ViewModels/SettingEnumViewModel.h"
#include "ViewModels/SettingActionViewModel.h"
#include "ViewModels/SettingPageViewModel.h"

void USettingRegistry::LoadCollection(USettingCollection* Collection, UObject* InHost)
{
	if (!Collection || !InHost) return;

	HostObject = InHost;
	LoadedCollections.AddUnique(Collection);

	for (USettingEntry* Entry : Collection->Entries)
	{
		USettingViewModelBase* VM = nullptr;
		switch (Entry->ValueType)
		{
		case ESettingValueType::Scalar:
			VM = NewObject<USettingScalarViewModel>(this);
			break;
		case ESettingValueType::Bool:
			VM = NewObject<USettingBoolViewModel>(this);
			break;
		case ESettingValueType::Enum:
			VM = NewObject<USettingEnumViewModel>(this);
			break;
		case ESettingValueType::Action:
			VM = NewObject<USettingActionViewModel>(this);
			break;
		case ESettingValueType::Page:
			VM = NewObject<USettingPageViewModel>(this);
			break;
		default: continue;
		}

		VM->Initialize(Entry, InHost);
		AllViewModels.Add(VM);
		RootViewModels.Add(VM);
	}

	CurrentPage = nullptr;
}

void USettingRegistry::SaveChanges()
{
	for (USettingViewModelBase* VM : AllViewModels)
	{
		if (VM) VM->StoreInitial();
	}
}

void USettingRegistry::NavigateToPage(USettingViewModelBase* PageVM)
{
	if (CurrentPage) NavStack.Add(CurrentPage);
	CurrentPage = PageVM;
}

bool USettingRegistry::NavigateBack()
{
	if (NavStack.Num() == 0) return false;
	CurrentPage = NavStack.Pop();
	return true;
}

USettingViewModelBase* USettingRegistry::FindSettingByDevName(FName DevName) const
{
	for (auto* VM : AllViewModels)
	{
		if (VM && VM->GetEntry() && VM->GetEntry()->DevName == DevName)
			return VM;
	}
	return nullptr;
}

TArray<USettingViewModelBase*> USettingRegistry::SearchSettings(const FString& Query) const
{
	TArray<USettingViewModelBase*> Results;
	if (Query.IsEmpty()) return AllViewModels;

	for (auto* VM : AllViewModels)
	{
		if (VM && VM->GetEntry())
		{
			const FString NameStr = VM->GetEntry()->DisplayName.ToString();
			if (NameStr.Contains(Query))
				Results.Add(VM);
		}
	}
	return Results;
}

void USettingRegistry::RefreshAllTraits(FGameplayTagContainer Traits)
{
	for (auto* VM : AllViewModels)
	{
		if (VM) VM->RefreshEditableState(Traits);
	}
}

// END
