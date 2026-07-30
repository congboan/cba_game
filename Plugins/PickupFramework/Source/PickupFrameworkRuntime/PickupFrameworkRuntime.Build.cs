using UnrealBuildTool;

public class PickupFrameworkRuntime : ModuleRules
{
	public PickupFrameworkRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"InteractionFrameworkRuntime",
				"InventoryFrameworkRuntime",
				"DataRegistry"
			}
		);
	}
}
