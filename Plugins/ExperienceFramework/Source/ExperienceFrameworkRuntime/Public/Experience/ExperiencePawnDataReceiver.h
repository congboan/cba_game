#pragma once

#include "UObject/Interface.h"
#include "ExperiencePawnDataReceiver.generated.h"

class UExperiencePawnData;

UINTERFACE(BlueprintType)
class EXPERIENCEFRAMEWORKRUNTIME_API UExperiencePawnDataReceiver : public UInterface
{
	GENERATED_BODY()
};

class EXPERIENCEFRAMEWORKRUNTIME_API IExperiencePawnDataReceiver
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Experience")
	void SetExperiencePawnData(const UExperiencePawnData* PawnData);
};
