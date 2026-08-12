#include "GFTestsModule.h"
#include "HAL/IConsoleManager.h"

void FGFTestsModule::StartupModule()
{
    IConsoleManager::Get().RegisterConsoleCommand(
        TEXT("GFTests.Hello"),
        TEXT("Print a test message"),
        FConsoleCommandDelegate::CreateLambda([]()
        {
            UE_LOG(LogTemp, Warning, TEXT("[GFTests] Hello"));
        }));
    UE_LOG(LogTemp, Warning, TEXT("[GFTests] Module started"));
}

void FGFTestsModule::ShutdownModule()
{
    UE_LOG(LogTemp, Warning, TEXT("[GFTests] Module shut down"));
}

IMPLEMENT_MODULE(FGFTestsModule, GFTests)
