#include "GFTestGameFeatureAction.h"

void UGFTestGameFeatureAction::OnGameFeatureActivating(FGameFeatureActivatingContext& Context)
{
    UE_LOG(LogTemp, Warning, TEXT("[GFTests] GF activated"));
}

void UGFTestGameFeatureAction::OnGameFeatureDeactivating(FGameFeatureDeactivatingContext& Context)
{
    UE_LOG(LogTemp, Warning, TEXT("[GFTests] GF deactivated"));
}
