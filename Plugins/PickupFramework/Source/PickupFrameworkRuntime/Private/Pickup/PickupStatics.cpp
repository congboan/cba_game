#include "Pickup/PickupStatics.h"

#include "Pickup/Pickupable.h"

bool UPickupStatics::TryPickupActor(AActor* PickupActor, UInventoryManagerComponent* InventoryManager)
{
	if (PickupActor == nullptr || !PickupActor->GetClass()->ImplementsInterface(UPickupFrameworkPickupable::StaticClass()))
	{
		return false;
	}

	return IPickupFrameworkPickupable::Execute_TryGrantPickupToInventory(PickupActor, InventoryManager);
}
