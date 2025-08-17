// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PGMessage : ModuleRules
{
	public PGMessage(ReadOnlyTargetRules Target) : base(Target)
	{
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
			}
		);
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"PGShared"
			});
	}
}
