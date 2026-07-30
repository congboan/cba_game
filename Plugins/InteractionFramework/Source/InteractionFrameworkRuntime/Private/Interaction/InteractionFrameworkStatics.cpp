#include "Interaction/InteractionFrameworkStatics.h"

#include "Engine/HitResult.h"
#include "GameFramework/Actor.h"
#include "Interaction/InteractableTarget.h"

void UInteractionFrameworkStatics::AppendInteractionOptionsFromHitResults(const FInteractionFrameworkQuery& Query, const TArray<FHitResult>& HitResults, TArray<FInteractionFrameworkOption>& OutOptions)
{
	TSet<TWeakObjectPtr<AActor>> VisitedActors;

	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (HitActor == nullptr || VisitedActors.Contains(HitActor))
		{
			continue;
		}

		VisitedActors.Add(HitActor);
		AppendInteractionOptionsFromActor(Query, HitActor, OutOptions);
	}
}

void UInteractionFrameworkStatics::AppendInteractionOptionsFromActor(const FInteractionFrameworkQuery& Query, AActor* TargetActor, TArray<FInteractionFrameworkOption>& OutOptions)
{
	if (TargetActor == nullptr || !TargetActor->GetClass()->ImplementsInterface(UInteractionFrameworkTarget::StaticClass()))
	{
		return;
	}

	const int32 PreviousCount = OutOptions.Num();
	IInteractionFrameworkTarget::Execute_GatherInteractionOptions(TargetActor, Query, OutOptions);

	for (int32 Index = PreviousCount; Index < OutOptions.Num(); ++Index)
	{
		if (OutOptions[Index].TargetActor == nullptr)
		{
			OutOptions[Index].TargetActor = TargetActor;
		}
	}
}
