// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Battlemap_TestCursor : ModuleRules
{
	public Battlemap_TestCursor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "NavigationSystem", "AIModule", "Niagara", "EnhancedInput", "Json", "JsonUtilities", "ImageWrapper", "DeveloperSettings" });
    }
}
