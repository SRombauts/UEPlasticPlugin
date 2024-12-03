// Copyright (c) 2024 Unity Technologies

using UnrealBuildTool;

public class PlasticSourceControl : ModuleRules
{
	public PlasticSourceControl(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Note: from UE5.2 onward, bEnforceIWYU = true; is replaced by IWYUSupport = IWYUSupport.Full;
		bEnforceIWYU = true;
		// IWYUSupport = IWYUSupport.Full;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"EditorStyle",
				"UnrealEd",
				"LevelEditor",
				"DesktopPlatform",
				"SourceControl",
				"SourceControlWindows",
				"XmlParser",
				"Projects",
				"AssetRegistry",
				"DeveloperSettings",
				"ToolMenus",
				"ContentBrowser",
			}
		);

		// NOTE: this produces warnings in Engine code in UE5.0
		UnsafeTypeCastWarningLevel = WarningLevel.Warning;
	}
}
