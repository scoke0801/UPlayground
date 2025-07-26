// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PGData : ModuleRules
{
	public PGData(ReadOnlyTargetRules Target) : base(Target)
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
				"GameplayTags",
				"GameplayAbilities",
				"GameplayTasks",
				"EnhancedInput",
				"PGAbilitySystem"
			}
		);
		

	}
}
