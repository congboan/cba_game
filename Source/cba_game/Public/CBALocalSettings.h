#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "CBALocalSettings.generated.h"

UCLASS()
class CBA_GAME_API UCBALocalSettings : public UGameUserSettings
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "CBA")
    static UCBALocalSettings* Get();
};
