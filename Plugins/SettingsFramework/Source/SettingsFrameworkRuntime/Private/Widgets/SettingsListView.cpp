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

void USettingsListView::OnItemClickedInternal(UObject* Item)
{
	// 只对可点选条目（Page）广播导航；分组（Group）点击不响应
	if (USettingViewModelBase* VM = Cast<USettingViewModelBase>(Item))
	{
		if (VM->IsSelectable())
		{
			OnEntryActivated.Broadcast(VM);
			return;
		}
	}
	Super::OnItemClickedInternal(Item);
}
