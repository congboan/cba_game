using UnrealBuildTool;

public class GameplayTagStack : ModuleRules
{
    public GameplayTagStack(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "GameplayTags",
                "NetCore"
            }
        );
    }
}
