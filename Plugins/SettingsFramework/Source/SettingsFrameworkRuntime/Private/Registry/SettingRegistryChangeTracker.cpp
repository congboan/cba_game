#include "Registry/SettingRegistryChangeTracker.h"
#include "Registry/SettingRegistry.h"
#include "ViewModels/SettingViewModelBase.h"

void FSettingRegistryChangeTracker::WatchRegistry(USettingRegistry* InRegistry)
{
	Registry = InRegistry;
	RebuildDirtyState();
}

void FSettingRegistryChangeTracker::StopWatchingRegistry()
{
	Registry = nullptr;
	DirtyViewModels.Reset();
	bSettingsChanged = false;
}

void FSettingRegistryChangeTracker::RebuildDirtyState()
{
	DirtyViewModels.Reset();
	bSettingsChanged = false;

	USettingRegistry* Reg = Registry.Get();
	if (!Reg) return;

	for (USettingViewModelBase* VM : Reg->GetAllViewModels())
	{
		if (VM && VM->IsDirty())
		{
			DirtyViewModels.Add(VM);
			bSettingsChanged = true;
		}
	}
}

void FSettingRegistryChangeTracker::ApplyChanges()
{
	for (TWeakObjectPtr<USettingViewModelBase> WeakVM : DirtyViewModels)
	{
		if (USettingViewModelBase* VM = WeakVM.Get())
		{
			VM->StoreInitial();
		}
	}
	ClearDirtyState();
}

void FSettingRegistryChangeTracker::RestoreToInitial()
{
	for (TWeakObjectPtr<USettingViewModelBase> WeakVM : DirtyViewModels)
	{
		if (USettingViewModelBase* VM = WeakVM.Get())
		{
			VM->RestoreToInitial();
		}
	}
	ClearDirtyState();
}

void FSettingRegistryChangeTracker::ClearDirtyState()
{
	DirtyViewModels.Reset();
	bSettingsChanged = false;
}
