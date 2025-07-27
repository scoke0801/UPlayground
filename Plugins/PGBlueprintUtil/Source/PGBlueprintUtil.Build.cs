using UnrealBuildTool;

public class PGBlueprintUtil : ModuleRules
{
	public PGBlueprintUtil(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Slate",
				"SlateCore"
			}
		);

		// 에디터 전용 의존성
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"ToolMenus",
					"EditorStyle",
					"EditorWidgets",
					"AssetRegistry",
					"ContentBrowser",
					"BlueprintGraph",
					"KismetCompiler",
					"ClassViewer",
					"PropertyEditor",
					"DesktopPlatform",
					"Json",
					"JsonUtilities",
					"LevelEditor",
					"WorkspaceMenuStructure",
					"ApplicationCore",
					"InputCore"
				}
			);
		}
	}
}
