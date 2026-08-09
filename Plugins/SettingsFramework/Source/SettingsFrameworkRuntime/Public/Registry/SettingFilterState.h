#pragma once

#include "CoreMinimal.h"

class USettingViewModelBase;

/** 设置过滤状态（对应 Lyra FGameSettingFilterState）。
 *  支持搜索文本 + AllowList + 显隐/禁用包含开关。 */
struct FSettingFilterState
{
	bool bIncludeDisabled = true;
	bool bIncludeHidden = false;
	bool bIncludeResetable = true;
	bool bIncludeNestedPages = false;

	TArray<USettingViewModelBase*> SettingAllowList;

	void SetSearchText(const FString& InSearchText) { SearchText = InSearchText; }
	const FString& GetSearchText() const { return SearchText; }

	void AddSettingToAllowList(USettingViewModelBase* InSetting)
	{
		SettingAllowList.AddUnique(InSetting);
	}

	bool DoesSettingPassFilter(const USettingViewModelBase* InSetting) const;

private:
	FString SearchText;
};
