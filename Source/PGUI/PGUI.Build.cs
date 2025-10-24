// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PGUI : ModuleRules
{
	public PGUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "PGShared", "PGData", "Paper2D", "PGActor", "PGMessage" });
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// C++ 20 사용 설정
		CppStandard = CppStandardVersion.Cpp20;

		// 모듈 헤더 공개 설정
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"UMG", // UI 위젯 시스템 (UUserWidget 등)
				"Slate", // 기본 Slate UI 시스템
				"SlateCore", // Slate의 핵심 기능
				"Niagara"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"PGMessage", 
			}
		);
	}
}
