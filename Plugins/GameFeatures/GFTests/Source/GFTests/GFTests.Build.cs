using UnrealBuildTool;

public class GFTests : ModuleRules
{
    public GFTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "GameFeatures" });
        PublicDependencyModuleNames.AddRange(new string[] { "CommonGame", "CommonUI", "UMG" });
    }
}
