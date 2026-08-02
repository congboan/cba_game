#include "Inventory/InventoryFragment_Stacking.h"
#include "Inventory/InventoryItemDef.h"

int32 FInventoryFragment_Stacking::GetStackSizeLimit(const FInventoryItemDef* Def) const
{
	if (MaxStackSizeOverride > 0)
	{
		return MaxStackSizeOverride;
	}
	return Def != nullptr ? FMath::Max(0, Def->MaxStackSize) : 1;
}
