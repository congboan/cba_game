#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SettingsFrameworkTypes.h"
#include "SettingEntry.generated.h"

class USettingCollection;

UCLASS(BlueprintType, EditInlineNew)
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

	/** 宿主绑定路径（PropertyPath，支持函数链，如 GetLocalSettings.MasterVolume）。编辑器提供属性选择器。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FSettingBindingPath BindingPath;

	/** 默认值字符串（Scalar/Bool/Enum 的 ResetToDefault 使用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString DefaultValue;

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

	/** ValueType=Page 时：该页包含的子页集合（可嵌套）。 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly)
	TArray<USettingCollection*> ChildPages;
};

// END
