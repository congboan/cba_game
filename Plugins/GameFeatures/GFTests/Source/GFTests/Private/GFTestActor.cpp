#include "GFTestActor.h"

AGFTestActor::AGFTestActor()
{
    PrimaryActorTick.bCanEverTick = false;
}

void AGFTestActor::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogTemp, Warning, TEXT("[GFTests] Actor: %s"), *TestMessage);
}
