#include "Widgets/SettingsScreenWidget.h"
#include "Widgets/SettingsListView.h"
#include "ViewModels/SettingPageViewModel.h"

void USettingsScreenWidget::SetupRegistry(USettingRegistry* InRegistry)
{
	Registry = InRegistry;
	RefreshDisplay();
}

void USettingsScreenWidget::NavigateToPage(USettingViewModelBase* PageVM)
{
	if (Registry) Registry->NavigateToPage(PageVM);
	RefreshDisplay();
}

void USettingsScreenWidget::NavigateBack()
{
	if (Registry && Registry->NavigateBack())
		RefreshDisplay();
}

void USettingsScreenWidget::SaveAndClose()
{
	if (Registry) Registry->SaveChanges();
	DeactivateWidget();
}

void USettingsScreenWidget::RefreshDisplay()
{
	if (SettingsList && Registry)
	{
		TArray<USettingViewModelBase*> Items;
		if (auto* Page = Registry->GetCurrentPage())
		{
			Items = Cast<USettingPageViewModel>(Page)->ChildViewModels;
		}
		else
		{
			Items = Registry->GetRootViewModels();
		}
		SettingsList->SetItems(Items);
	}
}
