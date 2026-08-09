#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "SettingsFrameworkTypes.generated.h"

UENUM(BlueprintType)
enum class ESettingValueType : uint8
{
	Scalar,
	Bool,
	Enum,
	Action,
	Page,
	Group,
	Color,
	Vector2D
};

UENUM(BlueprintType, Flags)
enum class ESettingEditableState : uint8
{
	None = 0,
	Visible = 1 << 0,
	Enabled = 1 << 1,
	Resetable = 1 << 2,
	All = Visible | Enabled | Resetable
};

USTRUCT(BlueprintType)
struct FSettingScalarRange
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Min = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Max = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Step = 1.0f;
};

USTRUCT(BlueprintType)
struct FSettingDisplayFormat
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString FormatString;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Multiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct FSettingOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Value;
};

/** 绑定路径：点分/函数链路径（如 "MasterVolume" 或 "GetLocalSettings.MasterVolume"）。编辑器提供属性选择器。 */
USTRUCT(BlueprintType)
struct FSettingBindingPath
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Settings")
	FString Path;

	bool IsEmpty() const { return Path.IsEmpty(); }
	operator const FString&() const { return Path; }
};
