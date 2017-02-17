// Copyright (c) 2016 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

using UnrealBuildTool;

public class PlasticSourceControl : ModuleRules
{
	public PlasticSourceControl(TargetInfo Target)
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
				"Slate",
				"SlateCore",
				"EditorStyle",
				"UnrealEd",
				"LevelEditor",
				"SourceControl",
				"InputCore",
				"XmlParser2",
				"Projects",
				"AssetRegistry",
			}
		);
	}
}
