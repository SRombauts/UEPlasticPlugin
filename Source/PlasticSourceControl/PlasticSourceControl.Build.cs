// Copyright (c) 2016-2019 Codice Software - Sebastien Rombauts (sebastien.rombauts@gmail.com)

using UnrealBuildTool;

public class PlasticSourceControl : ModuleRules
{
	public PlasticSourceControl(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
				"InputCore",
				"EditorStyle",
				"UnrealEd",
				"LevelEditor",
				"SourceControl",
				"XmlParser2",
				"Projects",
				"AssetRegistry",
			}
		);
	}
}
