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
