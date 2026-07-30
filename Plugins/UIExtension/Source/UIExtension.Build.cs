// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UIExtension : ModuleRules
{
	public UIExtension(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"SlateCore",
				"Slate",
				"UMG",
				"GameplayTags",
				"CommonUI"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CommonGame"
			}
		);
	}
}
