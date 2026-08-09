#include "Registry/SettingFilterState.h"
#include "ViewModels/SettingViewModelBase.h"

bool FSettingFilterState::DoesSettingPassFilter(const USettingViewModelBase* InSetting) const
{
	if (!InSetting) return false;

	if (!SettingAllowList.IsEmpty() && !SettingAllowList.Contains(InSetting))
	{
		return false;
	}

	const int32 Flags = InSetting->GetEditableStateFlags();
	const bool bVisible = (Flags & 1) != 0;
	const bool bEnabled = (Flags & 2) != 0;
	const bool bResetable = (Flags & 4) != 0;

	if (!bIncludeHidden && !bVisible) return false;
	if (!bIncludeDisabled && !bEnabled) return false;
	if (!bIncludeResetable && !bResetable) return false;

	if (!SearchText.IsEmpty())
	{
		const FString Name = InSetting->GetEntry()
			? InSetting->GetEntry()->DisplayName.ToString()
			: FString();
		const FString DevName = InSetting->GetEntry()
			? InSetting->GetEntry()->DevName.ToString()
			: FString();
		if (!Name.Contains(SearchText) && !DevName.Contains(SearchText))
		{
			return false;
		}
	}

	return true;
}
