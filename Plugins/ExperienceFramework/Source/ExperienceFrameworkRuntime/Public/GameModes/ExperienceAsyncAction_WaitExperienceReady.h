#pragma once

#include "Kismet/BlueprintAsyncActionBase.h"
#include "ExperienceAsyncAction_WaitExperienceReady.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FExperienceWaitReadyAsyncDelegate);

UCLASS()
class EXPERIENCEFRAMEWORKRUNTIME_API UExperienceAsyncAction_WaitExperienceReady : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", BlueprintInternalUseOnly = "true"))
	static UExperienceAsyncAction_WaitExperienceReady* WaitForExperienceReady(UObject* WorldContextObject);

	virtual void Activate() override;

	UPROPERTY(BlueprintAssignable)
	FExperienceWaitReadyAsyncDelegate OnReady;

private:
	void Step();

	UPROPERTY()
	TObjectPtr<UObject> WorldContextObject;
};
