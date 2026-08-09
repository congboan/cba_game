#pragma once

#include "CoreMinimal.h"

class USettingRegistry;
class USettingViewModelBase;

/** 注册表脏追踪器（对应 Lyra FGameSettingRegistryChangeTracker）。
 *  监听 VM 脏状态，支持统一 Apply / RestoreToInitial / ClearDirty。 */
class SETTINGSFRAMEWORKRUNTIME_API FSettingRegistryChangeTracker
{
public:
	FSettingRegistryChangeTracker() = default;

	void WatchRegistry(USettingRegistry* InRegistry);
	void StopWatchingRegistry();

	void ApplyChanges();
	void RestoreToInitial();
	void ClearDirtyState();

	bool HaveSettingsBeenChanged() const { return bSettingsChanged; }

private:
	void RebuildDirtyState();

	TWeakObjectPtr<USettingRegistry> Registry;
	TArray<TWeakObjectPtr<USettingViewModelBase>> DirtyViewModels;
	bool bSettingsChanged = false;
};
