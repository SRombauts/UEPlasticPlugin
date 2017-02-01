// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class XmlParser2 : ModuleRules
{
	public XmlParser2(ReadOnlyTargetRules Target) : base(Target)
	{
		// Do not enforce "Include What You Use" UE4.15 policy
		// since it does not follow the same rules for In-Engine Plugins as for Game Project Plugins,
		// and as such prevents us to make a source code compiling as both.
		bEnforceIWYU = false;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			}
		);
	}
}
