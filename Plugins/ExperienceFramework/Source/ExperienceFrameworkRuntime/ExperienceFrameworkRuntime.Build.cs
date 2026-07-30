using UnrealBuildTool;

public class ExperienceFrameworkRuntime : ModuleRules
{
	public ExperienceFrameworkRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"EnhancedInput",
				"GameFeatures",
				"GameplayAbilities",
				"GameplayTasks",
				"GameplayTags",
				"ModularGameplay",
				"ModularGameplayActors",
				"CommonUI",
				"UIExtension",
				"UMG"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"DeveloperSettings",
				"Projects", "CommonGame"
			}
		);
	}
}
