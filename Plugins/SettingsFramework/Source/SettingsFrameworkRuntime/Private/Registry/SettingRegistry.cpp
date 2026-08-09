#include "Registry/SettingRegistry.h"
#include "ViewModels/SettingViewModelBase.h"
#include "ViewModels/SettingScalarViewModel.h"
#include "ViewModels/SettingBoolViewModel.h"
#include "ViewModels/SettingEnumViewModel.h"
#include "ViewModels/SettingActionViewModel.h"
#include "ViewModels/SettingPageViewModel.h"
#include "ViewModels/SettingValueDiscrete.h"

void USettingRegistry::RegisterViewModelClass(ESettingValueType ValueType, TSubclassOf<USettingViewModelBase> ViewModelClass)
{
	if (ViewModelClass)
	{
		RegisteredViewModelClasses.Add(ValueType, ViewModelClass);
	}
}

TSubclassOf<USettingViewModelBase> USettingRegistry::GetViewModelClassForType(ESettingValueType ValueType) const
{
	const TSubclassOf<USettingViewModelBase>* Found = RegisteredViewModelClasses.Find(ValueType);
	return Found ? *Found : nullptr;
}

USettingViewModelBase* USettingRegistry::CreateViewModelForEntry(USettingEntry* Entry, UObject* InHost)
{
	if (!Entry) return nullptr;

	USettingViewModelBase* VM = nullptr;
	if (TSubclassOf<USettingViewModelBase> CustomClass = GetViewModelClassForType(Entry->ValueType))
	{
		VM = NewObject<USettingViewModelBase>(this, CustomClass);
	}
	else
	{
		switch (Entry->ValueType)
		{
		case ESettingValueType::Scalar: VM = NewObject<USettingScalarViewModel>(this); break;
		case ESettingValueType::Bool: VM = NewObject<USettingBoolViewModel>(this); break;
		case ESettingValueType::Enum: VM = NewObject<USettingEnumViewModel>(this); break;
		case ESettingValueType::Action: VM = NewObject<USettingActionViewModel>(this); break;
		case ESettingValueType::Page: VM = NewObject<USettingPageViewModel>(this); break;
		case ESettingValueType::Group: VM = NewObject<USettingPageViewModel>(this); break;
		default: return nullptr;
		}
	}

	if (VM)
	{
		VM->Initialize(Entry, InHost);
		VM->SetSelectable(Entry->ValueType == ESettingValueType::Page);
	}
	return VM;
}

UObject* USettingRegistry::ResolveHost(UClass* InHostClass, UObject* InHost) const
{
	if (InHost) return InHost;
	if (!InHostClass) return nullptr;

	// 反射查找静态 Get()
	UFunction* GetFn = InHostClass->FindFunctionByName(TEXT("Get"));
	if (GetFn && GetFn->HasAnyFunctionFlags(FUNC_Static) && GetFn->NumParms == 1)
	{
		UObject* Instance = nullptr;
		InHostClass->ProcessEvent(GetFn, &Instance);
		if (Instance) return Instance;
	}
	return nullptr;
}

void USettingRegistry::LoadCollection(USettingCollection* Collection, UObject* InHost)
{
	if (!Collection) return;
	UObject* Host = ResolveHost(Collection->HostClass, InHost);
	if (!Host) return;
	LoadCollectionInto(Collection, Host, nullptr);
}

void USettingRegistry::LoadCollectionInto(USettingCollection* Collection, UObject* InHost,
	USettingPageViewModel* ParentPage)
{
	HostObject = InHost;
	LoadedCollections.AddUnique(Collection);

	// 改动3：Collection 资产本身生成 Group VM 节点（用 CollectionName 作标题，不可点选）
	USettingPageViewModel* CollectionVM = NewObject<USettingPageViewModel>(this);
	CollectionVM->SetDisplayName(Collection->CollectionName);
	CollectionVM->SetSelectable(false);
	AllViewModels.Add(CollectionVM);

	if (ParentPage) ParentPage->ChildViewModels.Add(CollectionVM);
	else RootViewModels.Add(CollectionVM);

	for (USettingEntry* Entry : Collection->Entries)
	{
		USettingViewModelBase* VM = CreateViewModelForEntry(Entry, InHost);
		if (!VM) continue;

		AllViewModels.Add(VM);
		CollectionVM->ChildViewModels.Add(VM);

		USettingPageViewModel* PageVM = Cast<USettingPageViewModel>(VM);
		if (PageVM)
		{
			for (USettingCollection* ChildPage : Entry->ChildPages)
			{
				if (ChildPage) LoadCollectionInto(ChildPage, InHost, PageVM);
			}
		}
	}

	for (USettingCollection* ChildPage : Collection->ChildPages)
	{
		if (ChildPage) LoadCollectionInto(ChildPage, InHost, CollectionVM);
	}

	if (!ParentPage)
	{
		CurrentPage = nullptr;
	}
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
