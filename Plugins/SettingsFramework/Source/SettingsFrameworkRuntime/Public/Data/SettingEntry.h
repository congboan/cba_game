#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SettingsFrameworkTypes.h"
#include "SettingEntry.generated.h"

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

	/** 依赖的设置 DevName：依赖项值变化时刷新本项可编辑状态（对应 Lyra AddEditDependency）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FName> EditDependencyDevNames;

	/** 值依赖条件：依赖项当前值匹配时 Disable/Hide（对应 Lyra FWhenCondition 闭包）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FSettingValueCondition> ValueConditions;

	/** 容器节点（Page/Group）的子设置：递归 Entry 树，任意层级嵌套。 */
	UPROPERTY(EditAnywhere, Instanced, BlueprintReadOnly)
	TArray<TObjectPtr<USettingEntry>> Children;
};

// END
