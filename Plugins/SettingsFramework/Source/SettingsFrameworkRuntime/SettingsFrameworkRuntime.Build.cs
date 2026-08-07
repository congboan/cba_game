// Copyright cba_game. All Rights Reserved.

using UnrealBuildTool;

public class SettingsFrameworkRuntime : ModuleRules
{
	public SettingsFrameworkRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"ModelViewViewModel",
			"UMG",
			"CommonUI",
			"CommonGame",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"InputCore",
		});
	}
}
