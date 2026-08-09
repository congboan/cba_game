// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SettingsFrameworkEditor : ModuleRules
{
	public SettingsFrameworkEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"SettingsFrameworkRuntime",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UnrealEd",
			"PropertyEditor",
			"InputCore",
		});
	}
}
