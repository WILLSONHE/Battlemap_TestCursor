// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class Battlemap_TestCursorEditorTarget : TargetRules
{
	public Battlemap_TestCursorEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		// Avoid git-status adaptive working set + extra SourceFileCache.bin writers (reduces UBT file locks).
		bUseAdaptiveUnityBuild = false;
		ExtraModuleNames.Add("Battlemap_TestCursor");
	}
}
