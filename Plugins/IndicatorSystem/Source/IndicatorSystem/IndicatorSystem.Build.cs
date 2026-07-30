using UnrealBuildTool;

public class IndicatorSystem : ModuleRules
{
    public IndicatorSystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "UMG",
                "AsyncMixin",
                "ModularGameplay"
            }
        );
    }
}
