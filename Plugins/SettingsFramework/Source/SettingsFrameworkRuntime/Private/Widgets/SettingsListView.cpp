#include "Widgets/SettingsListView.h"
#include "ViewModels/SettingViewModelBase.h"

void USettingsListView::SetItems(const TArray<USettingViewModelBase*>& Items)
{
	ClearListItems();
	for (USettingViewModelBase* VM : Items)
	{
		if (VM) AddItem(VM);
	}
	RegenerateAllEntries();
}
