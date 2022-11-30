// Copyright 1998-2016 Epic Games, Inc

using UnrealBuildTool;

public class XmlParser2 : ModuleRules
{
	public XmlParser2(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			}
		);
	}
}
