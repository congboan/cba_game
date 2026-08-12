#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GFTestActor.generated.h"

UCLASS()
class GFTESTS_API AGFTestActor : public AActor
{
    GENERATED_BODY()

public:
    AGFTestActor();

    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "GFTests")
    FString TestMessage = TEXT("Hello from GFTests");
};

