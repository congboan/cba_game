#include "ViewModels/SettingActionViewModel.h"

void USettingActionViewModel::ExecuteAction()
{
	OnActionExecuted.Broadcast();
}
