#include "Inventory/InventoryFragment_Stats.h"
#include "Inventory/InventoryItemInstance.h"

void FInventoryFragment_Stats::OnInstanceCreated(UInventoryItemInstance* Instance) const
{
	if (Instance == nullptr)
	{
		return;
	}

	for (const TPair<FGameplayTag, int32>& Pair : InitialStats)
	{
		if (Pair.Key.IsValid())
		{
			Instance->AddStatTagStack(Pair.Key, Pair.Value);
		}
	}
}
