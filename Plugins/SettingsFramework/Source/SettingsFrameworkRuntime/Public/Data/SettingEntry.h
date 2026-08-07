#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SettingsFrameworkTypes.h"
#include "SettingEntry.generated.h"

UCLASS(BlueprintType)
class SETTINGSFRAMEWORKRUNTIME_API USettingEntry : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName DevName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ESettingValueType ValueType = ESettingValueType::Scalar;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString BindingPath;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FSettingScalarRange ScalarRange;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FSettingDisplayFormat DisplayFormat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FSettingOption> Options;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTagContainer PlatformTraits;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag EditConditionTag;
};

// END
