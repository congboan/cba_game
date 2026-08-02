// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class cba_gameTarget : TargetRules
{
	public cba_gameTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V8;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_9;
		ExtraModuleNames.Add("cba_game");
	}
}
