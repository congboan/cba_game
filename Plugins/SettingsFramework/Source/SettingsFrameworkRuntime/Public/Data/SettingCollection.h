#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SettingEntry.h"
#include "SettingCollection.generated.h"

UCLASS(BlueprintType)
class SETTINGSFRAMEWORKRUNTIME_API USettingCollection : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText CollectionName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<USettingEntry*> Entries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag AssetBundleTag;
};
