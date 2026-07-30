using UnrealBuildTool;

public class InventoryFrameworkRuntime : ModuleRules
{
	public InventoryFrameworkRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"NetCore",
				"GameplayTagStack",
				"GameplayAbilities",
				"GameplayTasks",
				"ModularGameplay",
				"IrisCore",
				"DataRegistry",
				"StructUtils"
			}
		);
	}
}
